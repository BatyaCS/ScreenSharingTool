#include <common.h>
#include <iostream>
#include <chrono>
#include <vector>

#include <network/srt-environment.h>
#include <app/application.h>

uint64_t GetTicksSinceStart() 
{
    static const auto start_time = std::chrono::steady_clock::now();
    auto current_time = std::chrono::steady_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
    return static_cast<uint64_t>(elapsed.count());
}

EXTERN_C uint logger_timestamp()
{
    return static_cast<uint>(GetTicksSinceStart());
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
    GetTicksSinceStart();
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