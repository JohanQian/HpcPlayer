#pragma once

#include "Clock.h"
#include "renderer/Renderer.h"
#include <mutex>
#include <atomic>

class MediaClock {
public:
    MediaClock();
    
    void start();
    void stop();
    
    int64_t getPositionUs();
    void syncPosition(int64_t position_us);
    
    void setPlaybackSpeed(float speed);

    void onRendererEnabled(std::shared_ptr<Renderer> renderer_clock);
    void onRendererDisabled();

private:
    SystemClock systemClock;
    
    bool isStarted = false;
    int64_t baseMediaPositionUs = 0;
    int64_t baseSystemTimeUs = 0;
    float playbackSpeed = 1.0f;

    std::shared_ptr<Renderer> rendererClock = nullptr;
    
    std::mutex mutex;
};
