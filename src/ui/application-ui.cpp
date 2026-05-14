#include <ui/application-ui.h>
#include <ui/ui-helpers.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

ApplicationUI::ApplicationUI() = default;

ApplicationUI::~ApplicationUI()
{
    shutdown();
}

bool ApplicationUI::init(const std::string& window_title, int width, int height)
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW!\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    _window = glfwCreateWindow(width, height, window_title.c_str(), nullptr, nullptr);
    if (!_window)
    {
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
    if (_video_texture != 0)
    {
        glDeleteTextures(1, &_video_texture);
        _video_texture = 0;
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

void ApplicationUI::refresh_sources()
{
    if (_on_refresh_sources)
    {
        bool is_app = (_capture_type == 1);
        _current_sources = _on_refresh_sources(is_app);
        _selected_source_idx = 0; // Reset selection to avoid out-of-bounds
    }
}

bool ApplicationUI::render_frame(const cv::Mat& current_video_frame, bool is_capturing)
{
    if (glfwWindowShouldClose(_window))
    {
        return false; 
    }

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
            render_broadcaster_tab(is_capturing);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Viewer (Loopback Test)"))
        {
            render_viewer_tab(current_video_frame);
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

void ApplicationUI::render_broadcaster_tab(bool is_capturing)
{
    ImGui::Text("Capture Source");
    ImGui::Separator();

    ImGui::BeginDisabled(is_capturing);

    ImGui::RadioButton("Monitor", &_capture_type, 0); ImGui::SameLine();
    ImGui::RadioButton("Application", &_capture_type, 1);

    // Auto-refresh lists if the user changes the capture type
    if (_capture_type != _previous_capture_type)
    {
        refresh_sources();
        _previous_capture_type = _capture_type;
    }

    // Dynamic Combo Box for Sources
    std::string combo_label = (_capture_type == 0) ? "Select Monitor" : "Select Window";
    std::string preview_value = _current_sources.empty() ? "None found" : _current_sources[_selected_source_idx].name;
    
    if (ImGui::BeginCombo(combo_label.c_str(), preview_value.c_str()))
    {
        for (int i = 0; i < _current_sources.size(); i++)
        {
            bool is_selected = (_selected_source_idx == i);
            if (ImGui::Selectable(_current_sources[i].name.c_str(), is_selected))
            {
                _selected_source_idx = i;
            }
            if (is_selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Refresh List"))
    {
        refresh_sources();
    }

    ImGui::Spacing();
    ImGui::Text("Encoding Settings");
    ImGui::Separator();

    ImGui::SliderInt("Target FPS", &_target_fps, 10, 60);
    ImGui::SliderInt("Bitrate (kbps)", &_target_bitrate, 500, 10000);

    ImGui::EndDisabled();

    ImGui::Spacing();
    
    if (!is_capturing)
    {
        if (ImGui::Button("Start Stream", ImVec2(150, 40)))
        {
            UiStreamSettings settings;
            settings.target_fps = _target_fps;
            settings.bitrate_kbps = _target_bitrate;
            settings.is_application = (_capture_type == 1);
            settings.selected_handle = nullptr;

            if (!_current_sources.empty())
            {
                settings.selected_handle = _current_sources[_selected_source_idx].handle;
            }

            if (_on_start) _on_start(settings);
        }
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Stop Stream", ImVec2(150, 40)))
        {
            if (_on_stop) _on_stop();
        }
        ImGui::PopStyleColor(2);
    }
}

void ApplicationUI::render_viewer_tab(const cv::Mat& frame)
{
    if (!frame.empty())
    {
        _video_texture = mat_to_texture(frame, _video_texture);
    }

    if (_video_texture != 0)
    {
        float aspect_ratio = (float)frame.cols / (float)frame.rows;
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

        ImGui::Image((ImTextureID)(intptr_t)_video_texture, ImVec2(draw_width, draw_height));
    }
    else
    {
        ImGui::Text("Waiting for video stream...");
    }
}