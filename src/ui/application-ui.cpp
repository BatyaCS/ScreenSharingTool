#include <common.h>
#include <ui/application-ui.h>
#include <ui/ui-helpers.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

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
        std::cerr << "Failed to initialize GLFW!\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    _window = glfwCreateWindow(config.width, config.height, config.window_title.c_str(), nullptr, nullptr);
    if (!_window)
    {
        std::cerr << "Failed to create glfw window!\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(_window);
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    _stream_config = stream_config;
    _previous_stream_config = stream_config;

    _network_config_rx = config_rx;
    _network_config_tx = config_tx;

    return true;
}

void ApplicationUI::shutdown()
{
    if (_web_frame_texture != 0)
    {
        glDeleteTextures(1, &_web_frame_texture);
        _web_frame_texture = 0;
    }

    if (_loopback_frame_texture != 0)
    {
        glDeleteTextures(1, &_loopback_frame_texture);
        _loopback_frame_texture = 0;
    }

    if (_window)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

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
    ImGui_ImplOpenGL3_NewFrame();
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

    float log_height = 150.0f;
    ImGui::BeginChild("TabsRegion", ImVec2(0, -log_height), true);
    if (ImGui::BeginTabBar("MainTabs")) 
    {
        if (ImGui::BeginTabItem("Broadcaster")) { render_broadcaster_tab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Viewer (Web)")) { render_web_preview_tab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Viewer (Loopback)")) { render_loopback_preview_tab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    
    ImGui::EndChild();

    render_log_window();

    ImGui::End();
    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(_window);

    return true;
}

bool ApplicationUI::render_broadcaster_tab()
{
    ImGui::BeginChild("SettingsRegion", ImVec2(0, -60), true);

    if (ImGui::CollapsingHeader("Capture & Encoding Settings", ImGuiTreeNodeFlags_DefaultOpen))
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

    if (ImGui::CollapsingHeader("Network Receiver (Rx)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(is_ui_locked(UiElement::RX_CONFIG));
        render_network_rx_settings();
        ImGui::EndDisabled();
    }

    ImGui::EndChild();

    render_action_buttons();

    return true;
}

bool ApplicationUI::render_web_preview_tab()
{
    if (!_web_frame)
        return false;

    if (!_web_frame->empty())
        _web_frame_texture = mat_to_texture(*_web_frame, _web_frame_texture);
    
    if (_web_frame_texture != 0)
    {
        float aspect_ratio = (float)_web_frame->cols / (float)_web_frame->rows;
        ImVec2 avail_size = ImGui::GetContentRegionAvail();
        
        float draw_width = avail_size.x;
        float draw_height = draw_width / aspect_ratio;

        if (draw_height > avail_size.y)
        {
            draw_height = avail_size.y;
            draw_width = draw_height * aspect_ratio;
        }

        ImVec2 cursor_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPosX(cursor_pos.x + (avail_size.x - draw_width) * 0.5f);
        ImGui::SetCursorPosY(cursor_pos.y + (avail_size.y - draw_height) * 0.5f);

        ImGui::Image((ImTextureID)(intptr_t)_web_frame_texture, ImVec2(draw_width, draw_height));
    }
    else
        ImGui::Text("Waiting for video stream...");

    return true;
}

bool ApplicationUI::render_loopback_preview_tab()
{
    if (!_loopback_frame)
        return false;

    if (!_loopback_frame->empty())
        _loopback_frame_texture = mat_to_texture(*_loopback_frame, _loopback_frame_texture);
    
    if (_loopback_frame_texture != 0)
    {
        float aspect_ratio = (float)_loopback_frame->cols / (float)_loopback_frame->rows;
        ImVec2 avail_size = ImGui::GetContentRegionAvail();
        
        float draw_width = avail_size.x;
        float draw_height = draw_width / aspect_ratio;

        if (draw_height > avail_size.y)
        {
            draw_height = avail_size.y;
            draw_width = draw_height * aspect_ratio;
        }

        ImVec2 cursor_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPosX(cursor_pos.x + (avail_size.x - draw_width) * 0.5f);
        ImGui::SetCursorPosY(cursor_pos.y + (avail_size.y - draw_height) * 0.5f);

        ImGui::Image((ImTextureID)(intptr_t)_loopback_frame_texture, ImVec2(draw_width, draw_height));
    }
    else
        ImGui::Text("Waiting for video stream...");

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
    InputTextString("Server IP##tx", _network_config_tx.server_ip);
    ImGui::InputInt("Server Port##tx", reinterpret_cast<int*>(&_network_config_tx.server_port));
    InputTextString("Stream ID##tx", _network_config_tx.stream_id);
    InputTextString("Password##tx", _network_config_tx.stream_pwd, ImGuiInputTextFlags_Password);
}

void ApplicationUI::render_network_rx_settings()
{
    InputTextString("Server IP##rx", _network_config_rx.server_ip);
    ImGui::InputInt("Server Port##rx", reinterpret_cast<int*>(&_network_config_rx.server_port));
    InputTextString("Stream ID##rx", _network_config_rx.stream_id);
    InputTextString("Password##rx", _network_config_rx.stream_pwd, ImGuiInputTextFlags_Password);
}

void ApplicationUI::render_action_buttons()
{
    if (!is_ui_locked(UiElement::STREAM_CONFIG))
    {
        if (ImGui::Button("Start Broadcast (Tx)", ImVec2(180, 40)))
            if (_start_stop_stream_callback) 
                _start_stop_stream_callback();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Stop Broadcast", ImVec2(180, 40)))
            if (_start_stop_stream_callback) 
                _start_stop_stream_callback();

        ImGui::PopStyleColor(2);
    }

    ImGui::SameLine();

    if (!is_ui_locked(UiElement::RX_CONFIG))
    {
        if (ImGui::Button("Start Preview (Rx)", ImVec2(180, 40)))
            if (_start_stop_rx_callback) 
                _start_stop_rx_callback();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Stop Preview", ImVec2(180, 40)))
            if (_start_stop_rx_callback) 
                _start_stop_rx_callback();

        ImGui::PopStyleColor(2);
    }
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