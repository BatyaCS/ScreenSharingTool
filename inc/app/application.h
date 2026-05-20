#ifndef APPLICATION_H_
#define APPLICATION_H_

#include <ui/application-ui.h>
#include <network/srt-transmitter.h>

#include <capturer/hw-video-capturer.h>
#include <encoder/hw-stream-encoder.h>

// #include <capturer/video-capturer.h>
// #include <encoder/stream-encoder.h>
// #include <decoder/stream-decoder.h>

#include <mutex>
#include <fstream>
#include <vector>

#include <opencv2/opencv.hpp>

class Application
{
public:
    Application() = default;
    ~Application() { cleanup(); }

    bool init();
    void cleanup();

    void run();

private:
    // using VideoSources = std::vector<void*>;

    bool start_streaming();
    void stop_streaming();

    void handle_frame_captured(ID3D11Texture2D* tex, ID3D11Device* dev);
    // void handle_capturer_frame_received(const cv::Mat& frame);

    // UI Handlers
    void handle_start_stop_stream();
    void handle_start_stop_preview();
    void handle_sources_update();

    ApplicationUI   _ui;

    HwVideoCapturer _capturer;
    HwStreamEncoder _encoder;

    // VideoCapturer   _capturer;
    // VideoSources    _video_sources;

    // StreamEncoder   _encoder;
    // StreamDecoder   _decoder;

    SrtTransmitter  _srt_sender;

    cv::Mat         _web_frame;
    cv::Mat         _web_frame_tmp;

    cv::Mat         _loopback_frame;
    cv::Mat         _loopback_frame_tmp;

    std::mutex      _loopback_frame_mutex;
    std::mutex      _web_frame_mutex;

    bool            _is_stream_enabled = false;
    bool            _is_preview_enabled = false;

    bool            _is_loopback_frame_updated = false;
};

#endif /* APPLICATION_H_ */