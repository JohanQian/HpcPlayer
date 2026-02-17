#include "MediaCodecVideoDecoder.h"
#include "common/HpcLog.h"
#include "common/MediaFormat.h"
#include "renderer/Renderer.h"
#include "common/Message.h"
#include "common/MediaSample.h"
#include <android/native_window.h>

namespace {
    constexpr char kTag[] = "MediaCodecVideoDecoder";
}

MediaCodecVideoDecoder::MediaCodecVideoDecoder(const std::shared_ptr<Looper>& looper) : Decoder(looper) {}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
    doStop();
}

void MediaCodecVideoDecoder::setNativeWindow(const std::shared_ptr<ANativeWindow>& window) {
    nativeWindow = window;
}

AMediaCodec* MediaCodecVideoDecoder::getCodec() const {
    return codec;
}

void MediaCodecVideoDecoder::onMessageReceived(const Message& msg) {
    switch (msg.what) {
        case kWhatConfigure: 
            doConfigure(std::static_pointer_cast<MediaFormat>(msg.obj)); 
            break;
        case kWhatSetRenderer: 
            doSetRenderer(std::static_pointer_cast<Renderer>(msg.obj)); 
            break;
        case kWhatSetExtractor: 
            doSetExtractor(std::static_pointer_cast<Extractor>(msg.obj)); 
            break;
        case kWhatStart:
            doStart(); 
            break;
        case kWhatStop: 
            doStop(); 
            break;
        case kWhatFlush: 
            doFlush(); 
            break;
        case kWhatRequestInputBuffers: 
            doRequestInputBuffers(); 
            break;
            break;
    }
}

void MediaCodecVideoDecoder::doConfigure(const std::shared_ptr<MediaFormat>& format) {
    LOG_E("DoConfigure");
    const char* mime = format->mime_type.c_str();
    codec = AMediaCodec_createDecoderByType(mime);
    if (!codec) {
        LOG_E("DoConfigure failed: AMediaCodec_createDecoderByType failed for mime %s", mime);
        return;
    }

    AMediaFormat* media_format = AMediaFormat_new();
    AMediaFormat_setString(media_format, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(media_format, AMEDIAFORMAT_KEY_WIDTH, format->width);
    AMediaFormat_setInt32(media_format, AMEDIAFORMAT_KEY_HEIGHT, format->height);
    
    // Set max-input-size to avoid buffer too small errors for high resolution/bitrate (especially H.265)
    int32_t max_input_size = format->width * format->height;
    if (max_input_size < 1024 * 1024) max_input_size = 1024 * 1024; // Minimum 1MB
    AMediaFormat_setInt32(media_format, "max-input-size", max_input_size);

    if (!format->extradata.empty()) {
        AMediaFormat_setBuffer(media_format, "csd-0", (void*)format->extradata.data(), format->extradata.size());
    }

    media_status_t status = AMediaCodec_configure(codec, media_format, nativeWindow.get(), nullptr, 0);
    if (status != AMEDIA_OK) {
        LOG_E("DoConfigure failed: AMediaCodec_configure returned status %d", status);
        AMediaCodec_delete(codec);
        codec = nullptr;
    }
    AMediaFormat_delete(media_format);
}

void MediaCodecVideoDecoder::doSetRenderer(const std::shared_ptr<Renderer>& renderer) {
    videoRenderer = renderer;
}

void MediaCodecVideoDecoder::doSetExtractor(const std::shared_ptr<Extractor>& extractor) {
    extractor_ = extractor;
}

void MediaCodecVideoDecoder::doStart() {
    if (!codec) {
        LOG_E("DoStart failed: codec is null. Was DoConfigure successful?");
        return;
    }
    media_status_t status = AMediaCodec_start(codec);
    if (status == AMEDIA_OK) {
        codecStarted = true;
        doRequestInputBuffers();
    } else {
        LOG_E("DoStart failed: AMediaCodec_start returned status %d", status);
    }
}

void MediaCodecVideoDecoder::doStop() {
    if (codec) {
        if (codecStarted) {
            AMediaCodec_stop(codec);
            codecStarted = false;
        }
        AMediaCodec_delete(codec);
        codec = nullptr;
    }
}

void MediaCodecVideoDecoder::doFlush() {
    if (codec && codecStarted) {
        AMediaCodec_flush(codec);
    }
    pendingSample = nullptr;
    isInputEosQueued = false;
    doRequestInputBuffers();
}

void MediaCodecVideoDecoder::doRequestInputBuffers() {
    if (!extractor_ || !codecStarted) {
        return;
    }

    // 1. Input processing
    // Use 0 timeout to avoid blocking output processing.
    if (!isInputEosQueued) {
        if (!pendingSample) {
            pendingSample = extractor_->getSample(MediaType::VIDEO);
        }

        if (pendingSample) {
            ssize_t in_index = AMediaCodec_dequeueInputBuffer(codec, 0); 
            if (in_index >= 0) {
                size_t buf_size = 0;
                uint8_t* buf = AMediaCodec_getInputBuffer(codec, in_index, &buf_size);
                if (buf) {
                    if (pendingSample->is_eos) {
                        AMediaCodec_queueInputBuffer(codec, in_index, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                        isInputEosQueued = true;
                    } else {
                        size_t copy_size = std::min(buf_size, pendingSample->data.size());
                        memcpy(buf, pendingSample->data.data(), copy_size);
                        AMediaCodec_queueInputBuffer(codec, in_index, 0, copy_size, pendingSample->pts, 0);
                    }
                    pendingSample = nullptr;
                }
            } else if (in_index != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                LOG_E("AMediaCodec_dequeueInputBuffer error: %zd", in_index);
            }
        }
    }

    // 2. Output processing
    // Drain all available output buffers to prevent backlog
    bool output_available = false;
    while (true) {
        AMediaCodecBufferInfo info;
        // Use a small timeout (2ms) for the first check to avoid busy loop if no output is ready yet,
        // but drain subsequent buffers immediately (0ms).
        ssize_t out_index = AMediaCodec_dequeueOutputBuffer(codec, &info, output_available ? 0 : 2000);
        
        if (out_index >= 0) {
            output_available = true;
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                // EOS frame
                auto eos_frame = std::make_shared<MediaFrame>();
                if (videoRenderer) {
                    videoRenderer->queueBuffer(eos_frame);
                }
                break; // Stop draining on EOS
            } else {
                auto out_frame = std::make_shared<MediaFrame>();
                out_frame->pts = info.presentationTimeUs;
                out_frame->index = out_index;
                out_frame->codec = codec;
                
                if (videoRenderer) {
                    videoRenderer->queueBuffer(out_frame);
                }
            }
        } else if (out_index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            auto format = AMediaCodec_getOutputFormat(codec);
            LOG_E("Output format changed: %s", AMediaFormat_toString(format));
            AMediaFormat_delete(format);
            output_available = true;
        } else if (out_index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            LOG_E("Output buffers changed");
            output_available = true;
        } else if (out_index == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            break; // No more output
        } else {
            LOG_E("AMediaCodec_dequeueOutputBuffer error: %zd", out_index);
            break;
        }
    }

    // 3. Loop control
    if (!isInputEosQueued || output_available) {
        // Use sendMessage to avoid recursion stack overflow
        sendMessage({kWhatRequestInputBuffers});
    }
}
