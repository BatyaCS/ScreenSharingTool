#ifndef SRT_RECEIVER_H_
#define SRT_RECEIVER_H_

#include <string>
#include <vector>
#include <mutex>

#include <network/srt-defs.h>
#include <utils/non-copyable.h>

class SrtReceiver : NonCopyable
{
public:
    struct NetworkConfig
    {
        SrtEncryption   encryption = SrtEncryption::AES_128;
        SrtLatePktDrop  packet_drop = SrtLatePktDrop::ENABLE;

        std::string     stream_id;
        std::string     pass_phrase;

        std::string     ip;
        uint            port;

        static constexpr size_t buffer_size = 10 * 1024 * 1024;
        static constexpr uint stream_latency_ms = 300;
        static constexpr uint receive_timeout_ms = 100;
    };

    SrtReceiver();
    ~SrtReceiver() { close_connection(); }

    bool is_connected() const;

    bool open_connection(const NetworkConfig& config);
    bool close_connection();

    bool receive(std::vector<uint8_t>& out_data);

private:
    using SRTSOCKET = int32_t;

    SRTSOCKET _socket;

    std::mutex _receive_mutex;
};

#endif /* SRT_RECEIVER_H_ */