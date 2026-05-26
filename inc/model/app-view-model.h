#ifndef APP_VIEW_MODEL_H_
#define APP_VIEW_MODEL_H_

#include <model/app-models.h>
#include <graphics/texture-buffer.h>
#include <vector>

#include <mutex>
#include <atomic>

class AppViewModel 
{
public:
    using StreamConfig = AppModels::StreamConfig;
    using NetworkConfigTx = AppModels::NetworkConfigTx;
    using NetworkConfigRx = AppModels::NetworkConfigRx;
    using Logs = std::vector<AppModels::LogEntry>;

    // probably need to be defined somewhere else
    struct FrameSize 
    {
        uint32_t width = 0;
        uint32_t height = 0;
    };

    StreamConfig            stream_config{};

    NetworkConfigTx         network_tx{};
    NetworkConfigRx         network_rx{};

    Logs                    logs;
    std::mutex              logs_mutex;

    TextureBuffer           loopback_texture;
    TextureBuffer           preview_texture;

    std::atomic<FrameSize>  loopback_frame_size;
    std::atomic<FrameSize>  preview_frame_size;
    
    std::atomic<bool>   is_broadcasting{false};
    std::atomic<bool>   is_watching{false};
};

#endif /* APP_VIEW_MODEL_H_ */