#include <common.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <srt/srt.h>

#include <network/srt-receiver.h>

SrtReceiver::SrtReceiver()
    : _socket(SRT_INVALID_SOCK)
{}

bool SrtReceiver::is_connected() const 
{ 
    return _socket != SRT_INVALID_SOCK; 
}

bool SrtReceiver::open_connection(const NetworkConfig& config) 
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
    srt_setsockopt(_socket, 0, SRTO_RCVBUF, &buf_size, sizeof(buf_size));

    const int pkt_drop = static_cast<int>(config.packet_drop);
    srt_setsockopt(_socket, 0, SRTO_TLPKTDROP, &pkt_drop, sizeof(pkt_drop));

    const int latency_ms = static_cast<int>(config.stream_latency_ms);
    srt_setsockopt(_socket, 0, SRTO_LATENCY, &latency_ms, sizeof(latency_ms));

    const int rcv_timeout_ms = config.receive_timeout_ms;
    srt_setsockopt(_socket, 0, SRTO_RCVTIMEO, &rcv_timeout_ms, sizeof(rcv_timeout_ms));
    
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

    LOG("SRT Connection opened to %s:%u for receiving!\n", config.ip.c_str(), config.port);
    return true;
}

bool SrtReceiver::close_connection()
{
    if (_socket == SRT_INVALID_SOCK)
        return true;

    if (SRT_ERROR == srt_close(_socket))
    {
        LOG_ERROR("Failed to close SRT connection on socket\n");
        return false;
    }

    _socket = SRT_INVALID_SOCK;
    return true;
}

bool SrtReceiver::receive(std::vector<uint8_t>& out_data) 
{
    static constexpr size_t MAX_RECEIVE_SIZE_BYTES = 8192;

    std::lock_guard<std::mutex> lock(_receive_mutex);

    if (SRT_INVALID_SOCK == _socket)
    {
        LOG_ERROR("Failed to receive SRT data, socket isn't connected!\n");
        return false;
    }

    out_data.resize(MAX_RECEIVE_SIZE_BYTES);
    const int bytes_received = srt_recvmsg(_socket,  
                                           reinterpret_cast<char*>(out_data.data()), 
                                           static_cast<int>(MAX_RECEIVE_SIZE_BYTES));
    
    if (SRT_ERROR == bytes_received) 
    {
        LOG_ERROR("Failed to receive SRT chunk! Error: %s\n", srt_getlasterror_str());
        out_data.clear();
        return false;
    }

    out_data.resize(static_cast<size_t>(bytes_received));    
    return true;
}