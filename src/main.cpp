#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <capturer/video-capturer.h>

int main()
{
    VideoCapturer capturer;

    auto windows = VideoCapturer::get_available_windows();
    std::cout << "--- Available windows for capture ---\n";
    for (size_t i = 0; i < windows.size(); ++i)
    {
        std::cout << "[" << i << "] HWND: " << windows[i].first << " | Title: " << windows[i].second << "\n";
    }
    std::cout << "-------------------------------------\n";

    auto monitors = VideoCapturer::get_available_monitors();
    std::cout << "--- Available Monitors ---\n";
    for (size_t i = 0; i < monitors.size(); ++i)
    {
        std::cout << "[" << i << "] HMONITOR: " << monitors[i].first << " | " << monitors[i].second << "\n";
    }
    std::cout << "--------------------------\n";

    CaptureConfig cap_config;
    cap_config.target = CaptureTarget::MONITOR;

    OutputConfig out_config;
    out_config.width = 1280;
    out_config.height = 720;
    out_config.target_fps = 30;

    // Shared resources between the capture thread and the main UI thread
    cv::Mat display_frame;
    std::mutex frame_mutex;

    // 3. Define the callback
    // We use a lambda to capture the shared frame and mutex by reference
    FrameCallback callback = [&display_frame, &frame_mutex](const cv::Mat& frame)
    {
        // Lock the mutex before writing to the shared frame
        std::lock_guard<std::mutex> lock(frame_mutex);
        frame.copyTo(display_frame);
    };

    // 4. Start the capture
    if (!capturer.start(cap_config, out_config, callback))
    {
        std::cerr << "Failed to start capturer!\n";
        return -1;
    }

    std::cout << "Capture started. Press ESC in the video window to stop.\n";

    // 5. Main UI loop
    cv::namedWindow("Stream Preview", cv::WINDOW_NORMAL);
    cv::resizeWindow("Stream Preview", 800, 600);

    while (capturer.is_running())
    {
        cv::Mat local_frame;
        
        // Safely read the latest frame from the capture thread
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            if (!display_frame.empty())
                display_frame.copyTo(local_frame);
        }

        // Display the frame if it's not empty
        if (!local_frame.empty())
            cv::imshow("Stream Preview", local_frame);

        // Wait for 30ms (~33 FPS) and check if ESC (key code 27) was pressed
        if (cv::waitKey(30) == 27)
            break;
    }

    // 6. Cleanup
    capturer.stop();
    cv::destroyAllWindows();
    
    std::cout << "Capture stopped gracefully.\n";

    return 0;
}