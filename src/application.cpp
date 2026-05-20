// #define TRACE_ME

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
    .stream_id = "read:mystream",
    .stream_pwd = "",
    .server_ip = "185.198.166.86",
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
        if (_is_preview_enabled && _decoder.has_error())
        {
            _ui.log_err("Preview stopped due to error!");
            stop_preview();
        }

        if (_is_stream_enabled && !_srt_sender.is_connected())
        {
            const ApplicationUI::UiStreamConfig& stream_cfg = _ui.fetch_stream_config();
            const bool is_web_stream = ApplicationUI::UiStreamConfig::StreamTarget::WEB == stream_cfg.stream_target;

            if (is_web_stream && !_srt_sender.is_connected())
            {
                _ui.log_err("Stream disconnected!");
                stop_streaming();
            }
        }

        {
            std::lock_guard<std::mutex> lock(_loopback_mutex);
            _ui.set_loopback_texture(reinterpret_cast<void*>(_current_loopback_srv), _loopback_w, _loopback_h);
        }

        {
            std::lock_guard<std::mutex> lock(_preview_mutex);
            _ui.set_web_texture(reinterpret_cast<void*>(_current_preview_srv), _preview_w, _loopback_h);
        }

        running = _ui.render();

        if (_loopback_srv_to_release)
        {
            _loopback_srv_to_release->Release();
            _loopback_srv_to_release = nullptr;
        }

        if (_preview_srv_to_release)
        {
            _preview_srv_to_release->Release();
            _preview_srv_to_release = nullptr;
        }
    }
}

bool Application::start_streaming()
{
    const ApplicationUI::UiStreamConfig& stream_cfg = _ui.fetch_stream_config();
    const bool is_web_stream = ApplicationUI::UiStreamConfig::StreamTarget::WEB == stream_cfg.stream_target;

    _ui.log(std::format("Starting stream with {} kbps at {} FPS.", stream_cfg.target_br_kbps, stream_cfg.target_fps));

    HwVideoCapturer::CaptureConfig cap_cfg;
    cap_cfg.target = HwVideoCapturer::Target::DISPLAY;
    cap_cfg.target_fps = stream_cfg.target_fps;
    cap_cfg.source_name = stream_cfg.source;

    if (is_web_stream)
    {
        HwStreamEncoder::EncoderConfig enc_cfg;
        enc_cfg.codec = HwStreamEncoder::StreamCodec::H264_AMF; // need to be gathered from GPU info
        enc_cfg.fps = stream_cfg.target_fps;
        enc_cfg.bitrate_kbps = stream_cfg.target_br_kbps;
        _encoder.init(enc_cfg);

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
        std::lock_guard<std::mutex> lock(_loopback_mutex);
        
        if (_current_loopback_srv) 
        {
            _loopback_srv_to_release = _current_loopback_srv; 
            _current_loopback_srv = nullptr; 
        }

        _loopback_h = 0;
        _loopback_w = 0;
    }
    
    _ui.log("Video Streaming stopped\n");
    _ui.set_ui_locked(ApplicationUI::UiElement::STREAM_CONFIG, false);

    _is_stream_enabled = false;
}

bool Application::start_preview()
{
    const ApplicationUI::UiNetworkConfigRx& rx_cfg = _ui.fetch_network_rx_config();    
    _ui.log(std::format("Starting Rx preview on {}:{}", rx_cfg.server_ip, rx_cfg.server_port));

    SrtReceiver::NetworkConfig srt_cfg;
    srt_cfg.ip = rx_cfg.server_ip;
    srt_cfg.port = rx_cfg.server_port;
    srt_cfg.stream_id = rx_cfg.stream_id;
    srt_cfg.stream_pwd = rx_cfg.stream_pwd;

    if (!_srt_receiver.open_connection(srt_cfg))
    {
        _ui.log_err("Failed to open SRT Receiver connection!");
        return false;
    }

    if (!_decoder.init(
        _ui.get_d3d11_device(), 
        [this](ID3D11Texture2D* tex, ID3D11Device* dev) { this->handle_frame_received(tex, dev); },
        [](AVFrame* frame) { /* Zero callback for audio */ }
    ))
    {
        _ui.log_err("Failed to init Hardware Decoder!");
        _srt_receiver.close_connection();
        return false;
    }

    _is_rx_running = true;
    _rx_thread = std::thread(&Application::srt_rx_loop, this);

    _ui.log("Video Preview (Rx) started\n");
    _ui.set_ui_locked(ApplicationUI::UiElement::RX_CONFIG, true);
    
    _is_preview_enabled = true;
    return true;
}

