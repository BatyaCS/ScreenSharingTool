#ifndef HW_VIDEO_CAPTURER_H_
#define HW_VIDEO_CAPTURER_H_

#include <ScreenCapture.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <memory>

#include <utils/non-copyable.h>

class HwVideoCapturer : NonCopyable
{
public:
    enum class Target { DISPLAY, APPLICATION };
    struct CaptureConfig
    {
        Target target           = Target::DISPLAY;
        std::string source_name = "";
        
        uint target_fps         = 30;
    };

    typedef std::vector<SL::Screen_Capture::Monitor> Monitors;
    typedef std::vector<SL::Screen_Capture::Window> Windows;

    typedef std::function<void(ID3D11Texture2D*, ID3D11Device*)> HwFrameCallback;

    HwVideoCapturer() = default;
    ~HwVideoCapturer() { stop(); }

    bool start(const CaptureConfig& cap, HwFrameCallback callback);
    void stop();

    bool is_running() const { return _is_running.load(); }

    static Monitors get_available_monitors() { return SL::Screen_Capture::GetMonitors(); }
    static Windows get_available_windows() { return SL::Screen_Capture::GetWindows(); }

private:


    std::atomic<bool> _is_running{false};
    
    std::shared_ptr<SL::Screen_Capture::IScreenCaptureManager> _capture_manager;
};

#endif /* HW_VIDEO_CAPTURER_H_ */