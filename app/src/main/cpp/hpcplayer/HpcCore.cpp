#include "HpcCore.h"
#include "common/Message.h"
#include <android/native_window.h>
#include "common/HpcLog.h"
#include "extractor/FfmpegExtractor.h"
#include "decoder/FfmpegVideoDecoder.h"
#include "decoder/FfmpegAudioDecoder.h"
#include "decoder/andriod/MediaCodecVideoDecoder.h"
#include "renderer/andriod/OpenGLRenderer.h"
#include "renderer/andriod/MediaCodecRenderer.h"
#include "renderer/andriod/AudioOpenSLESRenderer.h"
#include "common/HpcMessage.h"
#include "common/MediaClock.h"
#include "HpcPlayer.h"

namespace {
    constexpr char kTag[] = "HpcCore";
} 

std::shared_ptr<HpcCore> HpcCore::create(std::weak_ptr<HpcPlayer> player) {
    auto looper = std::make_shared<Looper>("HpcCore");
    auto core = std::shared_ptr<HpcCore>(new HpcCore(player, looper));
    core->init();
    looper->start();
    return core;
}

HpcCore::HpcCore(std::weak_ptr<HpcPlayer> player, std::shared_ptr<Looper> looper) 
    : Handler(looper), looper(looper), player(player) {}

void HpcCore::init() {
    auto self = std::static_pointer_cast<HpcCore>(shared_from_this());
    extractor = std::make_shared<FfmpegExtractor>(self);
    
    // Initialize MediaClock
    mediaClock = std::make_shared<MediaClock>();
    
    // Initialize Renderers
    videoRenderer = std::make_shared<MediaCodecRenderer>();
    
    // Set MediaClock to Renderers
    videoRenderer->setMediaClock(mediaClock);

    // Initialize Decoders
    videoLooper = std::make_shared<Looper>("VideoDecoder");
    videoLooper->start(); // Start the looper!
    videoDecoder = std::make_shared<MediaCodecVideoDecoder>(videoLooper);
    videoDecoder->setExtractor(extractor);
    videoDecoder->setRenderer(videoRenderer);

    // Connect Renderers to Decoders (for pulling frames)
    videoRenderer->setDecoder(videoDecoder);

    isRunning.store(true);
}

HpcCore::~HpcCore() {
    if (isRunning.load()) {
        doStop();
        if (looper) {
            looper->quit();
        }
    }
    if (videoLooper) {
        videoLooper->quit();
    }
    if (audioLooper) {
        audioLooper->quit();
    }
}

void HpcCore::setMessageCallback(std::function<void(const Message&)> callback) {
    messageCallback = std::move(callback);
}

void HpcCore::setDataSource(std::string_view path) {
    if (extractor) {
        extractor->setDataSource(std::string(path));
        if (messageCallback) {
            messageCallback({MSG_SET_DATA_SOURCE_COMPLETED});
        }
    }
}

void HpcCore::setSurface(const std::shared_ptr<ANativeWindow>& window) {
    sendMessage({.what = kWhatSetVideoSurface, .obj = window});
}

void HpcCore::prepare() {
    sendMessage({kWhatPrepare});
}

void HpcCore::start() {
    sendMessage({kWhatStart});
}

void HpcCore::resume() {
    sendMessage({kWhatResume});
}

void HpcCore::pause() {
    sendMessage({kWhatPause});
}

void HpcCore::stop() {
    sendMessage({kWhatReset});
}

void HpcCore::seekTo(long msec) {
    sendMessage({.what = kWhatSeek, .arg1 = msec});
}

void HpcCore::release() {
    sendMessage({kWhatMediaClockNotify});
}

int64_t HpcCore::getCurrentPosition() {
    if (mediaClock) {
        return mediaClock->getPositionUs() / 1000; // Convert to ms
    }
    return 0;
}

void HpcCore::onMessageReceived(const Message& msg) {
    switch (msg.what) {
        case kWhatPrepare: 
            doPrepare(); 
            break;
        case kWhatStart: 
            doStart(); 
            break;
        case kWhatResume:
            doResume();
            break;
        case kWhatPause: 
            doPause(); 
            break;
        case kWhatReset: 
            doStop(); 
            break;
        case kWhatSeek: 
            doSeekTo(msg.arg1); 
            break;
        case kWhatSourceNotify:
            if (msg.obj) {
                auto innerMsg = std::static_pointer_cast<Message>(msg.obj);
                onSourceNotify(*innerMsg);
            }
            break;
        case kWhatMediaClockNotify: 
            doRelease(); 
            break;
        case kWhatSetVideoSurface: {
            if (videoDecoder) {
                if (auto mcDecoder = dynamic_cast<MediaCodecVideoDecoder*>(videoDecoder.get())) {
                    mcDecoder->setNativeWindow(std::static_pointer_cast<ANativeWindow>(msg.obj));
                }
            }
            if (videoRenderer) {
                 videoRenderer->init();
            }
            break;
        }
    }
}

