#pragma once

#include "Extractor.h"
#include "common/Handler.h"
#include "common/Looper.h"
#include "common/DataQueue.h"
#include "common/MediaSample.h"
#include <atomic>
#include <mutex>
#include <vector>

namespace hpc {

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

class HpcCore;

class FfmpegExtractor final : public Extractor, public Handler {
public:
    explicit FfmpegExtractor(std::weak_ptr<HpcCore> core);
    ~FfmpegExtractor() override;

    void setDataSource(const std::string& path) override;
    void prepareAsync() override;
    void start() override;
    void pause() override;
    void resume() override;
    void stop() override;
    void seekTo(int64_t msec) override;
    
    size_t getTrackCount() override;
    int64_t getDuration() override;
    std::shared_ptr<MediaFormat> getTrackFormat(size_t index) override;
    void selectTrack(size_t index) override;
    std::shared_ptr<MediaSample> getSample(MediaType type) override;
    void postReadBuffer();
    void notifyPrepared(status_t err) override;

protected:
    void onMessageReceived(const Message& msg) override;

private:
    enum {
        kWhatPrepare,
        kWhatStart,
        kWhatStop,
        kWhatSeek,
        kWhatPause,
        kWhatResume,
        kWhatReadBuffer,
    };

    void doPrepare();
    void doStart();
    void doPause();
    void doResume();
    void doStop();
    void doSeekTo(int64_t msec);
    void doReadBuffer();

    AVFormatContext* formatContext = nullptr;
    AVBSFContext* bsfContext = nullptr;
    std::mutex mutex;
    std::weak_ptr<HpcCore> core;

    std::string dataSourcePath;
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    std::atomic_bool isPlaying{false};
    bool isSeek {false};

    DataQueue<std::shared_ptr<MediaSample>> videoPacketQueue{60};
    DataQueue<std::shared_ptr<MediaSample>> audioPacketQueue{200};
};
} // namespace hpc
