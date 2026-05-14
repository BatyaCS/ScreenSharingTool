#include <encoder/stream-encoder.h>
#include <iostream>

bool StreamEncoder::init(const EncoderConfig& config)
{
    _config = config;

    // 1. Find the libx264 encoder
    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec)
    {
        std::cerr << "libx264 codec not found! Check vcpkg.json features.\n";
        return false;
    }

    _codec_context = avcodec_alloc_context3(codec);
    if (!_codec_context)
        return false;

    // 2. Setup codec parameters
    _codec_context->width = config.width;
    _codec_context->height = config.height;
    _codec_context->time_base = { 1, static_cast<int>(config.fps) };
    _codec_context->framerate = { static_cast<int>(config.fps), 1 };
    _codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // Set bitrate
    _codec_context->bit_rate = config.bitrate_kbps * 1000;
    
    // Optional: ultrafast preset for real-time streaming to minimize delay
    av_opt_set(_codec_context->priv_data, "preset", "ultrafast", 0);
    av_opt_set(_codec_context->priv_data, "tune", "zerolatency", 0);

    // 3. Open the codec
    if (avcodec_open2(_codec_context, codec, nullptr) < 0)
    {
        std::cerr << "Could not open codec.\n";
        return false;
    }

    // 4. Allocate a frame for YUV420P data
    _frame = av_frame_alloc();
    _frame->format = _codec_context->pix_fmt;
    _frame->width  = _codec_context->width;
    _frame->height = _codec_context->height;
    av_frame_get_buffer(_frame, 0);

    // 5. Allocate a packet for encoded output
    _packet = av_packet_alloc();

    // 6. Setup SwsContext for converting OpenCV BGRA to FFmpeg YUV420P
    _sws_context = sws_getContext(
        config.width, config.height, AV_PIX_FMT_BGRA,
        config.width, config.height, AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
    );

    _frame_pts = 0;
    return true;
}

void StreamEncoder::release()
{
    if (_sws_context)
    {
        sws_freeContext(_sws_context);
        _sws_context = nullptr;
    }

    if (_frame)
        av_frame_free(&_frame);

    if (_packet)
        av_packet_free(&_packet);

    if (_codec_context)
        avcodec_free_context(&_codec_context);
}

std::vector<uint8_t> StreamEncoder::encode_frame(const cv::Mat& frame)
{
    std::vector<uint8_t> encoded_data;

    if (frame.empty() || !_codec_context || !_sws_context)
        return encoded_data;

    // 1. Ensure the frame is continuous in memory
    cv::Mat continuous_frame;
    if (frame.isContinuous())
        continuous_frame = frame;
    else
        frame.copyTo(continuous_frame);

    // 2. Convert BGRA to YUV420P
    const uint8_t* in_data[1] = { continuous_frame.data };
    int in_linesize[1] = { static_cast<int>(continuous_frame.step[0]) };

    sws_scale(_sws_context, in_data, in_linesize, 0, _config.height, _frame->data, _frame->linesize);

    // Set Presentation Time Stamp (PTS)
    _frame->pts = _frame_pts++;

    // 3. Send the raw frame to the encoder
    int ret = avcodec_send_frame(_codec_context, _frame);
    if (ret < 0)
        return encoded_data;

    // 4. Receive the encoded packet(s)
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(_codec_context, _packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break; 
        else if (ret < 0)
        {
            std::cerr << "Error during encoding!\n";
            break;
        }

        // 5. Append encoded data
        encoded_data.insert(encoded_data.end(), _packet->data, _packet->data + _packet->size);
        
        av_packet_unref(_packet);
    }

    return encoded_data;
}