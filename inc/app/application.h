#ifndef APPLICATION_H_
#define APPLICATION_H_

#include <ui/application-ui.h>

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

private:
    using StreamTarget = ApplicationUI::UiStreamConfig::StreamTarget;

    bool start_streaming();
    void stop_streaming();

    bool start_preview();
    void stop_preview();

    void handle_frame_captured(ID3D11Texture2D* tex, ID3D11Device* dev);
    void handle_frame_received(ID3D11Texture2D* tex, ID3D11Device* dev);

    void save_frame_for_loopback(ID3D11Texture2D* tex, ID3D11Device* dev);

    // UI Handlers
    void handle_start_stop_stream();
    void handle_start_stop_preview();
    void handle_sources_update();

    void srt_rx_loop();

    ApplicationUI   _ui;

    HwVideoCapturer _capturer;    
    HwStreamEncoder _encoder;
    HwStreamDecoder _decoder;
    
    SrtTransmitter  _srt_sender;
    SrtReceiver     _srt_receiver;

    std::mutex      _loopback_mutex;
    uint            _loopback_w = 0, _loopback_h = 0;

    std::mutex      _preview_mutex;
    uint            _preview_w = 0, _preview_h = 0;

    ID3D11ShaderResourceView * _current_loopback_srv = nullptr;
    ID3D11ShaderResourceView * _loopback_srv_to_release = nullptr;

    ID3D11ShaderResourceView * _current_preview_srv = nullptr;
    ID3D11ShaderResourceView * _preview_srv_to_release = nullptr;

    std::thread       _rx_thread;
    std::atomic<bool> _is_rx_running{false};

    bool            _is_stream_enabled = false;
    bool            _is_preview_enabled = false;
};

#endif /* APPLICATION_H_ */