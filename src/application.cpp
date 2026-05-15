#include <common.h>
#include <app/application.h>

#include <iostream>
#include <format>

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

static const ApplicationUI::UiNetworkConfigRx default_ui_network_config_rx = 
{
    .stream_id = "mystream",
    .stream_pwd = "123",
    .server_ip = "yourip",
    .server_port = 8890
};

static const ApplicationUI::UiNetworkConfigTx default_ui_network_config_tx = 
{
    .stream_id = "publish:mystream",
    .stream_pwd = "",
    .server_ip = "185.198.166.86",
    .server_port = 8890
};


bool Application::init()
{
    if (!_ui.init(default_ui_config, default_ui_stream_cfg, default_ui_network_config_rx, default_ui_network_config_tx))
    {
        std::cerr << "Failed to initialize the UI!\n";
        return false;
    }

    _ui.set_web_frame_mem(_web_frame);
    _ui.set_loopback_frame_mem(_loopback_frame);

    _ui.set_start_stop_stream_callback([this]() { this->handle_start_stop_stream(); });
    _ui.set_start_stop_rx_callback([this]() { this->handle_start_stop_preview(); });
    _ui.set_sources_update_callback([this]() { this->handle_sources_update(); });

    return true;
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
        {
            std::lock_guard<std::mutex> lock(_loopback_frame_mutex);

            if (_is_loopback_frame_updated)
            {
                _is_loopback_frame_updated = false;
                _loopback_frame_tmp.copyTo(_loopback_frame);
            }
        }

        running = _ui.render();
    }
}

void Application::handle_capturer_frame_received(const cv::Mat& frame)
{
    const ApplicationUI::UiStreamConfig& stream_cfg = _ui.fetch_stream_config();
    
    if (ApplicationUI::UiStreamConfig::StreamTarget::LOOPBACK == stream_cfg.stream_target)
    {            
        std::lock_guard<std::mutex> lock(_loopback_frame_mutex);

        frame.copyTo(_loopback_frame_tmp);
        _is_loopback_frame_updated = true;
    }
    else
    {
        std::vector<uint8_t> packet = _encoder.encode_h264_from_bgra(frame);
        if (!packet.empty())
            _srt_sender.send_frame(packet);
    }
}

void Application::handle_start_stop_stream()
{
    const ApplicationUI::UiStreamConfig& stream_cfg = _ui.fetch_stream_config();
    const bool is_web_stream = ApplicationUI::UiStreamConfig::StreamTarget::WEB == stream_cfg.stream_target;
    
    if (!_is_stream_enabled)
    {
        _ui.log(std::format("Starting stream with {} kbps at {} FPS.", stream_cfg.target_br_kbps, stream_cfg.target_fps));

        VideoCapturer::OutputConfig out_config;
        out_config.width = 1920;
        out_config.height = 1080;
        out_config.target_fps = stream_cfg.target_fps;

        VideoCapturer::CaptureConfig cap_config;
        cap_config.target = stream_cfg.capture_target == ApplicationUI::UiStreamConfig::CaptureTarget::WINDOW ? VideoCapturer::Target::APPLICATION : VideoCapturer::Target::DISPLAY;
        
        if (stream_cfg.capture_target == ApplicationUI::UiStreamConfig::CaptureTarget::WINDOW)
            cap_config.window_handle = static_cast<HWND>(_video_sources[stream_cfg.source_idx]);
        else
            cap_config.monitor_handle = static_cast<HMONITOR>(_video_sources[stream_cfg.source_idx]);

        if (is_web_stream)
        {
            StreamEncoder::EncoderConfig enc_config;
            enc_config.width = out_config.width;
            enc_config.height = out_config.height;
            enc_config.fps = stream_cfg.target_fps;
            enc_config.bitrate_kbps = stream_cfg.target_br_kbps;

            _encoder.init(enc_config);

            const ApplicationUI::UiNetworkConfigTx& network_cfg_tx = _ui.fetch_network_tx_config();
            if (!_srt_sender.init(network_cfg_tx.server_ip, network_cfg_tx.server_port, network_cfg_tx.stream_id, network_cfg_tx.stream_pwd))
                return _ui.log_err("Failed to initialize SRT Sender!");; 
        }

        if (!_capturer.start(cap_config, out_config, [this](const cv::Mat& frame) { this->handle_capturer_frame_received(frame); }))
            return _ui.log_err("Failed to start video capturer loop!\n");

        _ui.log("Video Streaming started\n");
        _is_stream_enabled = true;
    }
    else
    {
        _capturer.stop();
        _encoder.release();
        _srt_sender.stop();
        
        {
            std::lock_guard<std::mutex> lock(_loopback_frame_mutex);
            _loopback_frame_tmp.release();
            _loopback_frame.release();
        }

        _ui.log("Video Streaming stopped\n");
        _is_stream_enabled = false;
    }
    
    _ui.set_ui_locked(ApplicationUI::UiElement::STREAM_CONFIG, _is_stream_enabled);
}

void Application::handle_start_stop_preview()
{
    _ui.log_err("Handle start stop stream preview called!\n");
}

void Application::handle_sources_update()
{
    const ApplicationUI::UiStreamConfig& cfg = _ui.fetch_stream_config();
    std::vector<std::string> ui_sources;
        
    _video_sources.clear();
    if (ApplicationUI::UiStreamConfig::CaptureTarget::WINDOW == cfg.capture_target)
    {
        auto windows = VideoCapturer::get_available_windows();
        for (const auto& w : windows)
        {
            _video_sources.push_back(w.first);
            ui_sources.push_back(w.second);
        }
    }
    else
    {
        auto monitors = VideoCapturer::get_available_monitors();
        for (const auto& m : monitors)
        {
            _video_sources.push_back(m.first);
            ui_sources.push_back(m.second);
        }
    }

    _ui.set_stream_sources(ui_sources);
}