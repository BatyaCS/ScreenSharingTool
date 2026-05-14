#ifndef SRT_SENDER_H_
#define SRT_SENDER_H_

#include <string>
#include <vector>
#include <mutex>

typedef int32_t SRTSOCKET;

class SrtSender 
{
public:
    SrtSender();
    ~SrtSender();

    /**
     * @param ip IP-адрес вашего Linux-сервера
     * @param port Порт (по умолчанию 8890)
     * @param stream_id Формат для MediaMTX: "publish:mystream"
     * @param password Пароль для AES-шифрования (если пустой — без шифрования)
     */
    bool init(const std::string& ip, uint16_t port, const std::string& stream_id, const std::string& password = "");
    
    // Основной метод отправки сжатого кадра
    void send_frame(const std::vector<uint8_t>& frame_data);
    
    void stop();
    bool is_connected() const;

private:
    SRTSOCKET _socket;
    std::mutex _send_mutex;
};

#endif