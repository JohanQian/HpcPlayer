#include "AudioOpenSLESRenderer.h"

#include "common/HpcLog.h"
#include "common/MediaClock.h"
#include "common/Message.h"
#include "common/MediaFormat.h"
#include "decoder/Decoder.h"

constexpr char kTag[] = "AudioOpenSLESRenderer";
constexpr int kPcm16BitBytes = 2;

AudioOpenSLESRenderer::AudioOpenSLESRenderer() : Renderer("AudioOpenSLESRenderer") {
}

AudioOpenSLESRenderer::~AudioOpenSLESRenderer() {
    doRelease();
}

void AudioOpenSLESRenderer::init() {
    createEngine();
    createOutputMix();
    createPlayer(mediaFormat->sample_rate, mediaFormat->channels);
}

int64_t AudioOpenSLESRenderer::getCurrentPosition() {
    if (startPts == -1) {
        return -1;
    }
    return currentPositionUs;
}

void AudioOpenSLESRenderer::onMessageReceived(const Message& msg) {
    switch (msg.what) {
        case kWhatSetDecoder:
            doSetDecoder(std::static_pointer_cast<Decoder>(msg.obj));
            break;
        case kWhatSetMediaClock:
            mediaClock = std::static_pointer_cast<MediaClock>(msg.obj);
            break;
        case kWhatResume:
            doResume();
            break;
        case kWhatPause:
            doPause();
            break;
        case kWhatDrainAudioQueue:
            doDrainQueue();
            break;
        case kWhatFlush:
            doFlush();
            break;
        case kWhatQueueBuffer:
            frameQueue.push(std::static_pointer_cast<MediaFrame>(msg.obj));
            sendMessage({kWhatConsume});
            break;
        case kWhatConsume:
            notifyConsume(msg.arg1 == 1);
            break;
    }
}

void AudioOpenSLESRenderer::doSetDecoder(const std::shared_ptr<Decoder>& decoder) {
    this->decoder = decoder;
}

void AudioOpenSLESRenderer::doDrainQueue() {
    if (isPaused) {
        return;
    }
    notifyConsume();
}

void AudioOpenSLESRenderer::doFlush() {
    frameQueue.flush();

    {
        std::lock_guard<std::mutex> lock(playerMutex);

        // Stop the player to abort current playback and prevent further callbacks for old frames
        if (playerPlay) {
            (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_STOPPED);
        }

        if (playerBufferQueue) {
            (*playerBufferQueue)->Clear(playerBufferQueue);
        }

        // Do NOT clear bufferGraveyard here.
        // We must keep old frames alive until the audio thread is definitely done with them.
        // They will be cleared gradually in notifyConsume or fully in doRelease.

        // Move currently rendering frames to graveyard to keep them alive
        // until we are sure they are not used.
        while(!renderingQueue.empty()) {
            bufferGraveyard.push_back(renderingQueue.front());
            renderingQueue.pop();
        }

        // Keep graveyard size manageable
        while (bufferGraveyard.size() > 50) {
            bufferGraveyard.erase(bufferGraveyard.begin());
        }

        pendingFrame = nullptr;

        // Restore player state
        if (playerPlay) {
            if (isPaused) {
                (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PAUSED);
            } else {
                (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PLAYING);
            }
        }
    }

    isFirstFrame = true;
    firstFrameAfterFlush = true;
    startPts = -1;
    currentPositionUs = 0;
    isFlushing = false;
}

bool AudioOpenSLESRenderer::doRender(const std::shared_ptr<MediaFrame>& frame) {
    if (isFlushing) {
        return false;
    }
    if (!frame || !frame->data || frame->size == 0) {
        return false;
    }
    LOG_E("doRender");
    std::lock_guard<std::mutex> lock(playerMutex);

    if (!playerObject) {
        LOG_E("Player not initialized, dropping frame.");
        return false;
    }

    if (playerBufferQueue) {
        SLresult result = (*playerBufferQueue)->Enqueue(playerBufferQueue, frame->data.get(), frame->size);
        if (result == SL_RESULT_SUCCESS) {
            if (startPts == -1 || frame->is_seek_frame) {
                startPts = frame->pts;
            }
            renderingQueue.push(frame);
            return true;
        } else {
            LOG_E("Failed to enqueue audio buffer: %d", result);
            return false;
        }
    }
    return false;
}

