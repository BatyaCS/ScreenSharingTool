#include <common.h>
#include <decoder/hw-stream-decoder.h>

#define DECODER_ABORT(...) \
    do { \
        LOG_ERROR(__VA_ARGS__); \
        _has_error.store(true); \
        _is_running.store(false); \
        return; \
    } while(0)

bool HwStreamDecoder::init(ID3D11Device* device, VideoFrameCallback video_cb, AudioFrameCallback audio_cb)
{
    if (!device || !video_cb || !audio_cb)
    {
        LOG_ERROR("Invalid device or callbacks provided to Decoder!\n");
        return false;
    }

    _d3d11_device = device;
    _d3d11_device->AddRef();
    
    _video_callback = video_cb;
    _audio_callback = audio_cb;
    _is_running = true;

    _decode_thread = std::thread(&HwStreamDecoder::decode_loop, this);

    return true;
}

void HwStreamDecoder::release()
{
    _is_running = false;
    _buffer_cv.notify_all();

    if (_decode_thread.joinable())
        _decode_thread.join();

    if (_packet) 
    { 
        av_packet_free(&_packet); 
        _packet = nullptr;
    }

    if (_frame) 
    { 
        av_frame_free(&_frame); 
        _frame = nullptr;
    }

    if (_format_context) 
    {
        if (_avio_context) 
        {
            av_freep(&_avio_context->buffer);
            avio_context_free(&_avio_context);
        }

        avformat_close_input(&_format_context);
    }

    if (_video_codec_context) { avcodec_free_context(&_video_codec_context); }
    if (_audio_codec_context) { avcodec_free_context(&_audio_codec_context); }
    if (_hw_device_ctx)       { av_buffer_unref(&_hw_device_ctx); }
    
    if (_d3d11_device) 
    { 
        _d3d11_device->Release(); 
        _d3d11_device = nullptr; 
    }

    _stream_buffer.clear();
    _has_error.store(false);

    _video_stream_idx = -1;
    _audio_stream_idx = -1;
}

void HwStreamDecoder::push_data(const std::vector<uint8_t>& data)
{
    if (data.empty()) 
        return;

    std::lock_guard<std::mutex> lock(_buffer_mutex);
    _stream_buffer.insert(_stream_buffer.end(), data.begin(), data.end());
    _buffer_cv.notify_one();
}

int HwStreamDecoder::read_data(uint8_t * buf, int buf_size)
{
    std::unique_lock<std::mutex> lock(_buffer_mutex);
    _buffer_cv.wait(lock, [this]() { return !_stream_buffer.empty() || !_is_running.load(); });

    if (!_is_running.load() && _stream_buffer.empty())
        return AVERROR_EOF;

    const int to_copy = std::min(buf_size, static_cast<int>(_stream_buffer.size()));
    std::memcpy(buf, _stream_buffer.data(), to_copy);
    _stream_buffer.erase(_stream_buffer.begin(), _stream_buffer.begin() + to_copy);
    
    return to_copy;
}

