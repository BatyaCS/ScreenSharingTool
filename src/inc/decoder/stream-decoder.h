#ifndef STREAM_DECODER_H_
#define STREAM_DECODER_H_

#include <vector>
#include <cstdint>
#include <opencv2/opencv.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class StreamDecoder
{
public:
    StreamDecoder() = default;
    ~StreamDecoder() { release(); }

    StreamDecoder(const StreamDecoder&) = delete;
    StreamDecoder& operator=(const StreamDecoder&) = delete;

    bool init();
    void release();

    // Takes an encoded H.264 packet and attempts to decode it.
    // Returns true if a full frame was successfully decoded and placed into out_frame.
    bool decode_packet(const std::vector<uint8_t>& packet_data, cv::Mat& out_frame);

private:
    AVCodecContext* _codec_context = nullptr;
    AVFrame *       _frame_yuv = nullptr;
    AVFrame *       _frame_bgra = nullptr;
    AVPacket *      _packet = nullptr;
    SwsContext *    _sws_context = nullptr;

    // Track current resolution to re-initialize SwsContext if the stream changes
    int _current_width = 0;
    int _current_height = 0;
};

#endif /* STREAM_DECODER_H_ */