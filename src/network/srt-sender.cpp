#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <srt/srt.h>

#include <network/srt-sender.h>

#include <iostream>
#include <algorithm>

SrtSender::SrtSender() 
    : _socket(SRT_INVALID_SOCK)
{
    srt_startup();
}

SrtSender::~SrtSender() {
    stop();
    srt_cleanup();
}

bool SrtSender::init(const std::string& ip, uint16_t port, const std::string& stream_id, const std::string& password) {
    stop();

    _socket = srt_create_socket();
    if (_socket == SRT_INVALID_SOCK) return false;

    // 1. Устанавливаем StreamID — это критично для MediaMTX
    srt_setsockopt(_socket, 0, SRTO_STREAMID, stream_id.c_str(), (int)stream_id.length());

    // 2. Настройка безопасности (AES Encryption)
    if (!password.empty()) {
        int key_len = 16; // 16 байт = AES-128. Можно 24 (192) или 32 (256)
        srt_setsockopt(_socket, 0, SRTO_PBKEYLEN, &key_len, sizeof(int));
        srt_setsockopt(_socket, 0, SRTO_PASSPHRASE, password.c_str(), (int)password.length());
    }

    // 1. Увеличиваем буферы сокета (в байтах)
    int buf_size = 10 * 1024 * 1024; // 10 MB
    srt_setsockopt(_socket, 0, SRTO_SNDBUF, &buf_size, sizeof(int));

    // 2. Включаем Too Late Packet Drop (Критично для Live)
    // Если пакет не ушел вовремя, SRT его просто пропустит, а не будет вешать поток
    int drop_late = 1;
    srt_setsockopt(_socket, 0, SRTO_TLPKTDROP, &drop_late, sizeof(int));

    // // 3. Увеличиваем полетное окно (Flight Flag Size)
    // int flight_size = 50000; 
    // srt_setsockopt(_socket, 0, SRTO_FLIGHTSIZE, &flight_size, sizeof(int));

    // 3. Оптимизация для Real-time видео
    int latency = 300; // мс. Время на переповтор потерянных пакетов
    srt_setsockopt(_socket, 0, SRTO_LATENCY, &latency, sizeof(int));
    
    // int no_delay = 1; // Отключаем алгоритм Нагла для мгновенной отправки
    // srt_setsockopt(_socket, 0, SRTO_SNDNODELAY, &no_delay, sizeof(int));

    // 4. Подготовка адреса
    sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);

    // 5. Соединение (в режиме Caller)
    if (srt_connect(_socket, (struct sockaddr*)&sa, sizeof(sa)) == SRT_ERROR) {
        std::cerr << "SRT Connect Error: " << srt_getlasterror_str() << std::endl;
        stop();
        return false;
    }

    std::cout << "SRT Sender: Connected to " << ip << ":" << port << " [" << stream_id << "]" << std::endl;
    return true;
}

void SrtSender::send_frame(const std::vector<uint8_t>& frame_data) 
{
    if (_socket == SRT_INVALID_SOCK || frame_data.empty()) 
        return;

    std::lock_guard<std::mutex> lock(_send_mutex);

    const size_t max_payload = 1316;

    size_t bytes_sent = 0;
    size_t total_size = frame_data.size();

    while (bytes_sent < total_size) 
    {

        const size_t bytes_left = total_size - bytes_sent;
        const size_t bytes_to_send = std::min(max_payload, bytes_left);
        
        int res = srt_sendmsg(_socket, 
                             (const char*)(frame_data.data() + bytes_sent), 
                             (int)bytes_to_send, 
                             -1, 0);
        
        if (res == SRT_ERROR) {
            std::cerr << "SRT Send Chunk Error: " << srt_getlasterror_str() << std::endl;
            break;
        }
        
        bytes_sent += bytes_to_send;
    }
}

void SrtSender::stop() {
    if (_socket != SRT_INVALID_SOCK) {
        srt_close(_socket);
        _socket = SRT_INVALID_SOCK;
    }
}

bool SrtSender::is_connected() const 
{ 
    return _socket != SRT_INVALID_SOCK; 
}