void AudioOpenSLESRenderer::notifyConsume(bool fromCallback) {
    {
        std::lock_guard<std::mutex> lock(playerMutex);
        if (fromCallback && !renderingQueue.empty()) {
             auto frame = renderingQueue.front();
             renderingQueue.pop();
             bufferGraveyard.push_back(frame);

             while (bufferGraveyard.size() > 50) {
                bufferGraveyard.erase(bufferGraveyard.begin());
             }
        }

        // Defensive cleanup: if renderingQueue grows too large, force clear it
        while (renderingQueue.size() > 200) {
            LOG_E("Rendering queue too large, force clearing!");
            renderingQueue.pop();
        }
    }

    if (isPaused) {
        return;
    }

    int processedCount = 0;
    const int kMaxProcessPerLoop = 10;

    // Try to fill the OpenSLES queue
    while (processedCount < kMaxProcessPerLoop) {
        // If we have a pending frame that failed to enqueue previously, try it first
        std::shared_ptr<MediaFrame> frameToRender = pendingFrame;
        if (!frameToRender) {
             auto currentFrame = frameQueue.tryPop();
             if (currentFrame) {
                 frameToRender = *currentFrame;
             }
        }

        if (!frameToRender) {
            break;
        }

        LOG_E("notifyConsume %lld  size %d", frameToRender->pts,frameQueue.size());
        if (!doRender(frameToRender)) {
             // If render failed (e.g. queue full), save it as pending and stop.
             pendingFrame = frameToRender;
             break;
        } else {
             pendingFrame = nullptr;
             processedCount++;
        }
    }

    // If we processed the max batch size and there might be more data, schedule another run
    if (processedCount >= kMaxProcessPerLoop) {
        sendMessage({kWhatConsume});
    }
}

void AudioOpenSLESRenderer::playerCallback(SLAndroidSimpleBufferQueueItf bq, void* context) {
    auto* renderer = static_cast<AudioOpenSLESRenderer*>(context);
    // arg1 = 1 indicates fromCallback = true
    renderer->sendMessage({.what = kWhatConsume, .arg1 = 1});
}

void AudioOpenSLESRenderer::doPause() {
    isPaused = true;
    std::lock_guard<std::mutex> lock(playerMutex);
    if (playerPlay) {
        (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PAUSED);
    }
}

void AudioOpenSLESRenderer::doResume() {
    isPaused = false;
    std::lock_guard<std::mutex> lock(playerMutex);
    if (playerPlay) {
        (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PLAYING);
    }
    sendMessage({kWhatDrainAudioQueue});
}

void AudioOpenSLESRenderer::doRelease() {
    std::lock_guard<std::mutex> lock(playerMutex);
    if (playerObject) {
        (*playerObject)->Destroy(playerObject);
        playerObject = nullptr;
        playerPlay = nullptr;
        playerBufferQueue = nullptr;
    }
    if (outputMixObject) (*outputMixObject)->Destroy(outputMixObject);
    if (engineObject) (*engineObject)->Destroy(engineObject);
    outputMixObject = nullptr;
    engineObject = nullptr;
    engineEngine = nullptr;

    while(!renderingQueue.empty()) renderingQueue.pop();
    bufferGraveyard.clear();
    pendingFrame = nullptr;
}

void AudioOpenSLESRenderer::createEngine() {
    if (engineObject) return;
    slCreateEngine(&engineObject, 0, nullptr, 0, nullptr, nullptr);
    (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
}

void AudioOpenSLESRenderer::createOutputMix() {
    if (outputMixObject || !engineEngine) return;
    (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, nullptr, nullptr);
    (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
}

bool AudioOpenSLESRenderer::createPlayer(int sampleRate, int channels) {
    if (!engineEngine || !outputMixObject) return false;

    SLDataLocator_AndroidSimpleBufferQueue locBufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM formatPcm = {
            SL_DATAFORMAT_PCM, static_cast<SLuint32>(channels), static_cast<SLuint32>(sampleRate * 1000),
            SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
            static_cast<SLuint32>((channels == 2) ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT) : SL_SPEAKER_FRONT_CENTER),
            SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource audioSrc = {&locBufq, &formatPcm};
    SLDataLocator_OutputMix locOutmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&locOutmix, nullptr};
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE, SL_IID_PLAY, SL_IID_VOLUME};
    const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    if ((*engineEngine)->CreateAudioPlayer(engineEngine, &playerObject, &audioSrc, &audioSnk, 3, ids, req) != SL_RESULT_SUCCESS) {
        LOG_E("Failed to create audio player");
        return false;
    }

    if ((*playerObject)->Realize(playerObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) {
        LOG_E("Failed to realize audio player");
        return false;
    }

    if ((*playerObject)->GetInterface(playerObject, SL_IID_PLAY, &playerPlay) != SL_RESULT_SUCCESS) {
        LOG_E("Failed to get play interface");
        return false;
    }

    if ((*playerObject)->GetInterface(playerObject, SL_IID_BUFFERQUEUE, &playerBufferQueue) != SL_RESULT_SUCCESS) {
        LOG_E("Failed to get buffer queue interface");
        return false;
    }

    if (!playerBufferQueue) {
        LOG_E("Player buffer queue is null");
        return false;
    }

    if ((*playerBufferQueue)->RegisterCallback(playerBufferQueue, playerCallback, this) != SL_RESULT_SUCCESS) {
        LOG_E("Failed to register callback");
        return false;
    }

    if (isPaused) {
        (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PAUSED);
    } else {
        (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PLAYING);
    }
    return true;
}
