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

    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, [](GLFWwindow* win, int width, int height) 
    {
        auto * app = static_cast<Application*>(glfwGetWindowUserPointer(win));
        if (app) 
            app->render_window();
    });

    glfwSetWindowRefreshCallback(_window, [](GLFWwindow* win) 
    {
        auto * app = static_cast<Application*>(glfwGetWindowUserPointer(win));
        if (app) 
            app->render_window();
    });

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

        render_window();
    }
}

void Application::render_window()
{
    int w_tmp, h_tmp;
    glfwGetFramebufferSize(_window, &w_tmp, &h_tmp);

    if (w_tmp != _window_width || h_tmp != _window_height)
    {
        if (w_tmp > 0 && h_tmp > 0)
            _gfx.resize(static_cast<uint>(w_tmp), static_cast<uint>(h_tmp));

        _window_width = w_tmp;
        _window_height = h_tmp;
    }

    const AppViewModel::FrameSize loopback_size = _model.loopback_frame_size.load();
    if (loopback_size.width > 0 && loopback_size.height > 0)
        _model.loopback_texture.resize(_gfx.get_device(), loopback_size.width, loopback_size.height);

    const AppViewModel::FrameSize preview_size = _model.preview_frame_size.load();
    if (preview_size.width > 0 && preview_size.height > 0)
        _model.preview_texture.resize(_gfx.get_device(), preview_size.width, preview_size.height);

    _ui.render(_model);
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

        const SrtTransmitter::NetworkConfig cfg = to_srt_network_cfg(_model.network_tx);
        if (!_srt_sender.open_connection(cfg))
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
    
    LOG("Video Streaming stopped\n");
}

bool Application::start_preview()
{
    const SrtReceiver::NetworkConfig cfg = to_srt_network_cfg(_model.network_rx);
    if (!_srt_receiver.open_connection(cfg))
    {
        LOG_ERROR("Failed to open SRT Receiver connection!\n");
        return false;
    }

    if (!_decoder.init(
        _gfx.get_device(), 
        [this](ID3D11Texture2D* tex, ID3D11Device* dev, uint slice_index) { this->handle_frame_received(tex, dev, slice_index); },
        [](AVFrame* frame) { /* Zero callback for audio */ }
    ))
    {
        LOG_ERROR("Failed to init Hardware Decoder!\n");
        _srt_receiver.close_connection();
        return false;
    }

    _is_rx_running = true;
    _rx_thread = std::thread(&Application::srt_rx_loop, this);

    LOG("Starting RX preview on %s:%u!\n", cfg.ip.c_str(), cfg.port);
    return true;
}

void Application::stop_preview()
{
    _is_rx_running = false;
    if (_rx_thread.joinable())
        _rx_thread.join();

    _srt_receiver.close_connection();
    _decoder.release();

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

void Application::handle_frame_received(ID3D11Texture2D* tex, ID3D11Device* dev, uint slice_index)
{
    if (!tex)
        return;

    if (dev != _gfx.get_device()) 
    {
        LOG_ERROR("Device mismatch!\n");
        return; 
    }

    ID3D11Texture2D * rgba_tex = _yuv_rgb_converter.convert(dev, tex, slice_index);
    if (!rgba_tex)
        return;

    D3D11_TEXTURE2D_DESC desc;
    rgba_tex->GetDesc(&desc);
    
    ID3D11DeviceContext* ctx = _gfx.get_context();
    _model.preview_frame_size.store({desc.Width, desc.Height});
    _model.preview_texture.copy_from(ctx, rgba_tex);
}

void Application::save_frame_for_loopback(ID3D11Texture2D* tex, ID3D11Device* dev)
{
    if (!_model.is_broadcasting.load() || !tex) 
        return;

    ID3D11Texture2D* my_tex = _gfx_bridge.transfer(dev, tex, _gfx.get_device());
    if (!my_tex)
    {
        LOG_ERROR("CrossDevice bridge failed to transfer texture!\n");
        return; 
    }

    D3D11_TEXTURE2D_DESC desc;
    my_tex->GetDesc(&desc);
    
    ID3D11DeviceContext * ctx = _gfx.get_context();
    _model.loopback_frame_size.store({desc.Width, desc.Height});
    _model.loopback_texture.copy_from(ctx, my_tex);
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
        _model.is_broadcasting = false;
        stop_streaming();
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
        _model.is_watching = false;
        stop_preview();
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
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

/* static */ 
SrtReceiver::NetworkConfig Application::to_srt_network_cfg(const AppModels::NetworkConfigRx& cfg)
{
    SrtReceiver::NetworkConfig network_cfg;
    network_cfg.ip = cfg.server_ip;
    network_cfg.port = cfg.server_port;
    network_cfg.pass_phrase = cfg.srt_passphrase;
    network_cfg.stream_id = std::format("read:{}:{}:{}", cfg.stream_id, cfg.user_name, cfg.user_pwd);

    return network_cfg;
}

/* static */ 
SrtTransmitter::NetworkConfig Application::to_srt_network_cfg(const AppModels::NetworkConfigTx& cfg)
{
    SrtTransmitter::NetworkConfig network_cfg;
    network_cfg.ip = cfg.server_ip;
    network_cfg.port = cfg.server_port;
    network_cfg.pass_phrase = cfg.srt_passphrase;
    network_cfg.stream_id = std::format("publish:{}:{}:{}", cfg.stream_id, cfg.user_name, cfg.user_pwd);

    return network_cfg;
}