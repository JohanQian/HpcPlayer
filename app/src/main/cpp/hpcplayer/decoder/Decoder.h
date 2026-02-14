#pragma once

#include "common/Handler.h"
#include "common/DataQueue.h"
#include "common/MediaFrame.h"
#include "extractor/Extractor.h"
#include <memory>

class MediaFormat;
class Renderer;

class Decoder : public Handler {
public:
    explicit Decoder(const std::shared_ptr<Looper>& looper);
    ~Decoder() override;

    void configure(const std::shared_ptr<MediaFormat>& format);
    void setRenderer(const std::shared_ptr<Renderer>& renderer);
    void setExtractor(const std::shared_ptr<Extractor>& extractor);
    void start();
    void stop();
    void flush();
    void requestInputBuffers();

    std::shared_ptr<MediaFrame> getFrame();

protected:
    void onMessageReceived(const Message& msg) override = 0;

    enum {
        kWhatConfigure           = 'conf',
        kWhatSetRenderer         = 'setR',
        kWhatSetExtractor        = 'setE',
        kWhatStart               = 'strt',
        kWhatStop                = 'stop',
        kWhatFlush               = 'flus',
        kWhatRequestInputBuffers = 'reqB',
    };

    std::shared_ptr<Renderer> renderer_;
    std::shared_ptr<Extractor> extractor_;
    // Limit decoder output queue to avoid OOM if renderer is slow
    DataQueue<std::shared_ptr<MediaFrame>> frame_queue_{16};
};
