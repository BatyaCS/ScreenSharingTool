#include <common.h>
#include <ui/application-ui.h>
#include <ui/ui-helpers.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_dx11.h>
#include <iostream>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <d3d11_4.h>

static bool InputTextString(const char* label, std::string& str, ImGuiInputTextFlags flags = 0) 
{
    char buf[256];
    strncpy(buf, str.c_str(), sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText(label, buf, sizeof(buf), flags)) {
        str = buf;
        return true;
    }
    return false;
}

bool ApplicationUI::init(const UiConfig& config, const UiStreamConfig& stream_config, const UiNetworkConfigRx& config_rx, const UiNetworkConfigTx& config_tx)
{
    if (!glfwInit())
    {
        LOG_ERROR("Failed to init glfw!\n");

        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    
    _window = glfwCreateWindow(config.width, config.height, config.window_title.c_str(), nullptr, nullptr);
    if (!_window) 
    {
        LOG_ERROR("Failed to init glfw window!\n");

        glfwTerminate(); 
        return false; 
    }

    if (!CreateDeviceD3D(glfwGetWin32Window(_window)))
    {
        LOG_ERROR("Failed to create d3d11 device!\n");

        CleanupDeviceD3D();
        glfwDestroyWindow(_window);
        glfwTerminate();
        return false;
    }


    // Use multithread protection since pd3dDevice used by HwDecoder
    ID3D11Multithread * multithread = nullptr;
    const HRESULT hr = _pd3dDevice->QueryInterface(__uuidof(ID3D11Multithread), (void**)&multithread);
    if (SUCCEEDED(hr) && multithread)
    {
        multithread->SetMultithreadProtected(TRUE); 
        multithread->Release();
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(_window, true);
    ImGui_ImplDX11_Init(_pd3dDevice, _pd3dDeviceContext);

    _stream_config = stream_config;
    _previous_stream_config = stream_config;

    _network_config_rx = config_rx;
    _network_config_tx = config_tx;

    return true;
}

void ApplicationUI::shutdown()
{
    if (_window)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceD3D();
        
        glfwDestroyWindow(_window);
        glfwTerminate();
        _window = nullptr;
    }
}

bool ApplicationUI::render()
{
    if (glfwWindowShouldClose(_window)) 
        return false; 

    glfwPollEvents();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | 
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("MainLayout", nullptr, flags);
    ImGui::PopStyleVar();

    float log_height = 100.0f;
    ImGui::BeginChild("TabsRegion", ImVec2(0, -log_height), true);
    if (ImGui::BeginTabBar("MainTabs")) 
    {
        if (ImGui::BeginTabItem("Broadcaster")) { render_broadcaster_tab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Viewer (Web)")) { render_web_preview_tab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    render_log_window();

    ImGui::End();
    ImGui::Render();

    const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    _pd3dDeviceContext->OMSetRenderTargets(1, &_mainRenderTargetView, nullptr);
    _pd3dDeviceContext->ClearRenderTargetView(_mainRenderTargetView, clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    _pSwapChain->Present(1, 0);

    return true;
}

bool ApplicationUI::render_broadcaster_tab()
{
    ImGui::BeginChild("BroadcasterSettings", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0), false);
    
    if (ImGui::CollapsingHeader("Capture & Encoding", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(is_ui_locked(UiElement::STREAM_CONFIG));
        render_capture_settings();
        render_encoding_settings();
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Network Transmission (Tx)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(is_ui_locked(UiElement::STREAM_CONFIG));
        render_network_tx_settings();
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    if (!is_ui_locked(UiElement::STREAM_CONFIG))
    {
        if (ImGui::Button("Start Broadcast", ImVec2(-1, 40)))
            if (_start_stop_stream_callback) _start_stop_stream_callback();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Stop Broadcast", ImVec2(-1, 40)))
            if (_start_stop_stream_callback) _start_stop_stream_callback();
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("LoopbackPreviewRegion", ImVec2(0, 0), true);
    ImGui::SeparatorText("Loopback Preview");

    _loopback_preview_widget.render(_loopback_srv, _loopback_w, _loopback_h, 0.0f);
    
    ImGui::EndChild();

    return true;
}

bool ApplicationUI::render_web_preview_tab()
{
    if (ImGui::CollapsingHeader("Receiver Configuration (Rx)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(is_ui_locked(UiElement::RX_CONFIG));
        render_network_rx_settings();
        ImGui::EndDisabled();

        ImGui::Spacing();
        if (!is_ui_locked(UiElement::RX_CONFIG))
        {
            if (ImGui::Button("Connect to Stream", ImVec2(200, 30)))
                if (_start_stop_rx_callback) _start_stop_rx_callback();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Disconnect", ImVec2(200, 30)))
                if (_start_stop_rx_callback) _start_stop_rx_callback();
            ImGui::PopStyleColor();
        }
    }

    ImGui::SeparatorText("Live Stream");
    _web_preview_widget.render(_web_srv, _web_w, _web_h, 0.0f);

    return true;
}

void ApplicationUI::render_capture_settings()
{
    ImGui::Text("Capture Source / Stream Target");
    ImGui::Spacing();

    ImGui::RadioButton("Monitor", reinterpret_cast<int*>(&_stream_config.capture_target), 0); 
    // ImGui::SameLine(); ImGui::RadioButton("Application", reinterpret_cast<int*>(&_stream_config.capture_target), 1);

    ImGui::RadioButton("WebSRT", reinterpret_cast<int*>(&_stream_config.stream_target), 0); ImGui::SameLine();
    ImGui::RadioButton("Loopback", reinterpret_cast<int*>(&_stream_config.stream_target), 1);

    if (_stream_config.capture_target != _previous_stream_config.capture_target)
    {
        if (_sources_update_callback) 
            _sources_update_callback();

        _previous_stream_config = _stream_config;
    }

    const std::string combo_label = (_stream_config.capture_target == UiStreamConfig::CaptureTarget::DISPLAY) ? "Select Display" : "Select Window";
    const std::string preview_value = _current_sources.empty() ? "None found" : _current_sources[_selected_source];
    
    if (ImGui::BeginCombo(combo_label.c_str(), preview_value.c_str()))
    {
        for (uint idx = 0; idx < _current_sources.size(); idx++)
        {
            const bool is_selected = _selected_source == idx;
            if (ImGui::Selectable(_current_sources[idx].c_str(), is_selected))
            {
                _stream_config.source = _current_sources[idx];
                _selected_source = idx;
            }

            if (is_selected) 
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Refresh List"))
        if (_sources_update_callback) 
            _sources_update_callback();
}

void ApplicationUI::render_encoding_settings()
{
    ImGui::Spacing();
    ImGui::Text("Encoding Settings");
    ImGui::SliderInt("Target FPS", reinterpret_cast<int*>(&_stream_config.target_fps), TARGET_FPS_MIN, TARGET_FPS_MAX);
    ImGui::SliderInt("Bitrate (kbps)", reinterpret_cast<int*>(&_stream_config.target_br_kbps), TARGET_BITRATE_MIN, TARGET_BITRATE_MAX);
}

void ApplicationUI::render_network_tx_settings()
{
    InputTextString("Stream ID", _network_config_tx.stream_id);
    InputTextString("User Name", _network_config_tx.user_name);
    InputTextString("User Pwd", _network_config_tx.user_pwd, ImGuiInputTextFlags_Password);
    InputTextString("SRT Passphrase", _network_config_tx.srt_passphrase, ImGuiInputTextFlags_Password);
    InputTextString("Server IP", _network_config_tx.server_ip);
    ImGui::InputInt("Server Port", reinterpret_cast<int*>(&_network_config_tx.server_port));
}

void ApplicationUI::render_network_rx_settings()
{
    InputTextString("Stream ID", _network_config_rx.stream_id);
    InputTextString("User Name", _network_config_rx.user_name);
    InputTextString("User Pwd", _network_config_rx.user_pwd, ImGuiInputTextFlags_Password);
    InputTextString("SRT Passphrase", _network_config_rx.srt_passphrase, ImGuiInputTextFlags_Password);
    InputTextString("Server IP", _network_config_rx.server_ip);
    ImGui::InputInt("Server Port", reinterpret_cast<int*>(&_network_config_rx.server_port));
}

void ApplicationUI::render_log_window()
{
    ImGui::Separator();
    ImGui::Text("Application Logs");
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) 
        { _logs.clear(); }

    ImGui::BeginChild("LogScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); 
    
    for (const auto& log : _logs) 
    {
        if (log.is_error) 
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextUnformatted(log.text.c_str());
            ImGui::PopStyleColor();
        } else 
            ImGui::TextUnformatted(log.text.c_str());
    }

    if (_scroll_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        _scroll_to_bottom = false;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

bool ApplicationUI::CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &_pSwapChain, &_pd3dDevice, &featureLevel, &_pd3dDeviceContext);
    
    if (res == DXGI_ERROR_UNSUPPORTED) // Fallback for WARP
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &_pSwapChain, &_pd3dDevice, &featureLevel, &_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void ApplicationUI::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (_pSwapChain) { _pSwapChain->Release(); _pSwapChain = nullptr; }
    if (_pd3dDeviceContext) { _pd3dDeviceContext->Release(); _pd3dDeviceContext = nullptr; }
    if (_pd3dDevice) { _pd3dDevice->Release(); _pd3dDevice = nullptr; }
}

void ApplicationUI::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    _pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    _pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &_mainRenderTargetView);
    pBackBuffer->Release();
}

void ApplicationUI::CleanupRenderTarget()
{
    if (_mainRenderTargetView) { _mainRenderTargetView->Release(); _mainRenderTargetView = nullptr; }
}

void ApplicationUI::log(const std::string& message) 
{
    _logs.push_back({get_current_timestamp() + message, false});

    if (_logs.size() > 100) 
        _logs.erase(_logs.begin());

    _scroll_to_bottom = true;
}

void ApplicationUI::log_err(const std::string& message) 
{
    _logs.push_back({get_current_timestamp() + message, true});

    if (_logs.size() > 100) 
        _logs.erase(_logs.begin());

    _scroll_to_bottom = true;
}

std::string ApplicationUI::get_current_timestamp() const
{
    time_t now = time(0);
    tm *ltm = localtime(&now);

    char time_str[32];
    strftime(time_str, sizeof(time_str), "[%H:%M:%S] ", ltm);
    
    return std::string(time_str);
}