#include "FfmpegVideoDecoder.h"
#include "common/HpcLog.h"
#include "common/MediaFormat.h"
#include "renderer/Renderer.h"
#include "common/Message.h"
#include "common/MediaSample.h"

namespace {
    constexpr char kTag[] = "FfmpegVideoDecoder";
}

FfmpegVideoDecoder::FfmpegVideoDecoder(const std::shared_ptr<Looper>& looper) : Decoder(looper) {}

FfmpegVideoDecoder::~FfmpegVideoDecoder() {
    doStop();
}

void FfmpegVideoDecoder::onMessageReceived(const Message& msg) {
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
    }
}

void FfmpegVideoDecoder::doConfigure(const std::shared_ptr<MediaFormat>& format) {
    const AVCodec* codec = avcodec_find_decoder(static_cast<AVCodecID>(format->codec_id));
    if (!codec) {
        LOG_E("Failed to find video decoder");
        return;
    }

    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        LOG_E("Failed to allocate AVCodecContext");
        return;
    }

    codecContext->width = format->width;
    codecContext->height = format->height;
    if (!format->extradata.empty()) {
        codecContext->extradata = static_cast<uint8_t*>(av_malloc(format->extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        memcpy(codecContext->extradata, format->extradata.data(), format->extradata.size());
        codecContext->extradata_size = format->extradata.size();
    }

    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        LOG_E("Failed to open video codec");
        doStop();
        return;
    }
    avFrame = av_frame_alloc();
}

void FfmpegVideoDecoder::doSetRenderer(const std::shared_ptr<Renderer>& renderer) {
    renderer_ = renderer;
}

void FfmpegVideoDecoder::doSetExtractor(const std::shared_ptr<Extractor>& extractor) {
    extractor_ = extractor;
}

void FfmpegVideoDecoder::doStart() {
    sendMessage({kWhatRequestInputBuffers});
}

void FfmpegVideoDecoder::doStop() {
    if (swsContext) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
    if (avFrame) {
        av_frame_free(&avFrame);
        avFrame = nullptr;
    }
    if (codecContext) {
        if (codecContext->extradata) {
            av_freep(&codecContext->extradata);
        }
        avcodec_free_context(&codecContext);
        codecContext = nullptr;
    }
}

void FfmpegVideoDecoder::doFlush() {
    if (codecContext) {
        avcodec_flush_buffers(codecContext);
    }
    frameQueue.flush();
}

void FfmpegVideoDecoder::doRequestInputBuffers() {
    if (!extractor_ || !codecContext) return;

    auto sample = extractor_->getSample(MediaType::VIDEO);
    if (!sample) return; 

    AVPacket* packet = av_packet_alloc();
    if (sample->is_eos) {
        avcodec_send_packet(codecContext, nullptr);
    } else {
        if (av_new_packet(packet, sample->data.size()) == 0) {
            memcpy(packet->data, sample->data.data(), sample->data.size());
            packet->pts = sample->pts;
            avcodec_send_packet(codecContext, packet);
        }
    }
    av_packet_free(&packet);

    while (true) {
        int ret = avcodec_receive_frame(codecContext, avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break; 
        } else if (ret < 0) {
            break;
        }
        
        auto video_frame = std::make_shared<MediaFrame>();
        video_frame->pts = avFrame->pts;
        // The actual frame data conversion would happen here
        // For now just pushing empty frame or placeholder
        frameQueue.push(video_frame);
    }

    if (!sample->is_eos) {
        sendMessage({kWhatRequestInputBuffers});
    }
}
