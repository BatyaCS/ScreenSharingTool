// #define TRACE_ME

#include <common.h>
#include <encoder/hw-stream-encoder.h>

bool HwStreamEncoder::init(const EncoderConfig& config)
{
    _config = config;
    _frame_pts = 0;

    return true;
}

bool HwStreamEncoder::first_frame_init(ID3D11Device* device, ID3D11Texture2D* texture)
{
    if (nullptr == texture || nullptr == device)
    {
        LOG_ERROR("Received NULL D3D11 device or texture!\n");
        return false;
    }

    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    
    const uint width = desc.Width;
    const uint height = desc.Height;   
    if (0 == width || 0 == height)
    {
        LOG_ERROR("Incorrect frame received for first encoder initialization\n");
        return false;
    }

    _hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!_hw_device_ctx)
    {
        LOG_ERROR("Failed to allocate HW Device ctx\n");
        return false;
    }

    AVHWDeviceContext * device_ctx = reinterpret_cast<AVHWDeviceContext*>(_hw_device_ctx->data);
    AVD3D11VADeviceContext * d3d11_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
    
    d3d11_ctx->device = device;
    device->AddRef(); 

    const int av_hwdevice_ctx_init_err = av_hwdevice_ctx_init(_hw_device_ctx);
    if (av_hwdevice_ctx_init_err < 0)
    {
        LOG_ERROR("Failed to init HW Device ctx, error: %s\n", get_av_error_string(av_hwdevice_ctx_init_err).c_str());
        return false;
    }

    AVBufferRef* hw_frames_ref = av_hwframe_ctx_alloc(_hw_device_ctx);
    if (!hw_frames_ref) 
    {
        LOG_ERROR("Failed to allocate HW Frames ctx\n");
        return false;
    }

    AVHWFramesContext * frames_ctx = reinterpret_cast<AVHWFramesContext*>(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_D3D11; 
    frames_ctx->sw_format = AV_PIX_FMT_BGRA;  
    frames_ctx->width     = width;
    frames_ctx->height    = height;

    const int av_hwframe_ctx_init_err = av_hwframe_ctx_init(hw_frames_ref);
    if (av_hwframe_ctx_init_err < 0) 
    {
        LOG_ERROR("Failed to init HW Frames ctx, error: %s\n", get_av_error_string(av_hwframe_ctx_init_err).c_str());
        av_buffer_unref(&hw_frames_ref);
        return false;
    }

    const AVCodec * codec = avcodec_find_encoder_by_name(to_ffmpeg_codec(_config.codec).c_str());
    if (!codec) 
    {
        LOG_ERROR("HW Codec not found: %s\n", to_ffmpeg_codec(_config.codec).c_str());
        av_buffer_unref(&hw_frames_ref);
        return false;
    }

    _codec_context = avcodec_alloc_context3(codec);
    if (!_codec_context) 
    {
        LOG_ERROR("Failed to allocate codec context\n");
        av_buffer_unref(&hw_frames_ref);
        return false;
    }

    _codec_context->width = width;
    _codec_context->height = height;
    _codec_context->time_base = { 1, static_cast<int>(_config.fps) };
    _codec_context->framerate = { static_cast<int>(_config.fps), 1 };
    
    _codec_context->pix_fmt = AV_PIX_FMT_D3D11; 
    _codec_context->hw_device_ctx = av_buffer_ref(_hw_device_ctx);
    _codec_context->hw_frames_ctx = av_buffer_ref(hw_frames_ref); 
    _codec_context->bit_rate = _config.bitrate_kbps * 1000;

    if (StreamCodec::H264_NVEC == _config.codec) 
    {
        av_opt_set(_codec_context->priv_data, "preset", "p1", 0);
        av_opt_set(_codec_context->priv_data, "tune", "ull", 0);
    }
    else if (StreamCodec::H264_AMF == _config.codec) 
    {
        av_opt_set(_codec_context->priv_data, "quality", "speed", 0); 
        av_opt_set(_codec_context->priv_data, "usage", "ultralowlatency", 0); 
    }
    else if (StreamCodec::H264_QSV == _config.codec) 
        av_opt_set(_codec_context->priv_data, "preset", "veryfast", 0);

    const int avcodec_open2_err = avcodec_open2(_codec_context, codec, nullptr);
    if (avcodec_open2_err < 0) 
    {
        LOG_ERROR("Failed to open codec, error: %s\n", get_av_error_string(avcodec_open2_err).c_str());
        av_buffer_unref(&hw_frames_ref);
        return false;
    }

    _hw_frame = av_frame_alloc();
    _packet = av_packet_alloc();
    if (!_hw_frame || !_packet)
    {
        LOG_ERROR("Failed to allocate HW frame or packet\n");
        av_buffer_unref(&hw_frames_ref);
        return false;
    }

    // Need to free temporary reference
    av_buffer_unref(&hw_frames_ref); 

    const int avformat_alloc_err = avformat_alloc_output_context2(&_format_context, nullptr, "mpegts", nullptr);
    if (avformat_alloc_err < 0) 
    {
        LOG_ERROR("Failed to allocate output format context, error: %s\n", get_av_error_string(avformat_alloc_err).c_str());
        return false;
    }

    _video_stream = avformat_new_stream(_format_context, nullptr);
    if (!_video_stream)
    {
        LOG_ERROR("Failed to create new video stream\n");
        return false;
    }

    const int param_copy_err = avcodec_parameters_from_context(_video_stream->codecpar, _codec_context);
    if (param_copy_err < 0)
    {
        LOG_ERROR("Failed to copy codec parameters, error: %s\n", get_av_error_string(param_copy_err).c_str());
        return false;
    }

    const int avio_ctx_buffer_size = 8192;
    _avio_buffer = static_cast<uint8_t*>(av_malloc(avio_ctx_buffer_size));
    if (!_avio_buffer)
    {
        LOG_ERROR("Failed to allocate avio buffer\n");
        return false;
    }

    _avio_context = avio_alloc_context(
        _avio_buffer, avio_ctx_buffer_size, 1, 
        &_muxed_data,
        nullptr, &HwStreamEncoder::avio_write_packet, nullptr
    );

    if (!_avio_context)
    {
        LOG_ERROR("Failed to allocate avio context\n");
        return false;
    }

    _format_context->pb = _avio_context;
    _format_context->flags |= AVFMT_FLAG_CUSTOM_IO;

    const int avformat_write_header_err = avformat_write_header(_format_context, nullptr);
    if (avformat_write_header_err < 0) 
    {
        LOG_ERROR("Failed to write format header, error: %s\n", get_av_error_string(avformat_write_header_err).c_str());
        return false;
    }

    _is_first_frame_received = true;
    return true;
}

