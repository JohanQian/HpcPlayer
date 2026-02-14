#include "MediaClock.h"

MediaClock::MediaClock() {
}

void MediaClock::start() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!isStarted) {
        isStarted = true;
        baseSystemTimeUs = systemClock.GetPosition();
    }
}

void MediaClock::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (isStarted) {
        int64_t position_us = -1;
        if (rendererClock) {
            position_us = rendererClock->getCurrentPosition();
        }

        if (position_us != -1) {
            baseMediaPositionUs = position_us;
        } else {
            int64_t current_system_time = systemClock.GetPosition();
            int64_t elapsed_system = current_system_time - baseSystemTimeUs;
            baseMediaPositionUs += static_cast<int64_t>(elapsed_system * playbackSpeed);
        }
        isStarted = false;
    }
}

int64_t MediaClock::getPositionUs() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (rendererClock) {
        int64_t position_us = rendererClock->getCurrentPosition();
        if (position_us != -1) {
            // If the renderer clock has advanced, sync with it.
            if (position_us > baseMediaPositionUs) {
                baseMediaPositionUs = position_us;
                if (isStarted) {
                    baseSystemTimeUs = systemClock.GetPosition();
                }
                return position_us;
            }
            // If the renderer clock is stalled or has gone backward,
            // fall through to use the system clock for an estimated position.
        }
    }

    if (isStarted) {
        int64_t current_system_time = systemClock.GetPosition();
        int64_t elapsed_system = current_system_time - baseSystemTimeUs;
        return baseMediaPositionUs + static_cast<int64_t>(elapsed_system * playbackSpeed);
    }
    return baseMediaPositionUs;
}

void MediaClock::syncPosition(int64_t position_us) {
    std::lock_guard<std::mutex> lock(mutex);
    baseMediaPositionUs = position_us;
    if (isStarted) {
        baseSystemTimeUs = systemClock.GetPosition();
    }
}

void MediaClock::setPlaybackSpeed(float speed) {
    std::lock_guard<std::mutex> lock(mutex);
    if (isStarted) {
        int64_t position_us = -1;
        if (rendererClock) {
            position_us = rendererClock->getCurrentPosition();
        }
        
        if (position_us != -1) {
            baseMediaPositionUs = position_us;
        } else {
            int64_t current_system_time = systemClock.GetPosition();
            int64_t elapsed_system = current_system_time - baseSystemTimeUs;
            baseMediaPositionUs += static_cast<int64_t>(elapsed_system * playbackSpeed);
        }
        baseSystemTimeUs = systemClock.GetPosition();
    }
    
    playbackSpeed = speed;
}

void MediaClock::onRendererEnabled(std::shared_ptr<Renderer> renderer_clock) {
    std::lock_guard<std::mutex> lock(mutex);
    if (renderer_clock != rendererClock) {
        rendererClock = renderer_clock;
    }
}

void MediaClock::onRendererDisabled() {
    std::lock_guard<std::mutex> lock(mutex);
    if (rendererClock) {
        int64_t position_us = rendererClock->getCurrentPosition();
        if (position_us != -1) {
            baseMediaPositionUs = position_us;
        } else if (isStarted) {
            int64_t current_system_time = systemClock.GetPosition();
            int64_t elapsed_system = current_system_time - baseSystemTimeUs;
            baseMediaPositionUs += static_cast<int64_t>(elapsed_system * playbackSpeed);
        }
        
        if (isStarted) {
            baseSystemTimeUs = systemClock.GetPosition();
        }
        rendererClock = nullptr;
    }
}
