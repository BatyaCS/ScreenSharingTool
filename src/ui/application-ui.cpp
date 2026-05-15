#include <common.h>
#include <ui/application-ui.h>
#include <ui/ui-helpers.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

bool ApplicationUI::init(const UiConfig& config, const UiStreamConfig& stream_settings)
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

bool ApplicationUI::refresh_sources()
{
    if (!_on_refresh_sources)
        return false;

    _current_sources = _on_refresh_sources(_stream_config.capture_target);
    _stream_config.source_idx = 0;
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
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | 
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | 
                                    ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("MainLayout", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    if (ImGui::BeginTabBar("MainTabs"))
    {
        if (ImGui::BeginTabItem("Broadcaster"))
        {
            render_broadcaster_tab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Viewer (Web)"))
        {
            render_web_preview_tab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Viewer (Loopback)"))
        {
            render_loopback_preview_tab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

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
    ImGui::Text("Capture Source / Stream Target");
    ImGui::Separator();

    ImGui::BeginDisabled(_ui_state.is_streaming_enabled);

    ImGui::RadioButton("Monitor", reinterpret_cast<int*>(&_stream_config.capture_target), 0); ImGui::SameLine();
    ImGui::RadioButton("Application", reinterpret_cast<int*>(&_stream_config.capture_target), 1);

    ImGui::RadioButton("WebSRT", reinterpret_cast<int*>(&_stream_config.stream_target), 0); ImGui::SameLine();
    ImGui::RadioButton("Loopback", reinterpret_cast<int*>(&_stream_config.stream_target), 1);

    if (_stream_config.capture_target != _previous_stream_config.capture_target)
    {
        refresh_sources();
        _previous_stream_config = _stream_config;
    }

    // Dynamic Combo Box for Sources
    const std::string combo_label = (_stream_config.capture_target == UiStreamConfig::CaptureTarget::DISPLAY) ? "Select Display" : "Select Window";
    const std::string preview_value = _current_sources.empty() ? "None found" : _current_sources[_stream_config.source_idx];
    
    if (ImGui::BeginCombo(combo_label.c_str(), preview_value.c_str()))
    {
        for (uint idx = 0; idx < _current_sources.size(); idx++)
        {
            const bool is_selected = _stream_config.source_idx == idx;

            if (ImGui::Selectable(_current_sources[idx].c_str(), is_selected))
                _stream_config.source_idx = idx;

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (ImGui::Button("Refresh List"))
        refresh_sources();

    ImGui::Spacing();
    ImGui::Text("Encoding Settings");
    ImGui::Separator();

    ImGui::SliderInt("Target FPS", reinterpret_cast<int*>(&_stream_config.target_fps), TARGET_FPS_MIN, TARGET_FPS_MAX);
    ImGui::SliderInt("Bitrate (kbps)", reinterpret_cast<int*>(&_stream_config.target_br_kbps), TARGET_BITRATE_MIN, TARGET_BITRATE_MAX);

    ImGui::EndDisabled();

    ImGui::Spacing();
    
    if (!_ui_state.is_streaming_enabled)
    {
        if (ImGui::Button("Start Stream", ImVec2(150, 40)))
            if (_on_start_stream) 
                _on_start_stream(_stream_config);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Stop Stream", ImVec2(150, 40)))
        {
            if (_on_stop_stream) 
                _on_stop_stream();
        }

        ImGui::PopStyleColor(2);
    }

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