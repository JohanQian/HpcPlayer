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
    bytesPerSecond = mediaFormat->sample_rate * mediaFormat->channels * kPcm16BitBytes;
    if (bytesPerSecond == 0) {
        bytesPerSecond = 1; // Avoid division by zero
    }
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
            doEnqueue();
            break;
    }
}

void AudioOpenSLESRenderer::doSetDecoder(const std::shared_ptr<Decoder>& decoder) {
    this->decoder = decoder;
}

void AudioOpenSLESRenderer::doDrainQueue() {
    if (isPaused || !decoder) {
        return;
    }
    
    // Trigger the first enqueue if the player is ready
    if (playerBufferQueue) {
        doEnqueue();
    } else {
        // If player not created yet, try to get a frame to create it
        auto frame = decoder->getFrame();
        if (frame) {
            doRender(frame);
        }
    }
}

void AudioOpenSLESRenderer::doFlush() {
    if (playerBufferQueue) (*playerBufferQueue)->Clear(playerBufferQueue);
    playedBytes.store(0);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        std::queue<size_t> empty;
        std::swap(pendingBufferSizes, empty);
    }
    isFlushing = false;
    firstFrameAfterFlush = true;
    startPts = -1;
    currentPositionUs = 0;
}

bool AudioOpenSLESRenderer::ensurePlayerInitialized(int sampleRate, int channels) {
    if (!playerObject) {
        return false;
    }
    return true;
}

void AudioOpenSLESRenderer::doRender(const std::shared_ptr<MediaFrame>& frame) {
    if (!frame || !frame->data || frame->size == 0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(playerMutex);

    if (!ensurePlayerInitialized(frame->sample_rate, frame->channels)) {
        return;
    }
    
    // Store the frame so we can enqueue it
    currentFrame = frame;
    
    // Initial enqueue
    if (playerBufferQueue) {
        SLresult result = (*playerBufferQueue)->Enqueue(playerBufferQueue, frame->data.get(), frame->size);
        if (result == SL_RESULT_SUCCESS) {
            {
                std::lock_guard<std::mutex> qLock(queueMutex);
                pendingBufferSizes.push(frame->size);
            }

            if (startPts == -1) {
                startPts = frame->pts;
                currentPositionUs = startPts;
            }
        }
    }
}

void AudioOpenSLESRenderer::doEnqueue() {
    if (isPaused || !decoder) {
        return;
    }

    auto frame = decoder->getFrame();
    if (frame) {
        if (!frame->data || frame->size == 0) {
            // Skip invalid frames and try next
            sendMessage({kWhatQueueBuffer});
            return;
        }

        std::lock_guard<std::mutex> lock(playerMutex);
        
        if (!ensurePlayerInitialized(frame->sample_rate, frame->channels)) {
            return;
        }

        currentFrame = frame;

        if (playerBufferQueue) {
            SLresult result = (*playerBufferQueue)->Enqueue(playerBufferQueue, frame->data.get(), frame->size);
            if (result == SL_RESULT_SUCCESS) {
                {
                    std::lock_guard<std::mutex> qLock(queueMutex);
                    pendingBufferSizes.push(frame->size);
                }

                if (startPts == -1 || frame->is_seek_frame) {
                    startPts = frame->pts;
                    currentPositionUs = startPts;
                    // Reset played bytes counter for new seek segment if needed, 
                    // but here we rely on startPts + playedBytes calculation.
                    // If we seek, we should probably reset playedBytes logic or adjust startPts.
                    // Since we flushed, playedBytes is 0.
                }
            }
        }
    }
}

void AudioOpenSLESRenderer::playerCallback(SLAndroidSimpleBufferQueueItf bq, void* context) {
    auto* renderer = static_cast<AudioOpenSLESRenderer*>(context);
    
    // Update position
    size_t size = 0;
    {
        std::lock_guard<std::mutex> lock(renderer->queueMutex);
        if (!renderer->pendingBufferSizes.empty()) {
            size = renderer->pendingBufferSizes.front();
            renderer->pendingBufferSizes.pop();
        }
    }
    
    if (size > 0) {
        renderer->playedBytes.fetch_add(size, std::memory_order_relaxed);
        int64_t bytes = renderer->playedBytes.load(std::memory_order_relaxed);
        if (renderer->bytesPerSecond > 0) {
            // Calculate microseconds: (bytes * 1000000) / bytesPerSecond
            renderer->currentPositionUs = renderer->startPts + (bytes * 1000000) / renderer->bytesPerSecond;
        }
    }

    renderer->sendMessage({kWhatQueueBuffer});
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
    if (!engineEngine) return false;

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
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE, SL_IID_PLAY};
    const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    
    if ((*engineEngine)->CreateAudioPlayer(engineEngine, &playerObject, &audioSrc, &audioSnk, 2, ids, req) != SL_RESULT_SUCCESS) {
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
