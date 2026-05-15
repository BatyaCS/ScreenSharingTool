#ifndef APPLICATION_UI_H_
#define APPLICATION_UI_H_

#include <string>
#include <vector>
#include <functional>
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>

class ApplicationUI
{
    static constexpr uint TARGET_FPS_MIN = 10;
    static constexpr uint TARGET_FPS_MAX = 60;

    static constexpr uint TARGET_BITRATE_MIN = 500;
    static constexpr uint TARGET_BITRATE_MAX = 10000;

public:
    struct UiConfig
    {
        std::string window_title;

        uint width;
        uint height;
    };
    struct UiStreamConfig
    {
        enum class CaptureTarget { DISPLAY, WINDOW };
        enum class StreamTarget { WEB, LOOPBACK };

        CaptureTarget   capture_target;
        StreamTarget    stream_target;

        uint            target_fps;
        uint            target_br_kbps;

        uint            source_idx = 0;
    };
    struct UiNetworkConfigTx
    {
        std::string stream_id = "mystream";
        std::string stream_pwd = "strongpwd";

        std::string server_ip;
        uint        server_port;
    };
    struct UiNetworkConfigRx
    {
        std::string stream_id = "mystream";
        std::string stream_pwd = "strongpwd";

        std::string server_ip;
        uint        server_port;
    };

    using StartStreamCallback = std::function<void(const UiStreamConfig&)>;
    using StopStreamCallback = std::function<void()>;

    using StartWebPreviewCallback = std::function<void(const UiStreamConfig&)>;
    using StopWebPreviewCallback = std::function<void()>;

    using RefreshSourcesCallback = std::function<std::vector<std::string>(UiStreamConfig::CaptureTarget target)>;

    ApplicationUI() = default;
    ~ApplicationUI() { shutdown(); }

    bool init(const UiConfig& config, const UiStreamConfig& stream_settings);
    void shutdown();

    bool render();
    bool refresh_sources();

    void set_web_frame_mem(const cv::Mat& frame) { _web_frame = &frame; }
    void set_loopback_frame_mem(const cv::Mat& frame) { _loopback_frame = &frame; }

    void set_start_stream_callback(StartStreamCallback callback) { _on_start_stream = callback; }
    void set_stop_stream_callback(StopStreamCallback callback) { _on_stop_stream = callback; }

    void set_start_preview_callback(StartWebPreviewCallback callback) { _on_start_preview = callback; }
    void set_stop_preview_callback(StopWebPreviewCallback callback) { _on_stop_preview = callback; }

    void set_refresh_sources_callback(RefreshSourcesCallback callback) { _on_refresh_sources = callback; }

private:
    struct UiState
    {
        bool is_streaming_enabled = false;
        bool is_web_preview_enabled = false;
    };

    bool render_broadcaster_tab();
    bool render_web_preview_tab();
    bool render_loopback_preview_tab();

    GLFWwindow *            _window = nullptr;
    GLuint                  _web_frame_texture = 0;
    GLuint                  _loopback_frame_texture = 0;

    const cv::Mat *         _web_frame = nullptr;
    const cv::Mat *         _loopback_frame = nullptr;

    StartStreamCallback     _on_start_stream;
    StopStreamCallback      _on_stop_stream;
    StartWebPreviewCallback _on_start_preview;
    StopWebPreviewCallback  _on_stop_preview;
    RefreshSourcesCallback  _on_refresh_sources;

    UiStreamConfig          _stream_config;
    UiStreamConfig          _previous_stream_config;

    UiState                 _ui_state;

    std::vector<std::string> _current_sources;
};

#endif /* APPLICATION_UI_H_ */