#include <iostream>
#include <mutex>
#include <fstream>
#include <vector>

#include <opencv2/opencv.hpp>

#include <capturer/video-capturer.h>
#include <encoder/stream-encoder.h>
#include <decoder/stream-decoder.h>

#include <ui/application-ui.h>

int main()
{
    // 1. Initialize core modules
    VideoCapturer  capturer;
    StreamEncoder  encoder;
    StreamDecoder  decoder;

    // Shared resources for passing frames from the background thread to the UI thread
    cv::Mat display_frame;
    std::mutex frame_mutex;

    // 2. Initialize the UI
    ApplicationUI ui;
    if (!ui.init("TS3 Streamer - P2P Screen Share", 1280, 720))
    {
        std::cerr << "Failed to initialize the UI!\n";
        return -1;
    }

    // 3. Setup UI Callbacks (The Bridge)

    // Handle fetching sources (Windows/Monitors) and converting them to UI DTOs
    ui.set_refresh_sources_callback([](bool is_application) 
    {
        std::vector<UiSourceItem> result;
        
        if (is_application)
        {
            auto windows = VideoCapturer::get_available_windows();
            for (const auto& w : windows)
            {
                // Cast HWND to generic void* pointer for the UI
                result.push_back({ static_cast<void*>(w.first), w.second });
            }
        }
        else
        {
            auto monitors = VideoCapturer::get_available_monitors();
            for (const auto& m : monitors)
            {
                // Cast HMONITOR to generic void* pointer for the UI
                result.push_back({ static_cast<void*>(m.first), m.second });
            }
        }
        return result;
    });

    // Handle the "Start Stream" button click
    ui.set_start_callback([&](const UiStreamSettings& ui_settings) 
    {
        std::cout << "Starting stream with " << ui_settings.bitrate_kbps << " kbps at " << ui_settings.target_fps << " FPS.\n";

        // Map UI settings to Backend config
        OutputConfig out_config;
        out_config.width = 1280; // Base resolution. Can be expanded in UI later
        out_config.height = 720;
        out_config.target_fps = ui_settings.target_fps;

        EncoderConfig enc_config;
        enc_config.width = out_config.width;
        enc_config.height = out_config.height;
        enc_config.fps = ui_settings.target_fps;
        enc_config.bitrate_kbps = ui_settings.bitrate_kbps;

        CaptureConfig cap_config;
        cap_config.target = ui_settings.is_application ? CaptureTarget::APPLICATION : CaptureTarget::MONITOR;
        
        // Safely cast the generic void* back to native Windows handles
        if (ui_settings.is_application)
        {
            cap_config.window_handle = static_cast<HWND>(ui_settings.selected_handle);
        }
        else
        {
            cap_config.monitor_handle = static_cast<HMONITOR>(ui_settings.selected_handle);
        }

        // Re-initialize encoder and decoder with new settings
        encoder.init(enc_config);
        decoder.init();

        // Start capturing with our loopback callback
        FrameCallback loopback_callback = [&](const cv::Mat& raw_frame)
        {
            // Encode
            std::vector<uint8_t> packet = encoder.encode_frame(raw_frame);
            
            if (!packet.empty())
            {
                cv::Mat decoded_frame;
                // Decode
                if (decoder.decode_packet(packet, decoded_frame))
                {
                    // Safely pass to main thread
                    std::lock_guard<std::mutex> lock(frame_mutex);
                    decoded_frame.copyTo(display_frame);
                }
            }
        };

        if (!capturer.start(cap_config, out_config, loopback_callback))
        {
            std::cerr << "Failed to start capture loop!\n";
        }
    });

    // Handle the "Stop Stream" button click
    ui.set_stop_callback([&]() 
    {
        std::cout << "Stopping stream...\n";
        capturer.stop();
        
        // Clear the display frame safely
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            display_frame.release();
        }
    });

    // 4. Initial UI Population
    // Trigger the fetch so the combo boxes aren't empty on launch
    ui.refresh_sources();

    // 5. Main UI Loop
    bool running = true;
    while (running)
    {
        cv::Mat local_frame;
        
        // Extract the latest frame from the background thread safely
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            if (!display_frame.empty())
            {
                display_frame.copyTo(local_frame);
            }
        }

        // Render the UI frame. This handles all ImGui drawing, GLFW events, and OpenGL rendering.
        // It returns false if the user clicked the [X] to close the window.
        running = ui.render_frame(local_frame, capturer.is_running());
    }

    // 6. Cleanup
    capturer.stop();
    ui.shutdown();

    return 0;
}

// int main()
// {
//     VideoCapturer capturer;
//     StreamEncoder encoder;
//     StreamDecoder decoder;

//     // 1. Setup encoder configuration
//     EncoderConfig enc_config;
//     enc_config.width = 2560;
//     enc_config.height = 1440;
//     enc_config.fps = 60;
//     enc_config.bitrate_kbps = 5000;

//     // Initialize the encoder
//     if (!encoder.init(enc_config))
//     {
//         std::cerr << "Failed to initialize encoder!\n";
//         return -1;
//     }

//     // Initialize the decoder
//     if (!decoder.init())
//     {
//         std::cerr << "Failed to initialize decoder!\n";
//         return -1;
//     }

