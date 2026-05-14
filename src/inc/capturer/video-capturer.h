#ifndef VIDEO_CAPTURER_H_
#define VIDEO_CAPTURER_H_

#include <opencv2/opencv.hpp>
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

enum class CaptureTarget
{
    MONITOR,
    APPLICATION
};

struct CaptureConfig
{
    CaptureTarget target = CaptureTarget::MONITOR;
    HMONITOR      monitor_handle = nullptr;
    HWND          window_handle = nullptr;
};

struct OutputConfig
{
    unsigned int width = 1280;
    unsigned int height = 720;
    unsigned int target_fps = 30;
};

typedef std::function<void(const cv::Mat&)> FrameCallback;

class VideoCapturer
{
public:
    VideoCapturer() = default;
    ~VideoCapturer() { stop(); }

    VideoCapturer(const VideoCapturer&) = delete;
    VideoCapturer& operator=(const VideoCapturer&) = delete;

    bool start(const CaptureConfig& cap, const OutputConfig& out, FrameCallback callback);
    void stop();

    bool is_running() const { return _is_running.load(); }

    static std::vector<std::pair<HWND, std::string>> get_available_windows();
    static std::vector<std::pair<HMONITOR, std::string>> get_available_monitors();

private:
    void capture_loop(CaptureConfig cap, OutputConfig out, FrameCallback callback);
    cv::Mat grab_frame(const CaptureConfig& cap);

    std::atomic<bool> _is_running{false};
    std::atomic<bool> _should_stop{false};
    std::thread       _capture_thread;
};

#endif /* VIDEO_CAPTURER_H_ */