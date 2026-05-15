#include <common.h>
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

static const ApplicationUI::UiConfig default_ui_config = 
{
    .window_title = "Batya Streamer",
    .width = 1280,
    .height = 720
};

static const ApplicationUI::UiStreamConfig default_ui_stream_cfg = 
{
    .capture_target = ApplicationUI::UiStreamConfig::CaptureTarget::DISPLAY,
    .stream_target  = ApplicationUI::UiStreamConfig::StreamTarget::LOOPBACK,
    .target_fps     = 30,
    .target_br_kbps = 2500
};

int main()
{
    VideoCapturer  capturer;
    StreamEncoder  encoder;
    StreamDecoder  decoder;
    SrtSender      srt_sender;

    cv::Mat loopback_local_frame;
    cv::Mat loopback_display_frame;
    std::mutex frame_mutex;

    ApplicationUI ui;
    if (!ui.init(default_ui_config, default_ui_stream_cfg))
    {
        std::cerr << "Failed to initialize the UI!\n";
        return -1;
    }

    // ui.set_web_frame_mem(web_display_frame);
    ui.set_loopback_frame_mem(loopback_display_frame);
    
    ui.set_refresh_sources_callback([](ApplicationUI::UiStreamConfig::CaptureTarget target) 
    {
        std::vector<std::string> sources;
        
        if (ApplicationUI::UiStreamConfig::CaptureTarget::WINDOW == target)
        {
            auto windows = VideoCapturer::get_available_windows();
            for (const auto& w : windows)
                sources.push_back(w.second);
        }
        else
        {
            auto monitors = VideoCapturer::get_available_monitors();
            for (const auto& m : monitors)
                sources.push_back(m.second);
        }

        return sources;
    });

    // Handle the "Start Stream" button click
    ui.set_start_stream_callback([&](const ApplicationUI::UiStreamConfig& ui_stream_config) 
    {
        std::cout << "Starting stream with " << ui_stream_config.target_br_kbps << " kbps at " << ui_stream_config.target_fps << " FPS.\n";

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
        out_config.target_fps = ui_stream_config.target_fps;

        StreamEncoder::EncoderConfig enc_config;
        enc_config.width = out_config.width;
        enc_config.height = out_config.height;
        enc_config.fps = ui_stream_config.target_fps;
        enc_config.bitrate_kbps = ui_stream_config.target_br_kbps;

        VideoCapturer::CaptureConfig cap_config;
        cap_config.target = ui_stream_config.capture_target == ApplicationUI::UiStreamConfig::CaptureTarget::WINDOW ? VideoCapturer::Target::APPLICATION : VideoCapturer::Target::DISPLAY;
        
        // Safely cast the generic void* back to native Windows handles
        if (ui_stream_config.capture_target == ApplicationUI::UiStreamConfig::CaptureTarget::WINDOW)
            cap_config.window_handle = static_cast<HWND>(VideoCapturer::get_available_windows()[ui_stream_config.source_idx].first);
        else
            cap_config.monitor_handle = static_cast<HMONITOR>(VideoCapturer::get_available_monitors()[ui_stream_config.source_idx].first);

        // Re-initialize encoder and decoder with new settings
        encoder.init(enc_config);
        decoder.init();

        // Start capturing with our loopback callback
        VideoCapturer::FrameCallback loopback_callback = [&](const cv::Mat& raw_frame)
        {
            std::vector<uint8_t> packet = encoder.encode_h264_from_bgra(raw_frame);
            
            if (!packet.empty())
            {
                if (ui_stream_config.stream_target == ApplicationUI::UiStreamConfig::StreamTarget::WEB)
                    srt_sender.send_frame(packet);
                else
                {
                    cv::Mat decoded_frame;

                    if (decoder.decode_h264_to_bgra(packet, decoded_frame))
                    {
                        // Safely pass to main thread
                        std::lock_guard<std::mutex> lock(frame_mutex);
                        decoded_frame.copyTo(loopback_local_frame);
                    }
                }
            }
        };

        if (!capturer.start(cap_config, out_config, loopback_callback))
            std::cerr << "Failed to start capture loop!\n";
    });

    ui.set_stop_stream_callback([&]()
    {
        std::cout << "Stopping stream...\n";
        capturer.stop();

        encoder.release();
        decoder.release();

        srt_sender.stop();
            
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            loopback_display_frame.release();
        }
    });

    ui.refresh_sources();
    bool running = true;

    while (running)
    {        
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            if (!loopback_local_frame.empty())
                loopback_local_frame.copyTo(loopback_display_frame);
        }

        running = ui.render();
    }

    // 6. Cleanup
    capturer.stop();
    ui.shutdown();

    return 0;
}