void HwStreamEncoder::release()
{
    if (_format_context) 
    {
        if (_avio_context) 
        {
            av_freep(&_avio_context->buffer);
            avio_context_free(&_avio_context);
        }

        avformat_free_context(_format_context);
        _format_context = nullptr;
    }

    if (_hw_frame)      { av_frame_free(&_hw_frame); }
    if (_packet)        { av_packet_free(&_packet); }
    if (_codec_context) { avcodec_free_context(&_codec_context); }
    if (_hw_device_ctx) { av_buffer_unref(&_hw_device_ctx); }

    _is_first_frame_received = false;
}

bool HwStreamEncoder::encode_texture(ID3D11Texture2D* texture, ID3D11Device* device, std::vector<uint8_t>& data)
{
    if (!texture || !device)
        return false;

    _muxed_data.clear();
    if (!_is_first_frame_received) 
        if (!first_frame_init(device, texture))
            return false;

    const int av_hwframe_get_buf_err = av_hwframe_get_buffer(_codec_context->hw_frames_ctx, _hw_frame, 0);
    if (av_hwframe_get_buf_err < 0)
    {
        LOG_ERROR("Failed to get HW Frame buffer, error: %s\n", get_av_error_string(av_hwframe_get_buf_err).c_str());
        return false;
    }

    ID3D11Texture2D * ffmpeg_tex = reinterpret_cast<ID3D11Texture2D*>(_hw_frame->data[0]);
    ID3D11DeviceContext * context = nullptr;

    device->GetImmediateContext(&context);
    context->CopyResource(ffmpeg_tex, texture);
    context->Release();

    _hw_frame->pts = _frame_pts++;

    const int send_frame_err = avcodec_send_frame(_codec_context, _hw_frame);
    if (send_frame_err < 0) 
    {
        LOG_ERROR("Failed to send frame to codec: %s\n", get_av_error_string(av_hwframe_get_buf_err).c_str());
        
        av_frame_unref(_hw_frame);
        return false;
    }

    int receive_packet_res = 0;
    while (receive_packet_res >= 0)
    {
        receive_packet_res = avcodec_receive_packet(_codec_context, _packet);
        if (receive_packet_res == AVERROR(EAGAIN) || receive_packet_res == AVERROR_EOF) 
            break; 
        else if (receive_packet_res < 0) 
        {
            LOG_ERROR("Failed receive packet from avcodec: %s\n", get_av_error_string(receive_packet_res).c_str());
            break;
        }

        av_packet_rescale_ts(_packet, _codec_context->time_base, _video_stream->time_base);
        _packet->stream_index = _video_stream->index;

        av_interleaved_write_frame(_format_context, _packet);        
        avio_flush(_format_context->pb);

        av_packet_unref(_packet);
    }

    av_frame_unref(_hw_frame);

    data = _muxed_data;
    return true;
}

/* static */ 
std::string HwStreamEncoder::get_av_error_string(int err_num)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err_num, err_buf, sizeof(err_buf));
    return std::string(err_buf);
}

/* static */
int HwStreamEncoder::avio_write_packet(void * opaque, const uint8_t * buf, int buf_size)
{
    auto * muxed_buffer = static_cast<std::vector<uint8_t>*>(opaque);
    muxed_buffer->insert(muxed_buffer->end(), buf, buf + buf_size);
    return buf_size;
}

/* static */ 
std::string HwStreamEncoder::to_ffmpeg_codec(StreamCodec codec)
{
    switch (codec)
    {
        case StreamCodec::H264_NVEC:    return "h264_nvenc";
        case StreamCodec::H264_QSV:     return "h264_qsv";
        case StreamCodec::H264_AMF:     return "h264_amf";
    }

    return "Invalid codec";
}  