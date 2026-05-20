#include <common.h>
#include <capturer/hw-video-capturer.h>
#include <algorithm>

#include <iostream>

bool HwVideoCapturer::start(const CaptureConfig& cap, HwFrameCallback callback)
{
    if (_is_running.load())
    {
        LOG_ERROR("Failed to start capturer, already started!\n");
        return false;
    }

    if (!callback)
        return false;

    if (cap.target == Target::DISPLAY)
    {
        auto config = SL::Screen_Capture::CreateCaptureConfiguration([cap]() 
        {
            auto monitors = SL::Screen_Capture::GetMonitors();
            std::vector<SL::Screen_Capture::Monitor> filtered;
            
            std::string search_name = cap.source_name;
            std::transform(search_name.begin(), search_name.end(), search_name.begin(), ::tolower);

            for (auto& m : monitors) 
            {
                std::string current_monitor_name = m.Name;
                std::transform(current_monitor_name.begin(), current_monitor_name.end(), current_monitor_name.begin(), ::tolower);
                
                if (current_monitor_name.find(search_name) != std::string::npos) 
                {
                    filtered.push_back(m);
                    break;
                }
            }

            if (filtered.empty() && !monitors.empty()) 
                filtered.push_back(monitors[0]);
            
            return filtered;
        });

        config->onNewD3D11Frame([callback](ID3D11Texture2D* tex, ID3D11Device* dev, const SL::Screen_Capture::Monitor& monitor) 
        {
            callback(tex, dev);
        });

        _capture_manager = config->start_capturing();
    }
    else 
    {
        auto config = SL::Screen_Capture::CreateCaptureConfiguration([cap]() 
        {
            auto windows = SL::Screen_Capture::GetWindows();
            std::vector<SL::Screen_Capture::Window> filtered;
            
            std::string search_name = cap.source_name; 
            std::transform(search_name.begin(), search_name.end(), search_name.begin(), ::tolower);

            for (auto& w : windows) 
            {
                std::string current_window_name = w.Name;
                std::transform(current_window_name.begin(), current_window_name.end(), current_window_name.begin(), ::tolower);
                
                if (current_window_name.find(search_name) != std::string::npos) 
                {
                    filtered.push_back(w);
                    break;
                }
            }

            return filtered;
        });

        config->onNewD3D11Frame([callback](ID3D11Texture2D* tex, ID3D11Device* dev, const SL::Screen_Capture::Window& window) 
        {
            callback(tex, dev);
        });

        _capture_manager = config->start_capturing();
    }

    auto interval_us = std::chrono::microseconds(1000000 / cap.target_fps);
    _capture_manager->setFrameChangeInterval(interval_us);

    LOG("Start capturing, source: %s!\n", cap.source_name.c_str());

    _is_running.store(true);
    return true;
}

void HwVideoCapturer::stop()
{
    if (_capture_manager)
    {
        _capture_manager->pause();
        _capture_manager = nullptr;
    }

    _is_running.store(false);
}