void Application::stop_preview()
{
    _is_rx_running = false;
    if (_rx_thread.joinable())
        _rx_thread.join();

    _srt_receiver.close_connection();
    _decoder.release();

    {
        std::lock_guard<std::mutex> lock(_preview_mutex);
        if (_current_preview_srv) 
        {
            _preview_srv_to_release = _current_preview_srv; 
            _current_preview_srv = nullptr; 
        }

        _preview_h = 0;
        _preview_w = 0;
    }

    _ui.log("Video Preview (Rx) stopped\n");
    _ui.set_ui_locked(ApplicationUI::UiElement::RX_CONFIG, false);
    
    _is_preview_enabled = false;
}

void Application::handle_frame_captured(ID3D11Texture2D* tex, ID3D11Device* dev)
{
    const bool is_loopback = StreamTarget::LOOPBACK == _ui.fetch_stream_config().stream_target;

    if (is_loopback)
        save_frame_for_loopback(tex, dev);
    else
    {
        std::vector<uint8_t> mpegts_data;
        if (!_encoder.encode_texture(tex, dev, mpegts_data))
            return stop_streaming();
        
        if (!mpegts_data.empty())
            _srt_sender.send(mpegts_data);
    }
}

void Application::handle_frame_received(ID3D11Texture2D* tex, ID3D11Device* dev)
{
    if (!tex || !dev)
        return;

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);

    D3D11_TEXTURE2D_DESC ui_desc = desc;
    ui_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; 
    ui_desc.Usage = D3D11_USAGE_DEFAULT;
    ui_desc.CPUAccessFlags = 0;
    ui_desc.MiscFlags = 0;
    ui_desc.ArraySize = 1; 

    ID3D11Texture2D* ui_texture = nullptr;
    HRESULT hr = dev->CreateTexture2D(&ui_desc, nullptr, &ui_texture);
    if (FAILED(hr)) 
        return; 

    ID3D11DeviceContext* ctx = nullptr;
    dev->GetImmediateContext(&ctx);
    if (ctx)
    {
        ctx->CopySubresourceRegion(ui_texture, 0, 0, 0, 0, tex, 0, nullptr);
        ctx->Release();
    }

    ID3D11ShaderResourceView* new_srv = nullptr;
    hr = dev->CreateShaderResourceView(ui_texture, nullptr, &new_srv);
    
    ui_texture->Release(); 

    if (FAILED(hr)) 
        return; 

    {
        std::lock_guard<std::mutex> lock(_preview_mutex);
        if (_current_preview_srv)
            _current_preview_srv->Release();
        
        _current_preview_srv = new_srv;
        _preview_w = desc.Width;
        _preview_h = desc.Height;
    }
}

void Application::save_frame_for_loopback(ID3D11Texture2D* tex, ID3D11Device* dev)
{
    if (!tex || !dev)
        return;

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);

    D3D11_TEXTURE2D_DESC ui_desc = desc;
    ui_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; 
    ui_desc.Usage = D3D11_USAGE_DEFAULT;
    ui_desc.CPUAccessFlags = 0;
    ui_desc.MiscFlags = 0;

    ID3D11Texture2D* ui_texture = nullptr;
    HRESULT hr = dev->CreateTexture2D(&ui_desc, nullptr, &ui_texture);
    if (FAILED(hr)) 
        return; 

    ID3D11DeviceContext* ctx = nullptr;
    dev->GetImmediateContext(&ctx);
    if (ctx)
    {
        ctx->CopyResource(ui_texture, tex);
        ctx->Release();
    }

    ID3D11ShaderResourceView* new_srv = nullptr;
    hr = dev->CreateShaderResourceView(ui_texture, nullptr, &new_srv);
    
    ui_texture->Release(); 

    if (FAILED(hr)) 
        return; 

    {
        std::lock_guard<std::mutex> lock(_loopback_mutex);
        
        if (_current_loopback_srv)
            _current_loopback_srv->Release();
        
        _current_loopback_srv = new_srv;
        _loopback_w = desc.Width;
        _loopback_h = desc.Height;
    }
}

void Application::handle_start_stop_stream()
{
    if (_is_stream_enabled)
        return stop_streaming();
    
    start_streaming();
}

void Application::handle_start_stop_preview()
{
    if (_is_preview_enabled)
        return stop_preview();
    
    start_preview();
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

void Application::srt_rx_loop()
{
    std::vector<uint8_t> rx_data;
    
    while (_is_rx_running)
    {
        if (_srt_receiver.receive(rx_data) && !rx_data.empty())
        {
            LTRACE("Push SRT data into incoder, size: %u\n", rx_data.size());

            _decoder.push_data(rx_data);
            continue;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}