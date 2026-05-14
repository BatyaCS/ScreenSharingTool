#ifndef APPLICATION_UI_H_
#define APPLICATION_UI_H_

#include <string>
#include <vector>
#include <functional>
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>

// --- Data Transfer Objects (DTO) ---

// Generic structure to hold a capture source (Monitor or Window)
struct UiSourceItem
{
    void* handle; // Opaque pointer to HWND, HMONITOR, etc.
    std::string name;
};

// Generic structure to pass stream settings back to the main application
struct UiStreamSettings
{
    int target_fps;
    int bitrate_kbps;
    bool is_application; // true = Window, false = Monitor
    void* selected_handle;
};

// --- Callbacks ---

using StartCallback = std::function<void(const UiStreamSettings&)>;
using StopCallback = std::function<void()>;
// Takes a boolean (is_application) and returns a list of generic items
using RefreshSourcesCallback = std::function<std::vector<UiSourceItem>(bool is_application)>;

// --- UI Class ---

class ApplicationUI
{
public:
    ApplicationUI();
    ~ApplicationUI();

    bool init(const std::string& window_title, int width, int height);
    void shutdown();

    // Renders the UI frame. Returns false if the window was closed.
    bool render_frame(const cv::Mat& current_video_frame, bool is_capturing);

    // Setters for controller callbacks
    void set_start_callback(StartCallback cb) { _on_start = cb; }
    void set_stop_callback(StopCallback cb) { _on_stop = cb; }
    void set_refresh_sources_callback(RefreshSourcesCallback cb) { _on_refresh_sources = cb; }

    // Forces the UI to fetch the latest list of sources from the backend
    void refresh_sources();

private:
    void render_broadcaster_tab(bool is_capturing);
    void render_viewer_tab(const cv::Mat& frame);

    GLFWwindow* _window = nullptr;
    GLuint      _video_texture = 0;

    // Callbacks
    StartCallback          _on_start;
    StopCallback           _on_stop;
    RefreshSourcesCallback _on_refresh_sources;

    // UI State
    std::vector<UiSourceItem> _current_sources;
    
    int  _target_fps = 30;
    int  _target_bitrate = 2500;
    int  _selected_source_idx = 0;
    int  _capture_type = 0; // 0 = Monitor, 1 = Application
    int  _previous_capture_type = 0; // Used to detect radio button changes
};

#endif /* APPLICATION_UI_H_ */