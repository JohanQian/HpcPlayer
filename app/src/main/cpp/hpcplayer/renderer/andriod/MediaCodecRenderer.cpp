#include "MediaCodecRenderer.h"
#include "common/HpcLog.h"
#include "common/Message.h"
#include "decoder/Decoder.h"
#include <media/NdkMediaCodec.h>
#include "common/MediaClock.h"

MediaCodecRenderer::MediaCodecRenderer() : Renderer("MediaCodecRenderer") {}

MediaCodecRenderer::~MediaCodecRenderer() = default;

void MediaCodecRenderer::init() {
    // This function is no longer needed as the codec is retrieved from the frame.
}

void MediaCodecRenderer::onMessageReceived(const Message& msg) {
    switch (msg.what) {
        case kWhatSetDecoder:
            doSetDecoder(std::static_pointer_cast<Decoder>(msg.obj));
            break;
        case kWhatSetMediaClock:
            mediaClock = std::static_pointer_cast<MediaClock>(msg.obj);
            break;
        case kWhatResume:
            isPaused = false;
            sendMessage({kWhatDrainVideoQueue});
            break;
        case kWhatPause:
            isPaused = true;
            break;
        case kWhatDrainVideoQueue:
            doDrainQueue();
            break;
        case kWhatFlush:
            frameQueue.flush();
            isFlushing = false;
            firstFrameAfterFlush = true;
            isFirstFrame = true;
            break;
        case kWhatQueueBuffer:
            doQueueBuffer(std::static_pointer_cast<MediaFrame>(msg.obj));
            break;
        case kWhatConsume:
            notifyConsume();
            break;
    }
}

void MediaCodecRenderer::doSetDecoder(const std::shared_ptr<Decoder>& decoder) {
    this->decoder = decoder;
}

void MediaCodecRenderer::doDrainQueue() {
//    if (isPaused || !decoder) {
//        return;
//    }
//
//    auto optFrame = frameQueue.waitAndPopFor(std::chrono::milliseconds(5));
//    if (optFrame) {
//        auto frame = *optFrame;
//        if (frame) {
//            doRender(frame);
//        }
//    }
}

void MediaCodecRenderer::doQueueBuffer(const std::shared_ptr<MediaFrame>& frame) {
//    if (isPaused || !decoder || !frame) {
//        return;
//    }
//    if (isFirstFrame) {
//        isFirstFrame = false;
//        AMediaCodec_releaseOutputBuffer(frame->codec, frame->index, true);
//        notifyConsume();
//        return;
//    }
//    frameQueue.push(frame);
}

void MediaCodecRenderer::notifyConsume() {
    if (isPaused || !decoder) {
        return;
    }

    auto optFrame = frameQueue.tryPop();
    if (optFrame) {
        auto frame = *optFrame;
        if (frame) {
            doRender(frame);
        }
    } else {
        sendMessage({kWhatConsume});
    }
}

void MediaCodecRenderer::doRender(const std::shared_ptr<MediaFrame>& frame) {
    if (!frame || !frame->codec || frame->index < 0) {
        return;
    }

    if (syncFrame(frame)) {
        AMediaCodec_releaseOutputBuffer(frame->codec, frame->index, true);
    } else {
        AMediaCodec_releaseOutputBuffer(frame->codec, frame->index, false);
    }

    sendMessage({kWhatConsume});
}
