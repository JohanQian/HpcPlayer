#pragma once

#include "common/MediaSample.h"
#include <cstdint>
#include <memory>
#include <string>

class MediaFormat;

enum class MediaType {
    VIDEO,
    AUDIO,
};

class Extractor {
public:
    virtual ~Extractor() = default;

    virtual void setDataSource(const std::string& path) = 0;
    virtual void prepare() = 0;
    virtual void start() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
    virtual void seekTo(int64_t msec) = 0;

    virtual size_t getTrackCount() = 0;
    virtual std::shared_ptr<MediaFormat> getTrackFormat(size_t index) = 0;
    virtual void selectTrack(size_t index) = 0;
    virtual int64_t getDuration() = 0;

    virtual std::shared_ptr<MediaSample> getSample(MediaType type) = 0;
};
