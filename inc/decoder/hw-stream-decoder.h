#ifndef HW_STREAM_DECODER_H_
#define HW_STREAM_DECODER_H_

#include <d3d11.h>
#include <utils/non-copyable.h>

#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

EXTERN_C_START
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
EXTERN_C_END

class HwStreamDecoder : NonCopyable
{
    static constexpr size_t AVIO_CTX_BUFFER_SIZE = 16384;

public:
    using VideoFrameCallback = std::function<void(ID3D11Texture2D*, ID3D11Device*)>;
    using AudioFrameCallback = std::function<void(AVFrame*)>;

    HwStreamDecoder() = default;
    ~HwStreamDecoder() { release(); }

    bool init(ID3D11Device* device, VideoFrameCallback video_cb, AudioFrameCallback audio_cb);
    void release();

    bool has_error() const { return _has_error.load(); }

    void push_data(const std::vector<uint8_t>& data);

private:
    int read_data(uint8_t* buf, int buf_size);
    void decode_loop();

    static int avio_read_packet(void* opaque, uint8_t* buf, int buf_size);

    static AVPixelFormat get_hw_format(AVCodecContext* ctx, const AVPixelFormat* pix_fmts);
    static std::string get_av_error_string(int err_num);

    ID3D11Device *          _d3d11_device = nullptr;
    VideoFrameCallback      _video_callback;
    AudioFrameCallback      _audio_callback;

    AVFormatContext *       _format_context = nullptr;
    AVIOContext *           _avio_context = nullptr;
    uint8_t *               _avio_buffer = nullptr;

    AVPacket *              _packet = nullptr;
    AVFrame *               _frame  = nullptr;
    
    AVBufferRef *           _hw_device_ctx = nullptr;
    AVCodecContext *        _video_codec_context = nullptr;
    int                     _video_stream_idx = -1;

    AVCodecContext *        _audio_codec_context = nullptr;
    int                     _audio_stream_idx = -1;

    std::vector<uint8_t>    _stream_buffer;
    std::mutex              _buffer_mutex;
    std::condition_variable _buffer_cv;

    std::atomic<bool>       _is_running{false};
    std::atomic<bool>       _has_error{false};

    std::thread             _decode_thread;
};

#endif /* HW_STREAM_DECODER_H_ */