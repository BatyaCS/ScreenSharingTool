#include <capturer/video-capturer.h>
#include <chrono>

bool VideoCapturer::start(const CaptureConfig& cap, const OutputConfig& out, FrameCallback callback)
{
    if (_is_running.load())
        return false;

    _should_stop.store(false);
    _is_running.store(true);
    
    // Start standard std::thread
    _capture_thread = std::thread(&VideoCapturer::capture_loop, this, cap, out, callback);

    return true;
}

void VideoCapturer::stop()
{
    // Signal the loop to terminate
    _should_stop.store(true);

    // Wait for the thread to finish its current iteration
    if (_capture_thread.joinable())
        _capture_thread.join();

    _is_running.store(false);
}

void VideoCapturer::capture_loop(CaptureConfig cap, OutputConfig out, FrameCallback callback)
{
    const auto frame_duration = std::chrono::milliseconds(1000 / out.target_fps);

    // Check the atomic flag on each iteration
    while (!_should_stop.load())
    {
        auto start_time = std::chrono::steady_clock::now();

        cv::Mat raw_frame = grab_frame(cap);
        
        if (!raw_frame.empty())
        {
            cv::Mat resized_frame;
            if (raw_frame.cols != static_cast<int>(out.width) || raw_frame.rows != static_cast<int>(out.height))
                cv::resize(raw_frame, resized_frame, cv::Size(out.width, out.height), 0, 0, cv::INTER_AREA);
            else
                resized_frame = raw_frame;

            // Push to encoder, network, or UI
            if (callback)
                callback(resized_frame);
        }

        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Sleep to maintain the target FPS
        if (elapsed < frame_duration)
            std::this_thread::sleep_for(frame_duration - elapsed);
    }
}

cv::Mat VideoCapturer::grab_frame(const CaptureConfig& cap)
{
    HWND target_hwnd = nullptr;
    int src_x = 0;
    int src_y = 0;
    int width = 0;
    int height = 0;

    // 1. Determine target dimensions and coordinates based on target type
    if (cap.target == CaptureTarget::APPLICATION)
    {
        target_hwnd = cap.window_handle;
        if (!IsWindow(target_hwnd))
        {
            return cv::Mat();
        }
        
        RECT rc;
        GetClientRect(target_hwnd, &rc);
        width = rc.right - rc.left;
        height = rc.bottom - rc.top;
    }
    else
    {
        target_hwnd = GetDesktopWindow(); // The virtual desktop covering all monitors
        
        if (cap.monitor_handle != nullptr)
        {
            MONITORINFO mi;
            mi.cbSize = sizeof(MONITORINFO);
            if (GetMonitorInfo(cap.monitor_handle, &mi))
            {
                src_x = mi.rcMonitor.left;
                src_y = mi.rcMonitor.top;
                width = mi.rcMonitor.right - mi.rcMonitor.left;
                height = mi.rcMonitor.bottom - mi.rcMonitor.top;
            }
        }
        
        // Fallback to primary screen if handle is null or GetMonitorInfo fails
        if (width <= 0 || height <= 0)
        {
            src_x = 0;
            src_y = 0;
            width = GetSystemMetrics(SM_CXSCREEN);
            height = GetSystemMetrics(SM_CYSCREEN);
        }
    }

    if (width <= 0 || height <= 0)
    {
        return cv::Mat();
    }

    // 2. Perform WinAPI capturing
    HDC hdc_window = GetDC(target_hwnd);
    HDC hdc_mem_dc = CreateCompatibleDC(hdc_window);
    HBITMAP hbm_screen = CreateCompatibleBitmap(hdc_window, width, height);
    SelectObject(hdc_mem_dc, hbm_screen);

    // Notice src_x and src_y here: this allows capturing secondary monitors correctly
    BitBlt(hdc_mem_dc, 0, 0, width, height, hdc_window, src_x, src_y, SRCCOPY | CAPTUREBLT);

    // 3. Convert to OpenCV Mat
    cv::Mat mat(height, width, CV_8UC4);
    BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB };

    GetDIBits(hdc_window, hbm_screen, 0, height, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // 4. Cleanup resources
    DeleteObject(hbm_screen);
    DeleteDC(hdc_mem_dc);
    ReleaseDC(target_hwnd, hdc_window);

    return mat;
}

std::vector<std::pair<HWND, std::string>> VideoCapturer::get_available_windows()
{
    std::vector<std::pair<HWND, std::string>> windows;
    EnumWindows([](HWND hwnd, LPARAM l_param) -> BOOL
    {
        if (IsWindowVisible(hwnd))
        {
            int length = GetWindowTextLengthA(hwnd);
            if (length > 0)
            {
                char title[256];
                GetWindowTextA(hwnd, title, sizeof(title));
                auto* vec = reinterpret_cast<std::vector<std::pair<HWND, std::string>>*>(l_param);
                vec->emplace_back(hwnd, std::string(title));
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windows));
    
    return windows;
}

std::vector<std::pair<HMONITOR, std::string>> VideoCapturer::get_available_monitors()
{
    std::vector<std::pair<HMONITOR, std::string>> monitors;

    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR h_monitor, HDC hdc_monitor, LPRECT lprc_monitor, LPARAM l_param) -> BOOL
    {
        auto* vec = reinterpret_cast<std::vector<std::pair<HMONITOR, std::string>>*>(l_param);

        MONITORINFOEXA mi;
        mi.cbSize = sizeof(MONITORINFOEXA);
        
        // Get details about the monitor
        if (GetMonitorInfoA(h_monitor, &mi))
        {
            std::string name = mi.szDevice;
            
            // Calculate resolution for UI convenience
            int width = mi.rcMonitor.right - mi.rcMonitor.left;
            int height = mi.rcMonitor.bottom - mi.rcMonitor.top;
            
            name += " (" + std::to_string(width) + "x" + std::to_string(height) + ")";

            vec->emplace_back(h_monitor, name);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&monitors));

    return monitors;
}