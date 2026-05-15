#ifndef APPLICATION_H_
#define APPLICATION_H_

#include <ui/application-ui.h>
#include <capturer/video-capturer.h>
#include <encoder/stream-encoder.h>
#include <decoder/stream-decoder.h>
#include <network/srt-sender.h>

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
    void handle_start_stop_stream();
    void handle_sources_update();

    ApplicationUI   _ui;

    VideoCapturer   _capturer;
    StreamEncoder   _encoder;
    StreamDecoder   _decoder;
    SrtSender       _srt_sender;

    cv::Mat         _loopback_local_frame;
    cv::Mat         _loopback_display_frame;
    std::mutex      _frame_mutex;

    bool            _is_stream_enabled = false;
    bool            _is_preview_enabled = false;
};

#endif /* APPLICATION_H_ */