#ifndef APPLICATION_UI_H_
#define APPLICATION_UI_H_

#include <string>
#include <vector>
#include <functional>
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>
#include <bitset>

class ApplicationUI
{
    static constexpr uint TARGET_FPS_MIN = 10;
    static constexpr uint TARGET_FPS_MAX = 60;

    static constexpr uint TARGET_BITRATE_MIN = 500;
    static constexpr uint TARGET_BITRATE_MAX = 10000;

public:
    enum class UiElement : size_t 
    {
        STREAM_CONFIG,
        PREVIEW_CONFIG,

        COUNT
    };
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

    using StartStopStreamCallback = std::function<void()>;
    using SourcesUpdateCallback = std::function<void()>;

    ApplicationUI() = default;
    ~ApplicationUI() { shutdown(); }

    bool render();

    bool init(const UiConfig& config, const UiStreamConfig& stream_config, const UiNetworkConfigRx& config_rx, const UiNetworkConfigTx& config_tx);
    void shutdown();

    const UiStreamConfig& fetch_stream_config() const { return _stream_config; }

    const UiNetworkConfigRx& fetch_network_rx_config() const { return _network_config_rx; }
    const UiNetworkConfigTx& fetch_network_tx_config() const { return _network_config_tx; }

    void set_stream_sources(const std::vector<std::string>& sources) { _current_sources = sources; }

    void set_web_frame_mem(const cv::Mat& frame) { _web_frame = &frame; }
    void set_loopback_frame_mem(const cv::Mat& frame) { _loopback_frame = &frame; }

    void set_start_stop_stream_callback(StartStopStreamCallback callback) { _start_stop_stream_callback = callback; }
    void set_sources_update_callback(SourcesUpdateCallback callback) { _sources_update_callback = callback; }

    bool is_ui_locked(UiElement el) const { return _locked_ui.test(static_cast<size_t>(el)); }
    void set_ui_locked(UiElement el, bool is_locked) { _locked_ui.set(static_cast<size_t>(el), is_locked); }

private:
    bool render_broadcaster_tab();
    bool render_web_preview_tab();
    bool render_loopback_preview_tab();

    GLFWwindow *    _window = nullptr;
    GLuint          _web_frame_texture = 0;
    GLuint          _loopback_frame_texture = 0;

    const cv::Mat * _web_frame = nullptr;
    const cv::Mat * _loopback_frame = nullptr;

    UiStreamConfig  _stream_config;
    UiStreamConfig  _previous_stream_config;

    UiNetworkConfigRx   _network_config_rx;
    UiNetworkConfigTx   _network_config_tx;

    std::vector<std::string>    _current_sources;

    StartStopStreamCallback     _start_stop_stream_callback;
    SourcesUpdateCallback       _sources_update_callback;

    std::bitset<static_cast<size_t>(UiElement::COUNT)> _locked_ui;
};

#endif /* APPLICATION_UI_H_ */