#include <common.h>
#include <app/application.h>

#include <iostream>

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

bool Application::init()
{
    if (!_ui.init(default_ui_config, default_ui_stream_cfg, ApplicationUI::UiNetworkConfigRx(), ApplicationUI::UiNetworkConfigTx()))
    {
        std::cerr << "Failed to initialize the UI!\n";
        return false;
    }

    // ui.set_web_frame_mem(...);
    // ui.set_loopback_frame_mem(...);

    _ui.set_start_stop_stream_callback([this]() { this->handle_start_stop_stream(); });
    _ui.set_sources_update_callback([this]() { this->handle_sources_update(); });
}

void Application::cleanup()
{
    _ui.shutdown();
}

void Application::run()
{
    handle_sources_update();
    bool running = true;

    while (running)
    {        
        // {
        //     std::lock_guard<std::mutex> lock(frame_mutex);
        //     if (!loopback_local_frame.empty())
        //         loopback_local_frame.copyTo(loopback_display_frame);
        // }

        running = _ui.render();
    }
}

void Application::handle_start_stop_stream()
{
    std::cout << "Handle start stop stream called!\n";
}

void Application::handle_sources_update()
{
    std::cout << "Handle sources update called!\n";

    const ApplicationUI::UiStreamConfig& cfg = _ui.fetch_stream_config();
    std::vector<std::string> sources;
        
    if (ApplicationUI::UiStreamConfig::CaptureTarget::WINDOW == cfg.capture_target)
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

    _ui.set_stream_sources(sources);
}