#ifndef HW_STREAM_ENCODER_H_
#define HW_STREAM_ENCODER_H_

#include <d3d11.h>
#include <utils/non-copyable.h>

#include <vector>
#include <string>

EXTERN_C_START
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavformat/avformat.h>
EXTERN_C_END

class HwStreamEncoder : NonCopyable
{
public:
    enum class StreamCodec
    {
        H264_NVEC,
        H264_QSV,
        H264_AMF,
    };
    struct EncoderConfig
    {
        StreamCodec codec   = StreamCodec::H264_NVEC;

        uint fps            = 60;
        uint bitrate_kbps   = 3000;
    };

    HwStreamEncoder() = default;
    ~HwStreamEncoder() { release(); }

    bool init(const EncoderConfig& config);
    void release();

    bool encode_texture(ID3D11Texture2D* texture, ID3D11Device* device, std::vector<uint8_t>& data);

private:
    bool first_frame_init(ID3D11Device* device, ID3D11Texture2D* texture);
 
    static int avio_write_packet(void * opaque, const uint8_t * buf, int buf_size);

    static std::string get_av_error_string(int err_num);
    static std::string to_ffmpeg_codec(StreamCodec codec);

    EncoderConfig           _config;

    AVBufferRef *           _hw_device_ctx = nullptr;
    AVCodecContext *        _codec_context = nullptr;
    AVFrame *               _hw_frame = nullptr;
    AVPacket *              _packet = nullptr;

    AVFormatContext *       _format_context = nullptr;
    AVStream *              _video_stream = nullptr;
    AVIOContext *           _avio_context = nullptr;
    uint8_t *               _avio_buffer = nullptr;
    std::vector<uint8_t>    _muxed_data; 

    uint                    _frame_pts = 0;
    bool                    _is_first_frame_received = false;
};

#endif /* HW_STREAM_ENCODER_H_ */