//     // 2. Setup capturer configuration
//     CaptureConfig cap_config;
//     cap_config.target = CaptureTarget::MONITOR;

//     OutputConfig out_config;
//     out_config.width = enc_config.width;
//     out_config.height = enc_config.height;
//     out_config.target_fps = enc_config.fps;

//     // Shared resources for passing the decoded frame to the main UI thread
//     cv::Mat display_frame;
//     std::mutex frame_mutex;

//     // 3. Define the callback
//     FrameCallback callback = [&](const cv::Mat& frame)
//     {
//         // Encode the raw OpenCV frame into H.264 packets
//         std::vector<uint8_t> packet = encoder.encode_frame(frame);
        
//         if (!packet.empty())
//         {
//             cv::Mat decoded_frame;
            
//             // Decode the H.264 packet back into a BGR frame
//             if (decoder.decode_packet(packet, decoded_frame))
//             {
//                 // Lock the mutex before updating the shared display frame
//                 std::lock_guard<std::mutex> lock(frame_mutex);
//                 decoded_frame.copyTo(display_frame);
//             }
//         }
//     };

//     // 4. Start the capture process
//     std::cout << "Starting capture, encoding, and decoding loopback...\n";
//     if (!capturer.start(cap_config, out_config, callback))
//     {
//         std::cerr << "Failed to start capturer!\n";
//         return -1;
//     }

//     // 5. Main UI loop
//     cv::namedWindow("Stream Preview (Loopback)", cv::WINDOW_NORMAL);
//     // Resize the preview window so it doesn't take up the whole screen
//     cv::resizeWindow("Stream Preview (Loopback)", 2560, 1440);

//     std::cout << "Press ESC in the preview window to stop.\n";

//     while (capturer.is_running())
//     {
//         cv::Mat local_frame;
        
//         // Safely extract the latest decoded frame
//         {
//             std::lock_guard<std::mutex> lock(frame_mutex);
//             if (!display_frame.empty())
//             {
//                 display_frame.copyTo(local_frame);
//             }
//         }

//         // Show the frame
//         if (!local_frame.empty())
//         {
//             cv::imshow("Stream Preview (Loopback)", local_frame);
//         }

//         // Wait for ~16ms (~60 FPS) and check if ESC (key code 27) was pressed
//         if (cv::waitKey(16) == 27)
//         {
//             break;
//         }
//     }

//     // 6. Cleanup
//     capturer.stop();
//     cv::destroyAllWindows();

//     std::cout << "Loopback test completed gracefully.\n";

//     return 0;
// }

// int main()
// {
//     VideoCapturer capturer;

//     auto windows = VideoCapturer::get_available_windows();
//     std::cout << "--- Available windows for capture ---\n";
//     for (size_t i = 0; i < windows.size(); ++i)
//     {
//         std::cout << "[" << i << "] HWND: " << windows[i].first << " | Title: " << windows[i].second << "\n";
//     }
//     std::cout << "-------------------------------------\n";

//     auto monitors = VideoCapturer::get_available_monitors();
//     std::cout << "--- Available Monitors ---\n";
//     for (size_t i = 0; i < monitors.size(); ++i)
//     {
//         std::cout << "[" << i << "] HMONITOR: " << monitors[i].first << " | " << monitors[i].second << "\n";
//     }
//     std::cout << "--------------------------\n";

//     CaptureConfig cap_config;
//     cap_config.target = CaptureTarget::MONITOR;

//     OutputConfig out_config;
//     out_config.width = 1280;
//     out_config.height = 720;
//     out_config.target_fps = 30;

//     // Shared resources between the capture thread and the main UI thread
//     cv::Mat display_frame;
//     std::mutex frame_mutex;

//     // 3. Define the callback
//     // We use a lambda to capture the shared frame and mutex by reference
//     FrameCallback callback = [&display_frame, &frame_mutex](const cv::Mat& frame)
//     {
//         // Lock the mutex before writing to the shared frame
//         std::lock_guard<std::mutex> lock(frame_mutex);
//         frame.copyTo(display_frame);
//     };

//     // 4. Start the capture
//     if (!capturer.start(cap_config, out_config, callback))
//     {
//         std::cerr << "Failed to start capturer!\n";
//         return -1;
//     }

//     std::cout << "Capture started. Press ESC in the video window to stop.\n";

//     // 5. Main UI loop
//     cv::namedWindow("Stream Preview", cv::WINDOW_NORMAL);
//     cv::resizeWindow("Stream Preview", 800, 600);

//     while (capturer.is_running())
//     {
//         cv::Mat local_frame;
        
//         // Safely read the latest frame from the capture thread
//         {
//             std::lock_guard<std::mutex> lock(frame_mutex);
//             if (!display_frame.empty())
//                 display_frame.copyTo(local_frame);
//         }

//         // Display the frame if it's not empty
//         if (!local_frame.empty())
//             cv::imshow("Stream Preview", local_frame);

//         // Wait for 30ms (~33 FPS) and check if ESC (key code 27) was pressed
//         if (cv::waitKey(30) == 27)
//             break;
//     }

//     // 6. Cleanup
//     capturer.stop();
//     cv::destroyAllWindows();
    
//     std::cout << "Capture stopped gracefully.\n";

//     return 0;
// }