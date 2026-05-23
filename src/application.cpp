// #define TRACE_ME

#include <common.h>
#include <app/application.h>

#include <iostream>
#include <format>

#include <thread>
#include <chrono>

static const AppViewModel::StreamConfig default_ui_stream_cfg = 
{
    .capture_target = AppViewModel::StreamConfig::CaptureTarget::DISPLAY,
    .stream_target  = AppViewModel::StreamConfig::StreamTarget::LOOPBACK,
    .target_fps     = 30,
    .target_br_kbps = 2500
};

static const AppViewModel::NetworkConfigRx default_ui_network_config_rx = 
{
    .stream_id = "mystream",
    .user_name = "viewer",
    .user_pwd = "",
    .srt_passphrase = "",
    .server_ip = "",
    .server_port = 8890
};

static const AppViewModel::NetworkConfigTx default_ui_network_config_tx = 
{
    .stream_id = "mystream",
    .user_name = "streamer",
    .user_pwd = "",
    .srt_passphrase = "",
    .server_ip = "",
    .server_port = 8890
};

bool Application::init()
{
    if (!glfwInit())
    {
        LOG_ERROR("Failed to initialize GLFW!\n");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    
    // TODO: Add config for glfw window
    _window = glfwCreateWindow(1280, 720, "Batya Streamer", nullptr, nullptr);
    if (!_window) 
    {
        LOG_ERROR("Failed to create GLFW window!\n");
        glfwTerminate();
        return false;
    }

    if (!_gfx.init(_window))
    {
        LOG_ERROR("Failed to initialize GraphicsContext!\n");
        return false;
    }

    if (!_ui.init(_window, &_gfx))
    {
        LOG_ERROR("Failed to initialize ApplicationUI!\n");
        return false;
    }

    _ui.set_start_stop_stream_callback([this]() { this->handle_start_stop_stream(); });
    _ui.set_start_stop_rx_callback([this]() { this->handle_start_stop_preview(); });
    _ui.set_sources_update_callback([this]() { this->handle_sources_update(); });
    _ui.set_clear_logs_callback([this]() { this->handle_logs_clear_request(); });

    _model.stream_config = default_ui_stream_cfg;
    _model.network_tx = default_ui_network_config_tx;
    _model.network_rx = default_ui_network_config_rx;

    return true;
}

void Application::cleanup()
{
    if (_window)
    {
        glfwDestroyWindow(_window);
        glfwTerminate();

        _window = nullptr;
    }
}

void Application::run()
{
    handle_sources_update();
    bool running = true;

    LOG("Application started!\n");

    while (!glfwWindowShouldClose(_window))
    {
        glfwPollEvents();

        if (_model.is_watching && _decoder.has_error())
        {
            LOG_ERROR("Preview stopped due to error!\n");
            stop_preview();
        }

        if (_model.is_broadcasting && !_srt_sender.is_connected())
        {
            const bool is_web_stream = AppViewModel::StreamConfig::StreamTarget::WEB == _model.stream_config.stream_target;
            if (is_web_stream && !_srt_sender.is_connected())
            {
                LOG_ERROR("Stream disconnected!\n");
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

        _ui.render(_model);

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
    const AppViewModel::StreamConfig& stream_cfg = _model.stream_config;
    const bool is_web_stream = AppViewModel::StreamConfig::StreamTarget::WEB == _model.stream_config.stream_target;

    LOG("Starting stream with %u kbps at %u FPS!\n", stream_cfg.target_br_kbps, stream_cfg.target_fps);

    HwVideoCapturer::CaptureConfig cap_cfg;
    cap_cfg.target = HwVideoCapturer::Target::DISPLAY;
    cap_cfg.target_fps = stream_cfg.target_fps;
    cap_cfg.source_name = stream_cfg.capture_sources.empty() ? "" : stream_cfg.capture_sources.at(stream_cfg.selected_source_idx);

    if (is_web_stream)
    {
        HwStreamEncoder::EncoderConfig enc_cfg;
        enc_cfg.codec = HwStreamEncoder::StreamCodec::H264_AMF; // need to be gathered from GPU info
        enc_cfg.fps = stream_cfg.target_fps;
        enc_cfg.bitrate_kbps = stream_cfg.target_br_kbps;
        _encoder.init(enc_cfg);

        const AppViewModel::NetworkConfigTx& network_cfg_tx = _model.network_tx;
        const std::string stream_id = std::format("publish:{}:{}:{}", 
                                                network_cfg_tx.stream_id,
                                                network_cfg_tx.user_name, 
                                                network_cfg_tx.user_pwd);
        
        SrtTransmitter::NetworkConfig network_cfg;
        network_cfg.ip = network_cfg_tx.server_ip;
        network_cfg.port = network_cfg_tx.server_port;
        network_cfg.pass_phrase = network_cfg_tx.srt_passphrase;
        network_cfg.stream_id = stream_id;

        if (!_srt_sender.open_connection(network_cfg))
        {
            LOG_ERROR("Failed to initialize SRT Sender!\n");
            stop_streaming();

            return false;
        }
    }

    if (!_capturer.start(cap_cfg, [this](ID3D11Texture2D* tex, ID3D11Device* dev) { this->handle_frame_captured(tex, dev); }))
    {
        LOG_ERROR("Failed to start video capturer loop!\n");
        stop_streaming();

        return false;
    }

    LOG("Video Streaming started\n");
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
    
    LOG("Video Streaming stopped\n");
}

bool Application::start_preview()
{
    const AppViewModel::NetworkConfigRx& rx_cfg = _model.network_rx;
    LOG("Starting Rx preview on %s:%u!\n", rx_cfg.server_ip.c_str(), rx_cfg.server_port);

    const std::string stream_id = std::format("read:{}:{}:{}", rx_cfg.stream_id, rx_cfg.user_name, rx_cfg.user_pwd);
    SrtReceiver::NetworkConfig network_cfg;
    network_cfg.ip = rx_cfg.server_ip;
    network_cfg.port = rx_cfg.server_port;
    network_cfg.pass_phrase = rx_cfg.srt_passphrase;
    network_cfg.stream_id = stream_id;

    if (!_srt_receiver.open_connection(network_cfg))
    {
        LOG_ERROR("Failed to open SRT Receiver connection!\n");
        return false;
    }

    if (!_decoder.init(
        _gfx.get_device(), 
        [this](ID3D11Texture2D* tex, ID3D11Device* dev) { this->handle_frame_received(tex, dev); },
        [](AVFrame* frame) { /* Zero callback for audio */ }
    ))
    {
        LOG_ERROR("Failed to init Hardware Decoder!\n");
        _srt_receiver.close_connection();
        return false;
    }

    _is_rx_running = true;
    _rx_thread = std::thread(&Application::srt_rx_loop, this);

    LOG("Video Preview (Rx) started\n");
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

    LOG("Video Preview (Rx) stopped\n");
}

void Application::handle_frame_captured(ID3D11Texture2D* tex, ID3D11Device* dev)
{
    const bool is_web_stream = AppViewModel::StreamConfig::StreamTarget::WEB == _model.stream_config.stream_target;

    if (is_web_stream)
    {
        std::vector<uint8_t> mpegts_data;
        if (!_encoder.encode_texture(tex, dev, mpegts_data))
            return stop_streaming();
        
        if (!mpegts_data.empty())
        {
            LTRACE("Send MPEGTS data: %u bytes!\n", mpegts_data.size());
            
            if (!_srt_sender.send(mpegts_data))
                return stop_streaming();
        }
        else
            LTRACE("MPEGTS data is empty!\n");
    }
    else
        save_frame_for_loopback(tex, dev);
}

void Application::handle_frame_received(ID3D11Texture2D* tex, ID3D11Device* dev)
{
    if (!tex || !dev)
        return;

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);

    LOG("SRT Frame received width:%u, height:%u!\n", desc.Width, desc.Height);

    D3D11_TEXTURE2D_DESC ui_desc = desc;
    ui_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; 
    ui_desc.Usage = D3D11_USAGE_DEFAULT;
    ui_desc.CPUAccessFlags = 0;
    ui_desc.MiscFlags = 0;
    ui_desc.ArraySize = 1; 

    ID3D11Texture2D* ui_texture = nullptr;
    const HRESULT hr_tex = dev->CreateTexture2D(&ui_desc, nullptr, &ui_texture);
    if (FAILED(hr_tex)) 
    {
        LOG_ERROR("Failed to create 2D D3D11 texture!\n");
        return;
    }

    ID3D11DeviceContext* ctx = nullptr;
    dev->GetImmediateContext(&ctx);
    if (ctx)
    {
        ctx->CopySubresourceRegion(ui_texture, 0, 0, 0, 0, tex, 0, nullptr);
        ctx->Release();
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = DXGI_FORMAT_R8_UNORM; // Читаем только канал яркости (Y-plane)
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* new_srv = nullptr;
    const HRESULT hr = dev->CreateShaderResourceView(ui_texture, &srv_desc, &new_srv);
    
    ui_texture->Release(); 

    if (FAILED(hr) || nullptr == new_srv) 
    {
        LOG_ERROR("Failed to create shader resource view!\n");
        return;
    }

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
    const bool is_active = _model.is_broadcasting.load();

    if (!is_active)
    {   
        if (!start_streaming())
            return;

        _model.is_broadcasting = true;
    }
    else
    {
        stop_streaming();
        _model.is_broadcasting = false;
    }
}

void Application::handle_start_stop_preview()
{
    const bool is_active = _model.is_watching.load();
    
    if (!is_active)
    {   
        if (!start_preview())
            return;

        _model.is_watching = true;
    }
    else
    {
        stop_preview();
        _model.is_watching = false;
    }
}

void Application::handle_sources_update()
{
    AppViewModel::StreamConfig& cfg = _model.stream_config;
    
    cfg.capture_sources.clear();
    cfg.selected_source_idx = 0;
    
    if (AppViewModel::StreamConfig::CaptureTarget::WINDOW == cfg.capture_target)
    {
        auto windows = HwVideoCapturer::get_available_windows();
        for (const auto& w : windows)
            cfg.capture_sources.push_back(w.Name);
    }
    else
    {
        auto monitors = HwVideoCapturer::get_available_monitors();
        for (const auto& m : monitors)
            cfg.capture_sources.push_back(m.Name);
    }
}

void Application::handle_logs_clear_request()
{
    std::lock_guard<std::mutex> lock(_model.logs_mutex);
    _model.logs.clear();
}

void Application::srt_rx_loop()
{
    std::vector<uint8_t> rx_data;
    
    while (_is_rx_running)
    {
        if (_srt_receiver.receive(rx_data) && !rx_data.empty())
        {
            LTRACE("Push SRT data into decoder, size: %u\n", rx_data.size());

            _decoder.push_data(rx_data);
            continue;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}