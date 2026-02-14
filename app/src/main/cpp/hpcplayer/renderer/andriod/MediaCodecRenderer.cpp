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
            isFlushing = false;
            firstFrameAfterFlush = true;
            // After flush, we need to restart draining the queue
            sendMessage({kWhatDrainVideoQueue});
            break;
    }
}

void MediaCodecRenderer::doSetDecoder(const std::shared_ptr<Decoder>& decoder) {
    this->decoder = decoder;
}

void MediaCodecRenderer::doDrainQueue() {
    if (isPaused || !decoder) {
        return;
    }

    std::shared_ptr<MediaFrame> frame = decoder->getFrame();
    if (frame) {
        doRender(frame);
        sendMessage({kWhatDrainVideoQueue}); // Loop to drain the next frame
    } else {
        // If no frame is available, try again later to avoid busy loop if GetFrame returns null immediately
        // But since we changed GetFrame to wait with timeout, we can just loop.
        // However, if GetFrame timed out, we should probably check for pause/flush again.
        // Sending a message to self allows processing other messages in the queue.
        sendMessage({kWhatDrainVideoQueue});
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
}