void HpcCore::doPrepare() {
    LOG_I("doPrepare");
    if (!extractor) {
        return;
    }
    extractor->prepareAsync();
}

void HpcCore::doStart() {
    if (extractor) {
        extractor->start();
    }

    if (videoDecoder) {
        videoDecoder->start();
    }
    if (audioDecoder) {
        audioDecoder->start();
    }
    
    if (videoRenderer) {
        videoRenderer->start();
    }
    if (audioRenderer) {
        audioRenderer->start();
    }

    isPlaying.store(true);
}

void HpcCore::doResume() {
    if(extractor) {
        extractor->resume();
    }

    if (videoRenderer) {
        videoRenderer->resume();
    }
    if (audioRenderer) {
        audioRenderer->resume();
    }

    isPlaying.store(true);
}

void HpcCore::doPause() {
    if (extractor) {
        extractor->pause();
    }
    
    if (videoRenderer) {
        videoRenderer->pause();
    }
    if (audioRenderer) {
        audioRenderer->pause();
    }
    isPlaying.store(false);
}

void HpcCore::doStop() {
    if (extractor) {
        extractor->stop();
    }
    isPlaying.store(false);
    
    if (videoDecoder) {
        videoDecoder->stop();
    }
    if (audioDecoder) {
        audioDecoder->stop();
    }
    
    if (videoRenderer) {
        videoRenderer->stop();
    }
    if (audioRenderer) {
        audioRenderer->stop();
    }
}

void HpcCore::doSeekTo(long msec) {
    if (!extractor) {
        return;
    }

    doPause();
    // Flush renderers first to stop them from pulling old data
    if (videoRenderer) videoRenderer->flush();
    if (audioRenderer) audioRenderer->flush();
    
    // Flush decoders
    if (videoDecoder) videoDecoder->flush();
    if (audioDecoder) audioDecoder->flush();

    extractor->seekTo(msec);
    mediaClock->syncPosition(msec * 1000);
    
    doResume();
}

void HpcCore::doRelease() {
    if (isRunning.exchange(false)) {
        doStop();
        if (looper) {
            looper->quit();
        }
    }
}

int64_t HpcCore::getDuration() {
    if (extractor) {
        return extractor->getDuration();
    }
    return 0;
}

void HpcCore::initDecoder() {
    // Find tracks
    size_t trackCount = extractor->getTrackCount();
    for (size_t i = 0; i < trackCount; ++i) {
        auto format = extractor->getTrackFormat(i);
        if (format->mime_type.find("video/") == 0 && videoStreamIndex < 0) {
            videoStreamIndex = i;
            extractor->selectTrack(i);
            if (videoDecoder) {
                videoDecoder->configure(format);
            }
        } else if (format->mime_type.find("audio/") == 0 && audioStreamIndex < 0) {
            if (!audioRenderer) {
                audioRenderer = std::make_shared<AudioOpenSLESRenderer>();
                audioRenderer->setMediaFormat(format);
                audioRenderer->init();
                audioRenderer->setMediaClock(mediaClock);
            }
            if (!audioDecoder) {
                audioLooper = std::make_shared<Looper>("AudioDecoder");
                audioLooper->start();
                audioDecoder = std::make_shared<FfmpegAudioDecoder>(audioLooper);
                audioDecoder->setExtractor(extractor);
                audioDecoder->setRenderer(audioRenderer);
                audioRenderer->setDecoder(audioDecoder);
                mediaClock->onRendererEnabled(audioRenderer);
            }

            audioStreamIndex = i;
            extractor->selectTrack(i);
            if (audioDecoder) {
                audioDecoder->configure(format);
            }
        }
    }
}

void HpcCore::onSourceNotify(const Message &msg) {
    switch (msg.what) {
        case Extractor::kWhatPrepared: {
            status_t err = static_cast<status_t>(msg.arg1);
            if (err != 0) {
                if (auto p = player.lock()) {
                    p->notifyPrepareCompleted(err);
                }
                return;
            }

            initDecoder(); // Initialize decoders after extractor is prepared

            if (auto p = player.lock()) {
                p->notifyDuration(extractor->getDuration());
                p->notifyPrepareCompleted(0);
            }
            break;
        }
    }
}
