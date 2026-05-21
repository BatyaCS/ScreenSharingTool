#include <common.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <srt/srt.h>

#include <network/srt-transmitter.h>
#include <algorithm>

SrtTransmitter::SrtTransmitter()
    : _socket(SRT_INVALID_SOCK)
{}

bool SrtTransmitter::is_connected() const 
{ 
    return _socket != SRT_INVALID_SOCK; 
}

bool SrtTransmitter::open_connection(const NetworkConfig& config) 
{
    close_connection();

    _socket = srt_create_socket();
    if (_socket == SRT_INVALID_SOCK)
    {
        LOG_ERROR("Failed to create SRT socket to open connection!\n");
        return false;
    }

    srt_setsockopt(_socket, 0, SRTO_STREAMID, config.stream_id.c_str(), static_cast<int>(config.stream_id.length()));
    
    if (!config.pass_phrase.empty()) 
    {
        static constexpr size_t PWD_MIN_LEN = 10;
        static constexpr size_t PWD_MAX_LEN = 79;

        const int pass_phrase_len = static_cast<int>(config.pass_phrase.length());
        if (pass_phrase_len < PWD_MIN_LEN || pass_phrase_len > PWD_MAX_LEN)
        {
            LOG_ERROR("SRT passphrase must be between %llu and %llu characters!\n", PWD_MIN_LEN, PWD_MAX_LEN);
            return false;
        }
        
        const int key_len = static_cast<int>(config.encryption);
        srt_setsockopt(_socket, 0, SRTO_PBKEYLEN, &key_len, sizeof(key_len));
        srt_setsockopt(_socket, 0, SRTO_PASSPHRASE, config.pass_phrase.c_str(), pass_phrase_len);
    }

    const int buf_size = static_cast<int>(config.buffer_size);
    srt_setsockopt(_socket, 0, SRTO_SNDBUF, &buf_size, sizeof(buf_size));

    const int pkt_drop = static_cast<int>(config.packet_drop);
    srt_setsockopt(_socket, 0, SRTO_TLPKTDROP, &pkt_drop, sizeof(pkt_drop));

    const int latency_ms = static_cast<int>(config.stream_latency_ms);
    srt_setsockopt(_socket, 0, SRTO_LATENCY, &latency_ms, sizeof(latency_ms));
    
    sockaddr_in sa{};
        
    sa.sin_family = AF_INET;
    sa.sin_port = htons(config.port);
    inet_pton(AF_INET, config.ip.c_str(), &sa.sin_addr);

    if (SRT_ERROR == srt_connect(_socket, (struct sockaddr*)&sa, sizeof(sa))) 
    {
        LOG_ERROR("Failed to init SRT connect! Error: %s\n", srt_getlasterror_str());
        
        close_connection();
        return false;
    }

    LOG("SRT Connection opened to %s:%u!\n", config.ip.c_str(), config.port);
    return true;
}

bool SrtTransmitter::close_connection()
{
    if (_socket == SRT_INVALID_SOCK)
        return true;

    if (SRT_ERROR == srt_close(_socket))
    {
        LOG_ERROR("Failed to close SRT connection on socket %d\n");
        return false;
    }

    _socket = SRT_INVALID_SOCK;
    return true;
}

bool SrtTransmitter::send(const std::vector<uint8_t>& data) 
{
    static constexpr size_t MAX_PAYLOAD_SIZE_BYTES = 1316;
    // TODO: Might also be good to add check is bytes cout divisable by 188

    std::lock_guard<std::mutex> lock(_send_mutex);

    if (SRT_INVALID_SOCK == _socket)
    {
        LOG_ERROR("Failed to send SRT data, socket isn't connected!\n");
        return false;
    }

    const size_t bytes_to_send = data.size();
    size_t bytes_sent = 0;
    
    while (bytes_sent < bytes_to_send) 
    {
        const size_t bytes_left = bytes_to_send - bytes_sent;
        const size_t bytes_to_send = std::min(MAX_PAYLOAD_SIZE_BYTES, bytes_left);
        
        const int res = srt_sendmsg(_socket,  
                            reinterpret_cast<const char*>(&data.at(bytes_sent)), 
                            static_cast<int>(bytes_to_send),
                            static_cast<int>(SrtMessageTtl::TTL_INFINITE), 
                            static_cast<int>(SrtDeliveryOrder::OUT_OF_ORDER));
        
        if (SRT_ERROR == res) 
        {
            LOG_ERROR("Failed to send SRT chunk! Error: %s\n", srt_getlasterror_str());
            return false;
        }
        
        bytes_sent += bytes_to_send;
    }

    return true;
}
