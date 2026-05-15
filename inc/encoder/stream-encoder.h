#ifndef STREAM_ENCODER_H_
#define STREAM_ENCODER_H_

#include <vector>
#include <cstdint>
#include <opencv2/opencv.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}

class StreamEncoder
{
public:
    struct EncoderConfig
    {
        uint width          = 1920;
        uint height         = 1080;
        uint fps            = 60;
        uint bitrate_kbps   = 3000;
    };

    StreamEncoder() = default;
    ~StreamEncoder() { release(); }

    StreamEncoder(const StreamEncoder&) = delete;
    StreamEncoder& operator=(const StreamEncoder&) = delete;

    bool init(const EncoderConfig& config);
    void release();

    std::vector<uint8_t> encode_h264_from_bgra(const cv::Mat& frame);

private:
    AVCodecContext *    _codec_context = nullptr;
    AVFrame *           _frame = nullptr;
    AVPacket *          _packet = nullptr;
    SwsContext *        _sws_context = nullptr;

    EncoderConfig       _config;

    uint                _frame_pts = 0;

    AVFormatContext*        _format_context = nullptr;
    AVStream*               _video_stream = nullptr;
    AVIOContext*            _avio_context = nullptr;
    uint8_t*                _avio_buffer = nullptr;
    std::vector<uint8_t>    _muxed_data; 

    static int avio_write_packet(void *opaque, const uint8_t *buf, int buf_size);
};

#endif /* STREAM_ENCODER_H_ */