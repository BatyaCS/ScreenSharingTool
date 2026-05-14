#include <capturer/video-capturer.h>
#include <chrono>

bool VideoCapturer::start(const CaptureConfig& cap, const OutputConfig& out, FrameCallback callback)
{
    if (_is_running.load())
        return false;

    _should_stop.store(false);
    _is_running.store(true);
    
    _capture_thread = std::thread(&VideoCapturer::capture_loop, this, cap, out, callback);

    return true;
}

void VideoCapturer::stop()
{
    _should_stop.store(true);

    if (_capture_thread.joinable())
        _capture_thread.join();

    _is_running.store(false);
}

void VideoCapturer::cleanup_gdi()
{
    if (_hbm_screen)
    {
        DeleteObject(_hbm_screen);
        _hbm_screen = nullptr;
    }

    if (_hdc_mem)
    {
        DeleteDC(_hdc_mem);
        _hdc_mem = nullptr;
    }

    _cached_width = 0;
    _cached_height = 0;
}

void VideoCapturer::capture_loop(CaptureConfig cap, OutputConfig out, FrameCallback callback)
{
    const auto frame_duration = std::chrono::milliseconds(1000 / out.target_fps);

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

            if (callback)
                callback(resized_frame);
        }

        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (elapsed < frame_duration)
            std::this_thread::sleep_for(frame_duration - elapsed);
    }
}

const cv::Mat& VideoCapturer::grab_frame(const CaptureConfig& cap)
{
    HWND target_hwnd = nullptr;
    int src_x = 0;
    int src_y = 0;
    int width = 0;
    int height = 0;

    // 1. Determine target dimensions
    if (cap.target == Target::APPLICATION) // У вас было Target::APPLICATION
    {
        target_hwnd = cap.window_handle;
        if (!IsWindow(target_hwnd))
            return _null_frame;
        
        RECT rc;
        GetClientRect(target_hwnd, &rc);
        width = rc.right - rc.left;
        height = rc.bottom - rc.top;
    }
    else
    {
        target_hwnd = GetDesktopWindow(); 
        
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
        
        if (width <= 0 || height <= 0)
        {
            src_x = 0;
            src_y = 0;
            width = GetSystemMetrics(SM_CXSCREEN);
            height = GetSystemMetrics(SM_CYSCREEN);
        }
    }

    if (width <= 0 || height <= 0)
        return _null_frame;

    HDC hdc_window = GetDC(target_hwnd);

    // 2. Reallocate resources ONLY if dimensions changed
    if (width != _cached_width || height != _cached_height)
    {
        cleanup_gdi(); // Clean old buffers

        _hdc_mem = CreateCompatibleDC(hdc_window);
        _hbm_screen = CreateCompatibleBitmap(hdc_window, width, height);
        
        // cv::Mat::create is smart: it only reallocates if size or type changed
        _cached_frame.create(height, width, CV_8UC4);
        
        _cached_width = width;
        _cached_height = height;
    }

    // 3. Perform WinAPI capturing using cached resources
    HBITMAP hbm_old = (HBITMAP)SelectObject(_hdc_mem, _hbm_screen);

    // Добавляем проверку: если это окно, используем специальный метод
    if (cap.target == Target::APPLICATION)
        PrintWindow(target_hwnd, _hdc_mem, 3);
    else
        BitBlt(_hdc_mem, 0, 0, width, height, hdc_window, src_x, src_y, SRCCOPY | CAPTUREBLT);

    // 4. Extract bits directly into our cached OpenCV Mat
    BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB };
    GetDIBits(hdc_window, _hbm_screen, 0, height, _cached_frame.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // Restore old bitmap and release window DC (Window DCs should always be released promptly)
    SelectObject(_hdc_mem, hbm_old);
    ReleaseDC(target_hwnd, hdc_window);

    // Return the cached frame. 
    // IMPORTANT: Since we use it sequentially in the same thread (capture -> encode),
    // returning the reference is perfectly safe and avoids ANOTHER copy.
    return _cached_frame;
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