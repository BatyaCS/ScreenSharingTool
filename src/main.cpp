#include <common.h>
#include <iostream>
#include <chrono>

#include <network/srt-environment.h>
#include <app/application.h>

#include <windows.h>
EXTERN_C_START
    // Try to set NVIDIA and AMD drivers to use external GPU
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
EXTERN_C_END

/* static */ 
Timer::Time Timer::now()
{
    static const auto start_time = std::chrono::steady_clock::now();
    auto current_time = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
    return static_cast<Timer::Time>(elapsed.count());
}

/* static */ 
void Timer::delay_us(uint us)
{
    // Be aware that OS may unfreeze thread some time after us time 
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}


EXTERN_C uint logger_timestamp()
{
    return static_cast<uint>(Timer::now());
}

void std_logger(LogKind level, const char * fmt, va_list args)
{
    int len = std::vsnprintf(nullptr, 0, fmt, args);
    if (len < 0)
    {
        std::cout << "Incorrect log received for std cout!";
        return;
    }

    std::vector<char> buffer(len + 1);
    std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
    std::cout << buffer.data();
}

AppViewModel * g_view_model = nullptr;
void ui_logger(LogKind level, const char * fmt, va_list args)
{
    int len = std::vsnprintf(nullptr, 0, fmt, args);
    if (len < 0) 
        return;

    std::vector<char> buffer(len + 1);
    std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
    std::string message(buffer.data());
    std::cout << message;

    if (g_view_model)
    {
        std::lock_guard<std::mutex> lock(g_view_model->logs_mutex);
        
        AppModels::LogEntry entry;
        entry.level = level;
        entry.text = message;
        
        g_view_model->logs.push_back(entry);
        if (g_view_model->logs.size() > UI_MAX_LOGS_COUNT) 
            g_view_model->logs.erase(g_view_model->logs.begin());
    }
}

int main()
{
    ::set_logger(&std_logger);
    
    // Init libraries
    SrtEnvironment srt_environment;

    // Init App
    Application app;

    if (!app.init())
        return -1;

    g_view_model = &app.get_view_model();
    ::set_logger(&ui_logger);

    app.run();

    g_view_model = nullptr;
    ::set_logger(&std_logger);

    return 0;
}