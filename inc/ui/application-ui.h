#ifndef APPLICATION_UI_H_
#define APPLICATION_UI_H_

#include <ui/video-preview-widget.h>
#include <model/app-view-model.h>
#include <functional>

struct GLFWwindow;
class GraphicsContext;

class ApplicationUI
{
    static constexpr uint TARGET_FPS_MIN = 10;
    static constexpr uint TARGET_FPS_MAX = 60;

    static constexpr uint TARGET_BITRATE_MIN = 500;
    static constexpr uint TARGET_BITRATE_MAX = 10000;

    using StreamConfig = AppModels::StreamConfig;

public:
    using StartStopStreamCallback = std::function<void()>;
    using StartStopRxCallback = std::function<void()>;
    using SourcesUpdateCallback = std::function<void()>;
    using ClearLogsCallback = std::function<void()>;

    ApplicationUI() = default;
    ~ApplicationUI() { shutdown(); }

    bool render(AppViewModel& view);

    bool init(GLFWwindow * window, GraphicsContext * gfx);
    void shutdown();

    void set_start_stop_stream_callback(StartStopStreamCallback callback) { _start_stop_stream_callback = callback; }
    void set_start_stop_rx_callback(StartStopRxCallback callback) { _start_stop_rx_callback = callback; }
    void set_sources_update_callback(SourcesUpdateCallback callback) { _sources_update_callback = callback; }
    void set_clear_logs_callback(ClearLogsCallback callback) { _clear_logs_callback = callback; }

private:
    bool render_broadcaster_tab(AppViewModel& view);
    bool render_web_preview_tab(AppViewModel& view);

    void render_capture_settings(AppViewModel& view);
    void render_encoding_settings(AppViewModel& view);
    void render_network_tx_settings(AppViewModel& view);
    void render_network_rx_settings(AppViewModel& view);

    void render_log_window(AppViewModel& view);

    GLFWwindow *                _window = nullptr;
    GraphicsContext *           _gfx = nullptr;

    VideoPreviewWidget          _web_preview_widget;
    VideoPreviewWidget          _loopback_preview_widget;
   
    StartStopStreamCallback     _start_stop_stream_callback;
    StartStopRxCallback         _start_stop_rx_callback;
    SourcesUpdateCallback       _sources_update_callback;
    ClearLogsCallback           _clear_logs_callback;

    bool                        _scroll_to_bottom = false;
};

#endif /* APPLICATION_UI_H_ */