void HwStreamDecoder::decode_loop()
{
    // Allocate packet and frame once (they will be freed in release())
    _packet = av_packet_alloc();
    _frame = av_frame_alloc();
    if (!_packet || !_frame)
        DECODER_ABORT("Failed to allocate AV packet or frame!\n");

    _avio_buffer = static_cast<uint8_t*>(av_malloc(static_cast<int>(AVIO_CTX_BUFFER_SIZE)));
    if (!_avio_buffer)
        DECODER_ABORT("Failed to allocate AVIO buffer!\n");
    
    _avio_context = avio_alloc_context(_avio_buffer, static_cast<int>(AVIO_CTX_BUFFER_SIZE), 0, this, &HwStreamDecoder::avio_read_packet, nullptr, nullptr);
    if (!_avio_context)
        DECODER_ABORT("Failed to allocate AVIO context!\n");

    _format_context = avformat_alloc_context();
    if (!_format_context)
        DECODER_ABORT("Failed to allocate format context!\n");

    _format_context->pb = _avio_context;
    _format_context->flags |= AVFMT_FLAG_CUSTOM_IO;

    int ret = avformat_open_input(&_format_context, nullptr, nullptr, nullptr);
    if (ret < 0) 
        DECODER_ABORT("Failed to open input stream: %s!\n", get_av_error_string(ret).c_str());

    ret = avformat_find_stream_info(_format_context, nullptr);
    if (ret < 0) 
        DECODER_ABORT("Failed to find stream info: %s!\n", get_av_error_string(ret).c_str());

    // 1. Find both Video and Audio streams in the container
    _video_stream_idx = -1;
    _audio_stream_idx = -1;
    for (uint i = 0; i < _format_context->nb_streams; i++) 
    {
        if (_format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && _video_stream_idx == -1) 
            _video_stream_idx = i;
        else if (_format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && _audio_stream_idx == -1)
            _audio_stream_idx = i;
    }

    if (_video_stream_idx == -1 && _audio_stream_idx == -1)
        DECODER_ABORT("No video or audio streams found in input!\n");

    // 2. Initialize Video Decoder (Hardware D3D11VA)
    if (_video_stream_idx != -1)
    {
        const AVCodec* video_codec = avcodec_find_decoder(_format_context->streams[_video_stream_idx]->codecpar->codec_id);
        if (!video_codec)
            DECODER_ABORT("Failed to find video decoder!\n");

        _video_codec_context = avcodec_alloc_context3(video_codec);
        if (!_video_codec_context)
            DECODER_ABORT("Failed to allocate video codec context!\n");

        ret = avcodec_parameters_to_context(_video_codec_context, _format_context->streams[_video_stream_idx]->codecpar);
        if (ret < 0)
            DECODER_ABORT("Failed to copy video codec parameters: %s!\n", get_av_error_string(ret).c_str());

        _hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        if (!_hw_device_ctx)
            DECODER_ABORT("Failed to allocate HW device context!\n");

        AVHWDeviceContext* device_ctx = reinterpret_cast<AVHWDeviceContext*>(_hw_device_ctx->data);
        AVD3D11VADeviceContext* d3d11_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
        d3d11_ctx->device = _d3d11_device;
        
        ret = av_hwdevice_ctx_init(_hw_device_ctx);
        if (ret < 0)
            DECODER_ABORT("Failed to init HW device context: %s!\n", get_av_error_string(ret).c_str());

        _video_codec_context->hw_device_ctx = av_buffer_ref(_hw_device_ctx);
        _video_codec_context->get_format = get_hw_format;

        ret = avcodec_open2(_video_codec_context, video_codec, nullptr);
        if (ret < 0)
            DECODER_ABORT("Failed to open video codec: %s!\n", get_av_error_string(ret).c_str());
    }

    // 3. Initialize Audio Decoder (Software - AAC/Opus)
    if (_audio_stream_idx != -1)
    {
        const AVCodec* audio_codec = avcodec_find_decoder(_format_context->streams[_audio_stream_idx]->codecpar->codec_id);
        if (!audio_codec)
            DECODER_ABORT("Failed to find audio decoder!\n");
        
        _audio_codec_context = avcodec_alloc_context3(audio_codec);
        if (!_audio_codec_context)
            DECODER_ABORT("Failed to allocate audio codec context!\n");

        ret = avcodec_parameters_to_context(_audio_codec_context, _format_context->streams[_audio_stream_idx]->codecpar);
        if (ret < 0)
            DECODER_ABORT("Failed to copy audio codec parameters: %s!\n", get_av_error_string(ret).c_str());
        
        ret = avcodec_open2(_audio_codec_context, audio_codec, nullptr);
        if (ret < 0)
            DECODER_ABORT("Failed to open Audio Codec context: %s!\n", get_av_error_string(ret).c_str());
    }

    // 4. Main packet reading and decoding loop
    while (_is_running.load())
    {
        ret = av_read_frame(_format_context, _packet);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
                DECODER_ABORT("End of stream reached (Network disconnected)!\n");
            else
                DECODER_ABORT("Failed to read frame from input: %s!\n", get_av_error_string(ret).c_str());
        }

        // VIDEO Packet
        if (_packet->stream_index == _video_stream_idx && _video_codec_context)
        {
            ret = avcodec_send_packet(_video_codec_context, _packet);
            if (ret < 0)
                DECODER_ABORT("Error sending video packet for decoding: %s!\n", get_av_error_string(ret).c_str());
            
            while (true)
            {
                ret = avcodec_receive_frame(_video_codec_context, _frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
                    break;
                else if (ret < 0) 
                    DECODER_ABORT("Error during video decoding: %s!\n", get_av_error_string(ret).c_str());

                ID3D11Texture2D* tex = reinterpret_cast<ID3D11Texture2D*>(_frame->data[0]);
                if (_video_callback && tex)
                    _video_callback(tex, _d3d11_device);
                
                av_frame_unref(_frame);
            }
        }
        // AUDIO Packet
        else if (_packet->stream_index == _audio_stream_idx && _audio_codec_context)
        {
            ret = avcodec_send_packet(_audio_codec_context, _packet);
            if (ret < 0)
                DECODER_ABORT("Error sending audio packet for decoding: %s!\n", get_av_error_string(ret).c_str());
            
            while (true)
            {
                ret = avcodec_receive_frame(_audio_codec_context, _frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
                    break;
                else if (ret < 0) 
                    DECODER_ABORT("Error during audio decoding: %s!\n", get_av_error_string(ret).c_str());

                // Trigger audio callback and pass the decoded AVFrame containing PCM samples
                if (_audio_callback)
                    _audio_callback(_frame);
                
                av_frame_unref(_frame);
            }
        }
        
        av_packet_unref(_packet);
    }
}

/* static */
int HwStreamDecoder::avio_read_packet(void* opaque, uint8_t* buf, int buf_size)
{
    auto* decoder = static_cast<HwStreamDecoder*>(opaque);
    return decoder->read_data(buf, buf_size);
}

/* static */
AVPixelFormat HwStreamDecoder::get_hw_format(AVCodecContext* ctx, const AVPixelFormat* pix_fmts)
{
    for (const AVPixelFormat* p = pix_fmts; *p != -1; p++) 
        if (*p == AV_PIX_FMT_D3D11) 
            return *p;

    return AV_PIX_FMT_NONE;
}

/* static */ 
std::string HwStreamDecoder::get_av_error_string(int err_num)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err_num, err_buf, sizeof(err_buf));
    return std::string(err_buf);
}