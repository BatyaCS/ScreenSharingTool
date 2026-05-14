#include <common.h>
#include <decoder/stream-decoder.h>
#include <iostream>

bool StreamDecoder::init()
{
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        std::cerr << "H.264 decoder not found!\n";
        return false;
    }

    _codec_context = avcodec_alloc_context3(codec);
    if (!_codec_context)
        return false;

    if (avcodec_open2(_codec_context, codec, nullptr) < 0)
    {
        std::cerr << "Could not open decoder.\n";
        return false;
    }

    _frame_yuv = av_frame_alloc();
    _frame_bgra = av_frame_alloc();
    _packet = av_packet_alloc();

    return true;
}

void StreamDecoder::release()
{
    if (_sws_context)
    {
        sws_freeContext(_sws_context);
        _sws_context = nullptr;
    }
    
    // Free the allocated memory for the BGRA frame buffer if it exists
    if (_frame_bgra && _frame_bgra->data[0])
        av_freep(&_frame_bgra->data[0]);

    if (_frame_yuv) av_frame_free(&_frame_yuv);
    if (_frame_bgra) av_frame_free(&_frame_bgra);
    if (_packet) av_packet_free(&_packet);
    if (_codec_context) avcodec_free_context(&_codec_context);
}

bool StreamDecoder::decode_h264_to_bgra(const std::vector<uint8_t>& packet_data, cv::Mat& out_frame)
{
    if (packet_data.empty() || !_codec_context)
        return false;

    // 1. Prepare the packet
    // Note: We cast away const here because FFmpeg's API requires it, 
    // but avcodec_send_packet does not modify the data.
    _packet->data = const_cast<uint8_t*>(packet_data.data());
    _packet->size = static_cast<int>(packet_data.size());

    // 2. Send the packet to the decoder
    int ret = avcodec_send_packet(_codec_context, _packet);
    if (ret < 0)
    {
        std::cerr << "Error sending packet for decoding.\n";
        return false;
    }

    // 3. Receive the decoded frame
    ret = avcodec_receive_frame(_codec_context, _frame_yuv);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return false; 
    else if (ret < 0)
    {
        std::cerr << "Error during decoding!\n";
        return false;
    }

    // 4. Check if video resolution changed
    if (_frame_yuv->width != _current_width || _frame_yuv->height != _current_height || !_sws_context)
    {
        _current_width = _frame_yuv->width;
        _current_height = _frame_yuv->height;

        if (_sws_context)
            sws_freeContext(_sws_context);
        
        if (_frame_bgra->data[0])
            av_freep(&_frame_bgra->data[0]);

        // Initialize SwsContext for YUV to BGRA conversion
        _sws_context = sws_getContext(
            _current_width, _current_height, _codec_context->pix_fmt,
            _current_width, _current_height, AV_PIX_FMT_BGRA,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
        );

        // Allocate memory for the BGRA frame
        av_image_alloc(_frame_bgra->data, _frame_bgra->linesize, _current_width, _current_height, AV_PIX_FMT_BGRA, 32);
    }

    // 5. Convert YUV to BGRA
    sws_scale(
        _sws_context,
        _frame_yuv->data, _frame_yuv->linesize, 0, _current_height,
        _frame_bgra->data, _frame_bgra->linesize
    );

    // 6. Copy the data into an OpenCV Mat
    // We deep copy it to ensure the Mat owns the memory, as FFmpeg will reuse _frame_bgra
    cv::Mat temp(_current_height, _current_width, CV_8UC4, _frame_bgra->data[0], _frame_bgra->linesize[0]);
    temp.copyTo(out_frame);

    return true;
}