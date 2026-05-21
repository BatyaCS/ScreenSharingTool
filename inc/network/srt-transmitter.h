#ifndef SRT_TRANSMITTER_H_
#define SRT_TRANSMITTER_H_

#include <string>
#include <vector>
#include <mutex>

#include <network/srt-defs.h>
#include <utils/non-copyable.h>

class SrtTransmitter : NonCopyable
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
    };

    SrtTransmitter();
    ~SrtTransmitter() { close_connection(); }

    bool is_connected() const;

    bool open_connection(const NetworkConfig& config);
    bool close_connection();

    bool send(const std::vector<uint8_t>& data);

private:
    using SRTSOCKET = int32_t;

    SRTSOCKET _socket;

    std::mutex _send_mutex;
};

#endif /* SRT_TRANSMITTER_H_ */