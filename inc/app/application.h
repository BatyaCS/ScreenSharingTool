#ifndef APPLICATION_H_
#define APPLICATION_H_

#include <ui/application-ui.h>
#include <model/app-view-model.h>
#include <GLFW/glfw3.h>

#include <graphics/graphics-context.h>
#include <graphics/cross-device-bridge.h>
#include <graphics/yuv-rgb-converter.h>

#include <network/srt-receiver.h>
#include <network/srt-transmitter.h>

#include <capturer/hw-video-capturer.h>
#include <encoder/hw-stream-encoder.h>
#include <decoder/hw-stream-decoder.h>

#include <mutex>
#include <fstream>
#include <vector>

class Application
{
public:
    Application() = default;
    ~Application() { cleanup(); }

    bool init();
    void cleanup();
    void run();

    AppViewModel& get_view_model() { return _model; }

private:
    void render_window();

    bool start_streaming();
    void stop_streaming();

    bool start_preview();
    void stop_preview();

    void handle_frame_captured(ID3D11Texture2D* tex, ID3D11Device* dev);
    void handle_frame_received(ID3D11Texture2D* tex, ID3D11Device* dev, uint slice_index);

    void save_frame_for_loopback(ID3D11Texture2D* tex, ID3D11Device* dev);

    // UI Handlers
    void handle_start_stop_stream();
    void handle_start_stop_preview();
    void handle_sources_update();
    void handle_logs_clear_request();

    void srt_rx_loop();

    static SrtReceiver::NetworkConfig to_srt_network_cfg(const AppModels::NetworkConfigRx& cfg);
    static SrtTransmitter::NetworkConfig to_srt_network_cfg(const AppModels::NetworkConfigTx& cfg);

    GLFWwindow *                _window = nullptr;
    AppViewModel                _model;
    GraphicsContext             _gfx;
    ApplicationUI               _ui;

    CrossDeviceTextureBridge    _gfx_bridge;
    YuvRgbConverter             _yuv_rgb_converter;

    HwVideoCapturer             _capturer;    
    HwStreamEncoder             _encoder;
    HwStreamDecoder             _decoder;
    
    SrtTransmitter              _srt_sender;
    SrtReceiver                 _srt_receiver;

    std::thread                 _rx_thread;
    std::atomic<bool>           _is_rx_running{false};

    // TODO: GLFW using ints for window sizes, prabably ok to leave this way
    int                         _window_width = 0;
    int                         _window_height = 0;
};

#endif /* APPLICATION_H_ */