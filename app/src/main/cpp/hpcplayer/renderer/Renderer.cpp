#include "Renderer.h"
#include "common/Message.h"
#include "decoder/Decoder.h"
#include "common/Looper.h"
#include "common/MediaClock.h"
#include "common/MediaFrame.h"
#include <unistd.h>
#include <thread>

Renderer::Renderer(std::string name) 
    : Handler(std::make_shared<Looper>(std::move(name))) 
{
    looper_->start();
}

Renderer::~Renderer() {
    if (looper_) {
        looper_->stop();
    }
}

void Renderer::setDecoder(const std::shared_ptr<Decoder>& decoder) {
    sendMessage({.what = kWhatSetDecoder, .obj = decoder});
}

void Renderer::setMediaClock(const std::shared_ptr<MediaClock>& clock) {
    sendMessage({.what = kWhatSetMediaClock, .obj = clock});
}

void Renderer::start() {
    resume();
}

void Renderer::stop() {
    pause();
    flush();
}

void Renderer::flush() {
    isFlushing = true;
    sendMessage({kWhatFlush});
}

void Renderer::pause() {
    isPaused = true;
    // Ensure clock is stopped when renderer is paused
    if (mediaClock) {
        mediaClock->stop();
    }
    sendMessage({kWhatPause});
}

void Renderer::resume() {
    isPaused = false;
    // Ensure clock is started when renderer is resumed
    // This allows Video-only playback to use System Clock
    if (mediaClock) {
        mediaClock->start();
    }
    sendMessage({kWhatResume});
}

bool Renderer::syncFrame(const std::shared_ptr<MediaFrame>& frame) {
    if (!mediaClock || !frame) {
        return true; // No clock or frame, render immediately
    }

    if (isFlushing) {
        return false;
    }

    int64_t frameTimeUs = frame->pts;
    
    // Thresholds
    const int64_t kMinEarlyUs = 30000; // 30ms
    const int64_t kMaxLateUs = -30000; // -30ms
    const int64_t kDropLateUs = -500000; // -500ms (Drop if very late)

    int64_t lastClockUs = mediaClock->getPositionUs();
    int stuckCount = 0;
    const int kMaxStuckCount = 10; // 100 * 10ms = 1000ms

    while (true) {
        if (isFlushing) {
            return false; // Drop frames while flushing
        }
        if (isPaused) {
            return true; // Render immediately if paused
        }

        int64_t clockTimeUs = mediaClock->getPositionUs();
        int64_t earlyUs = frameTimeUs - clockTimeUs;

        if (earlyUs > kMinEarlyUs) {
            // Check if clock is stuck
            if (clockTimeUs == lastClockUs) {
                stuckCount++;
                if (stuckCount > kMaxStuckCount) {
                    // Clock stuck for too long, force render
                    return true;
                }
            } else {
                stuckCount = 0;
                lastClockUs = clockTimeUs;
            }

            // Too early, wait
            int64_t waitUs = earlyUs - 10000; // Wake up 10ms early
            if (waitUs > 10000) {
                // Sleep in chunks to check for pause
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue; // Re-check clock and pause status
            } else if (waitUs > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(waitUs));
            }
            return true;
        } else if (earlyUs < kDropLateUs) {
            // Way too late, drop it to catch up
            return false;
        } else if (earlyUs < kMaxLateUs) {
            // Late, but maybe still worth showing? 
            // For now, let's drop to keep sync tight.
            return false; 
        } else {
            // On time
            return true;
        }
    }
}
