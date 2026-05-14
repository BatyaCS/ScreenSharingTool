#include <iostream>
#include <mutex>
#include <fstream>
#include <vector>

#include <opencv2/opencv.hpp>

#include <capturer/video-capturer.h>
#include <encoder/stream-encoder.h>
#include <decoder/stream-decoder.h>
#include <network/srt-sender.h>

#include <ui/application-ui.h>

int main()
{
    VideoCapturer  capturer;
    StreamEncoder  encoder;
    StreamDecoder  decoder;
    SrtSender      srt_sender;

    cv::Mat display_frame;
    std::mutex frame_mutex;

    ApplicationUI ui;
    if (!ui.init("TS3 Streamer - P2P Screen Share", 1280, 720))
    {
        std::cerr << "Failed to initialize the UI!\n";
        return -1;
    }

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

        // 1. Ручные настройки для теста (замените на свой реальный IP сервера)
        std::string server_ip = "185.198.166.86"; // ВАШ IP ЛИНУКС СЕРВЕРА
        uint16_t server_port = 8890;
        std::string stream_id = "publish:mystream";
        std::string password = ""; 

        // 2. Инициализация SRT
        if (!srt_sender.init(server_ip, server_port, stream_id, password)) {
            std::cerr << "Failed to initialize SRT Sender!" << std::endl;
            return; 
        }

        // Map UI settings to Backend config
        VideoCapturer::OutputConfig out_config;
        out_config.width = 1280; // Base resolution. Can be expanded in UI later
        out_config.height = 720;
        out_config.target_fps = ui_settings.target_fps;

        StreamEncoder::EncoderConfig enc_config;
        enc_config.width = out_config.width;
        enc_config.height = out_config.height;
        enc_config.fps = ui_settings.target_fps;
        enc_config.bitrate_kbps = ui_settings.bitrate_kbps;

        VideoCapturer::CaptureConfig cap_config;
        cap_config.target = ui_settings.is_application ? VideoCapturer::Target::APPLICATION : VideoCapturer::Target::DISPLAY;
        
        // Safely cast the generic void* back to native Windows handles
        if (ui_settings.is_application)
            cap_config.window_handle = static_cast<HWND>(ui_settings.selected_handle);
        else
            cap_config.monitor_handle = static_cast<HMONITOR>(ui_settings.selected_handle);

        // Re-initialize encoder and decoder with new settings
        encoder.init(enc_config);
        decoder.init();

        // Start capturing with our loopback callback
        VideoCapturer::FrameCallback loopback_callback = [&](const cv::Mat& raw_frame)
        {
            // Encode
            std::vector<uint8_t> packet = encoder.encode_h264_from_bgra(raw_frame);
            
            if (!packet.empty())
            {
                srt_sender.send_frame(packet);

                cv::Mat decoded_frame;
                // Decode
                if (decoder.decode_h264_to_bgra(packet, decoded_frame))
                {
                    // Safely pass to main thread
                    std::lock_guard<std::mutex> lock(frame_mutex);
                    decoded_frame.copyTo(display_frame);
                }
            }
        };

        if (!capturer.start(cap_config, out_config, loopback_callback))
            std::cerr << "Failed to start capture loop!\n";
    });

    // Handle the "Stop Stream" button click
    ui.set_stop_callback([&]() 
    {
        std::cout << "Stopping stream...\n";
        capturer.stop();
        srt_sender.stop();

        encoder.release();
        decoder.release();
        
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