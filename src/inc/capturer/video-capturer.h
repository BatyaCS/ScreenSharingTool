#ifndef VIDEO_CAPTURER_H_
#define VIDEO_CAPTURER_H_

#include <opencv2/opencv.hpp>
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

class VideoCapturer
{
public:
    enum class Target { DISPLAY, APPLICATION };
    struct CaptureConfig
    {
        Target target           = Target::DISPLAY;
        HMONITOR monitor_handle = nullptr;
        HWND window_handle      = nullptr;
    };
    struct OutputConfig
    {
        uint width              = 1280;
        uint height             = 720;
        uint target_fps         = 30;
    };

    typedef std::function<void(const cv::Mat&)> FrameCallback;

    VideoCapturer() = default;
    ~VideoCapturer() { stop(); cleanup_gdi(); }

    VideoCapturer(const VideoCapturer&) = delete;
    VideoCapturer& operator=(const VideoCapturer&) = delete;

    bool start(const CaptureConfig& cap, const OutputConfig& out, FrameCallback callback);
    void stop();

    bool is_running() const { return _is_running.load(); }

    static std::vector<std::pair<HWND, std::string>> get_available_windows();
    static std::vector<std::pair<HMONITOR, std::string>> get_available_monitors();

private:
    void cleanup_gdi();

    void capture_loop(CaptureConfig cap, OutputConfig out, FrameCallback callback);
    const cv::Mat& grab_frame(const CaptureConfig& cap);

    std::atomic<bool> _is_running{false};
    std::atomic<bool> _should_stop{false};
    std::thread       _capture_thread;

    HDC     _hdc_mem = nullptr;
    HBITMAP _hbm_screen = nullptr;
    int     _cached_width = 0;
    int     _cached_height = 0;

    cv::Mat _cached_frame;
};

#endif /* VIDEO_CAPTURER_H_ */