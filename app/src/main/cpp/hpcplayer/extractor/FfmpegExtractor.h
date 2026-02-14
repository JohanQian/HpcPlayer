#pragma once

#include "Extractor.h"
#include "common/Handler.h"
#include "common/Looper.h"
#include "common/DataQueue.h"
#include "common/MediaSample.h"
#include <atomic>
#include <mutex>
#include <vector>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

class FfmpegExtractor final : public Extractor, public Handler {
public:
    FfmpegExtractor();
    ~FfmpegExtractor() override;

    void setDataSource(const std::string& path) override;
    void prepare() override;
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

    AVFormatContext* format_context_ = nullptr;
    AVBSFContext* bsf_context_ = nullptr;
    std::mutex mutex_;

    std::string data_source_path_;
    int video_stream_index_ = -1;
    int audio_stream_index_ = -1;
    std::atomic_bool is_playing_{false};
    bool is_seek_ {false};

    DataQueue<std::shared_ptr<MediaSample>> video_packet_queue_{60};
    DataQueue<std::shared_ptr<MediaSample>> audio_packet_queue_{200};
};
