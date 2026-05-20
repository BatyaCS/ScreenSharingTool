#include <common.h>
#include <app/application.h>

#include <iostream>
#include <format>

#include <thread>
#include <chrono>

// std::this_thread::sleep_for(std::chrono::milliseconds(100));

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

    LOG("Application started!\n");

    while (running)
    {        
        // {
        //     std::lock_guard<std::mutex> lock(_loopback_frame_mutex);

        //     if (_is_loopback_frame_updated)
        //     {
        //         _is_loopback_frame_updated = false;
        //         _loopback_frame_tmp.copyTo(_loopback_frame);
        //     }
        // }

        running = _ui.render();
    }
}

bool Application::start_streaming()
{
    const ApplicationUI::UiStreamConfig& stream_cfg = _ui.fetch_stream_config();
    const bool is_web_stream = ApplicationUI::UiStreamConfig::StreamTarget::WEB == stream_cfg.stream_target;

    _ui.log(std::format("Starting stream with {} kbps at {} FPS.", stream_cfg.target_br_kbps, stream_cfg.target_fps));

    HwStreamEncoder::EncoderConfig enc_cfg;
    enc_cfg.codec = HwStreamEncoder::StreamCodec::H264_AMF; // need to be gathered from GPU info
    enc_cfg.fps = stream_cfg.target_fps;
    enc_cfg.bitrate_kbps = stream_cfg.target_br_kbps;
    _encoder.init(enc_cfg);

    HwVideoCapturer::CaptureConfig cap_cfg;
    cap_cfg.target = HwVideoCapturer::Target::DISPLAY;
    cap_cfg.target_fps = stream_cfg.target_fps;
    cap_cfg.source_name = stream_cfg.source;

    const ApplicationUI::UiNetworkConfigTx& network_cfg_tx = _ui.fetch_network_tx_config();
    SrtTransmitter::NetworkConfig network_cfg;
    network_cfg.ip = network_cfg_tx.server_ip;
    network_cfg.port = network_cfg_tx.server_port;
    network_cfg.stream_id = network_cfg_tx.stream_id;
    network_cfg.stream_pdw = network_cfg_tx.stream_pwd;

    if (!_srt_sender.open_connection(network_cfg))
    {
        _ui.log_err("Failed to initialize SRT Sender!");
        stop_streaming();

        return false;
    }

    if (!_capturer.start(cap_cfg, [this](ID3D11Texture2D* tex, ID3D11Device* dev) { this->handle_frame_captured(tex, dev); }))
    {
        _ui.log_err("Failed to start video capturer loop!\n");
        stop_streaming();

        return false;
    }

    _ui.log("Video Streaming started\n");
    _ui.set_ui_locked(ApplicationUI::UiElement::STREAM_CONFIG, true);

    _is_stream_enabled = true;
    return true;
}

void Application::stop_streaming()
{
    _capturer.stop();
    _encoder.release();
    _srt_sender.close_connection();
    
    {
        std::lock_guard<std::mutex> lock(_loopback_frame_mutex);
        _loopback_frame_tmp.release();
        _loopback_frame.release();
    }

    _ui.log("Video Streaming stopped\n");
    _ui.set_ui_locked(ApplicationUI::UiElement::STREAM_CONFIG, false);

    _is_stream_enabled = false;
}

void Application::handle_frame_captured(ID3D11Texture2D* tex, ID3D11Device* dev)
{
    std::vector<uint8_t> mpegts_data;
    if (!_encoder.encode_texture(tex, dev, mpegts_data))
        return stop_streaming();
    
    if (!mpegts_data.empty())
        _srt_sender.send(mpegts_data);
}

void Application::handle_start_stop_stream()
{
    if (_is_stream_enabled)
        return stop_streaming();
    
    start_streaming();
}

void Application::handle_start_stop_preview()
{
    _ui.log_err("Handle start stop stream preview called!\n");
}

void Application::handle_sources_update()
{
    const ApplicationUI::UiStreamConfig& cfg = _ui.fetch_stream_config();
    std::vector<std::string> ui_sources;
        
    if (ApplicationUI::UiStreamConfig::CaptureTarget::WINDOW == cfg.capture_target)
    {
        auto windows = HwVideoCapturer::get_available_windows();
        for (const auto& w : windows)
            ui_sources.push_back(w.Name);
    }
    else
    {
        auto monitors = HwVideoCapturer::get_available_monitors();
        for (const auto& m : monitors)
            ui_sources.push_back(m.Name);
    }

    _ui.set_stream_sources(ui_sources);
}