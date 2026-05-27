#include <common.h>
#include <ui/application-ui.h>
#include <graphics/graphics-context.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_dx11.h>
#include <imgui_stdlib.h>
#include <GLFW/glfw3.h>

bool ApplicationUI::init(GLFWwindow * window, GraphicsContext * gfx)
{
    if (!window || !gfx)
    {
        LOG_ERROR("Received invalid dependencies GLFWwindow or GraphicsContext!\n");
        return false;
    }

    _window = window;
    _gfx = gfx;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(_window, true);
    ImGui_ImplDX11_Init(_gfx->get_device(), _gfx->get_context());

    return true;
}

void ApplicationUI::shutdown()
{
    if (_window)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        _window = nullptr;
        _gfx = nullptr;
    }
}

bool ApplicationUI::render(AppViewModel& view)
{
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

    ImGui::BeginChild("TabsRegion", ImVec2(0, 0), true);
    if (ImGui::BeginTabBar("MainTabs")) 
    {
        if (ImGui::BeginTabItem("Broadcaster")) { render_broadcaster_tab(view); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Viewer (Web)")) { render_web_preview_tab(view); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::End();
    ImGui::Render();

    const GraphicsContext::Colors clear_color = { 0.1f, 0.1f, 0.1f, 1.0f };
    _gfx->clear_render_target(clear_color);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    _gfx->present();

    return true;
}

bool ApplicationUI::render_broadcaster_tab(AppViewModel& view)
{
    const float log_height = 120.0f;
    ImGui::BeginChild("TopRegion", ImVec2(0, -log_height), false);

    float avail_width = ImGui::GetContentRegionAvail().x;
    float settings_width = 420.0f; 
    if (settings_width > avail_width * 0.5f)
        settings_width = avail_width * 0.5f;

    ImGui::BeginChild("BroadcasterSettings", ImVec2(settings_width, 0), false);
    
    if (ImGui::CollapsingHeader("Capture & Encoding", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(view.is_broadcasting);
        render_capture_settings(view);
        render_encoding_settings(view);
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Network Transmission (Tx)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(view.is_broadcasting);
        render_network_tx_settings(view);
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    if (!view.is_broadcasting)
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

    const AppViewModel::FrameSize size = view.loopback_frame_size.load();
    _loopback_preview_widget.render(view.loopback_texture.get_texture(), size.width, size.height, 0.0f);
    
    ImGui::EndChild();

    ImGui::EndChild();

    render_log_window(view);

    return true;
}

bool ApplicationUI::render_web_preview_tab(AppViewModel& view)
{
    if (ImGui::CollapsingHeader("Receiver Configuration (Rx)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(view.is_watching);
        
        ImGui::PushItemWidth(300.0f);
        render_network_rx_settings(view);
        ImGui::PopItemWidth();

        ImGui::EndDisabled();

        ImGui::Spacing();
        if (!view.is_watching)
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

    const AppViewModel::FrameSize size = view.preview_frame_size.load();
    _loopback_preview_widget.render(view.preview_texture.get_texture(), size.width, size.height, 0.0f);

    return true;
}

void ApplicationUI::render_capture_settings(AppViewModel& view)
{
    ImGui::Text("Capture Source / Stream Target");
    ImGui::Spacing();

    ImGui::RadioButton("Monitor", reinterpret_cast<int*>(&view.stream_config.capture_target), 0); 
    // ImGui::SameLine(); ImGui::RadioButton("Application", reinterpret_cast<int*>(&_stream_config.capture_target), 1);

    ImGui::RadioButton("WebSRT", reinterpret_cast<int*>(&view.stream_config.stream_target), 0); ImGui::SameLine();
    ImGui::RadioButton("Loopback", reinterpret_cast<int*>(&view.stream_config.stream_target), 1);

    const std::string combo_label = (view.stream_config.capture_target == AppViewModel::StreamConfig::CaptureTarget::DISPLAY) ? "Select Display" : "Select Window";
    const std::string preview_value = view.stream_config.capture_sources.empty() ? "None found" 
                                                                                 : view.stream_config.capture_sources[view.stream_config.selected_source_idx];
    
    if (ImGui::BeginCombo(combo_label.c_str(), preview_value.c_str()))
    {
        for (uint idx = 0; idx < view.stream_config.capture_sources.size(); idx++)
        {
            const bool is_selected = view.stream_config.selected_source_idx == idx;
            if (ImGui::Selectable(view.stream_config.capture_sources[idx].c_str(), is_selected))
                view.stream_config.selected_source_idx = idx;

            if (is_selected) 
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Refresh List"))
        if (_sources_update_callback) 
            _sources_update_callback();
}

void ApplicationUI::render_encoding_settings(AppViewModel& view)
{
    ImGui::Spacing();
    ImGui::Text("Encoding Settings");
    ImGui::SliderInt("Target FPS", reinterpret_cast<int*>(&view.stream_config.target_fps), TARGET_FPS_MIN, TARGET_FPS_MAX);
    ImGui::SliderInt("Bitrate (kbps)", reinterpret_cast<int*>(&view.stream_config.target_br_kbps), TARGET_BITRATE_MIN, TARGET_BITRATE_MAX);
}

void ApplicationUI::render_network_tx_settings(AppViewModel& view)
{
    ImGui::InputText("Stream ID", &view.network_tx.stream_id);
    ImGui::InputText("User Name", &view.network_tx.user_name);
    ImGui::InputText("User Password", &view.network_tx.user_pwd, ImGuiInputTextFlags_Password);
    ImGui::InputText("SRT Passphrase", &view.network_tx.srt_passphrase, ImGuiInputTextFlags_Password);
    ImGui::InputText("Server IP", &view.network_tx.server_ip);
    ImGui::InputInt("Server Port", reinterpret_cast<int*>(&view.network_tx.server_port));
    ImGui::SliderInt("Stream latency ms", reinterpret_cast<int*>(&view.network_tx.latency_ms), LATENCY_MS_MIN, LATENCY_MS_MAX);
}

void ApplicationUI::render_network_rx_settings(AppViewModel& view)
{
    ImGui::InputText("Stream ID", &view.network_rx.stream_id);
    ImGui::InputText("User Name", &view.network_rx.user_name);
    ImGui::InputText("User Password", &view.network_rx.user_pwd, ImGuiInputTextFlags_Password);
    ImGui::InputText("SRT Passphrase", &view.network_rx.srt_passphrase, ImGuiInputTextFlags_Password);
    ImGui::InputText("Server IP", &view.network_rx.server_ip);
    ImGui::InputInt("Server Port", reinterpret_cast<int*>(&view.network_rx.server_port));
    ImGui::SliderInt("Stream latency ms", reinterpret_cast<int*>(&view.network_rx.latency_ms), LATENCY_MS_MIN, LATENCY_MS_MAX);
    ImGui::SliderInt("Stream timeout ms", reinterpret_cast<int*>(&view.network_rx.timeout_ms), TIMEOUT_MS_MIN, TIMEOUT_MS_MAX);
}

void ApplicationUI::render_log_window(AppViewModel& view)
{
    ImGui::Separator();
    ImGui::Text("Application Logs");
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) 
        if (_clear_logs_callback) 
            _clear_logs_callback();

    ImGui::SameLine();
    if (ImGui::SmallButton("Copy All")) 
    {
        std::lock_guard<std::mutex> lock(view.logs_mutex);
        std::string clipboard_data;
        for (const auto& log : view.logs) 
            clipboard_data += log.text;
        
        ImGui::SetClipboardText(clipboard_data.c_str());
    }

    ImGui::BeginChild("LogScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); 
    
    std::lock_guard<std::mutex> lock(view.logs_mutex);
    for (const auto& log : view.logs) 
    {
        if (LogKind::NV_ERROR == log.level) 
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextUnformatted(log.text.c_str());
            ImGui::PopStyleColor();
        } 
        else 
            ImGui::TextUnformatted(log.text.c_str());
    }

    if (_scroll_to_bottom) 
    {
        ImGui::SetScrollHereY(1.0f);
        _scroll_to_bottom = false;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}