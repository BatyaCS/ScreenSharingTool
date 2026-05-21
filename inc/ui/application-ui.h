#ifndef APPLICATION_UI_H_
#define APPLICATION_UI_H_

#include <ui/video-preview-widget.h>

#include <string>
#include <vector>
#include <functional>
#include <bitset>

#include <GLFW/glfw3.h>
#include <d3d11.h>

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
        RX_CONFIG,
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

        std::string     source;

        uint            target_fps;
        uint            target_br_kbps;
    };
    struct UiNetworkConfigTx
    {
        std::string stream_id;
        std::string user_name;
        std::string user_pwd;

        std::string srt_passphrase;

        std::string server_ip;
        uint        server_port;
    };
    struct UiNetworkConfigRx
    {
        std::string stream_id;
        std::string user_name;
        std::string user_pwd;

        std::string srt_passphrase;

        std::string server_ip;
        uint        server_port;
    };

    using StartStopStreamCallback = std::function<void()>;
    using StartStopRxCallback = std::function<void()>;
    using SourcesUpdateCallback = std::function<void()>;

    ApplicationUI() = default;
    ~ApplicationUI() { shutdown(); }

    bool render();

    bool init(const UiConfig& config, const UiStreamConfig& stream_config, const UiNetworkConfigRx& config_rx, const UiNetworkConfigTx& config_tx);
    void shutdown();

    ID3D11Device* get_d3d11_device() const { return _pd3dDevice; }

    const UiNetworkConfigRx& fetch_network_rx_config() const { return _network_config_rx; }
    const UiNetworkConfigTx& fetch_network_tx_config() const { return _network_config_tx; }

    const UiStreamConfig& fetch_stream_config() const { return _stream_config; }
    void set_stream_sources(const std::vector<std::string>& sources) { _current_sources = sources; _selected_source = 0; _stream_config.source = sources[0]; }

    void set_web_texture(void * srv, uint w, uint h) { _web_srv = srv; _web_w = w; _web_h = h; }
    void set_loopback_texture(void * srv, uint w, uint h) { _loopback_srv = srv; _loopback_w = w; _loopback_h = h; }

    void set_start_stop_stream_callback(StartStopStreamCallback callback) { _start_stop_stream_callback = callback; }
    void set_start_stop_rx_callback(StartStopRxCallback callback) { _start_stop_rx_callback = callback; }
    void set_sources_update_callback(SourcesUpdateCallback callback) { _sources_update_callback = callback; }

    bool is_ui_locked(UiElement el) const { return _locked_ui.test(static_cast<size_t>(el)); }
    void set_ui_locked(UiElement el, bool is_locked) { _locked_ui.set(static_cast<size_t>(el), is_locked); }

    void log(const std::string& message);
    void log_err(const std::string& message);

private:
    struct LogEntry 
    {
        std::string text;
        bool is_error;
    };

    bool render_broadcaster_tab();
    bool render_web_preview_tab();

    void render_capture_settings();
    void render_encoding_settings();
    void render_network_tx_settings();
    void render_network_rx_settings();

    void render_log_window();

    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();

    std::string get_current_timestamp() const;

    GLFWwindow *                _window = nullptr;
    ID3D11Device *              _pd3dDevice = nullptr;
    ID3D11DeviceContext *       _pd3dDeviceContext = nullptr;
    IDXGISwapChain *            _pSwapChain = nullptr;
    ID3D11RenderTargetView *    _mainRenderTargetView = nullptr;

    VideoPreviewWidget          _web_preview_widget;
    VideoPreviewWidget          _loopback_preview_widget;

    void *                      _web_srv = nullptr;
    void *                      _loopback_srv = nullptr;

    uint                        _web_w = 0, _web_h = 0;
    uint                        _loopback_w = 0, _loopback_h = 0;

    UiStreamConfig              _stream_config;
    UiStreamConfig              _previous_stream_config;

    UiNetworkConfigRx           _network_config_rx;
    UiNetworkConfigTx           _network_config_tx;

    std::vector<std::string>    _current_sources;
    uint                        _selected_source = 0;
   
    StartStopStreamCallback     _start_stop_stream_callback;
    StartStopRxCallback         _start_stop_rx_callback;
    SourcesUpdateCallback       _sources_update_callback;

    std::bitset<static_cast<size_t>(UiElement::COUNT)> _locked_ui;

    std::vector<LogEntry>       _logs;
    bool                        _scroll_to_bottom = false;
};

#endif /* APPLICATION_UI_H_ */