#include "FfmpegVideoDecoder.h"
#include "common/HpcLog.h"
#include "common/MediaFormat.h"
#include "renderer/Renderer.h"
#include "common/Message.h"

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
    if (!codec) return;

    codec_context_ = avcodec_alloc_context3(codec);
    if (!codec_context_) return;

    codec_context_->width = format->width;
    codec_context_->height = format->height;
    if (!format->extradata.empty()) {
        codec_context_->extradata = static_cast<uint8_t*>(av_malloc(format->extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        memcpy(codec_context_->extradata, format->extradata.data(), format->extradata.size());
        codec_context_->extradata_size = format->extradata.size();
    }

    if (avcodec_open2(codec_context_, codec, nullptr) < 0) {
        doStop();
        return;
    }
    av_frame_ = av_frame_alloc();
}

void FfmpegVideoDecoder::doSetRenderer(const std::shared_ptr<Renderer>& renderer) {
    renderer_ = renderer;
}

void FfmpegVideoDecoder::doSetExtractor(const std::shared_ptr<Extractor>& extractor) {
    extractor_ = extractor;
}

void FfmpegVideoDecoder::doStart() {
    requestInputBuffers();
}

void FfmpegVideoDecoder::doStop() {
    // Cleanup logic
}

void FfmpegVideoDecoder::doFlush() {
    if (codec_context_) {
        avcodec_flush_buffers(codec_context_);
    }
    frame_queue_.Flush();
}

void FfmpegVideoDecoder::doRequestInputBuffers() {
    if (!extractor_ || !codec_context_) return;

    auto sample = extractor_->getSample(MediaType::VIDEO);
    if (!sample) return; 

    AVPacket* packet = av_packet_alloc();
    if (sample->is_eos) {
        avcodec_send_packet(codec_context_, nullptr);
    } else {
        av_packet_from_data(packet, sample->data.data(), sample->data.size());
        packet->pts = sample->pts;
        avcodec_send_packet(codec_context_, packet);
    }
    av_packet_free(&packet);

    while (true) {
        int ret = avcodec_receive_frame(codec_context_, av_frame_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break; 
        } else if (ret < 0) {
            break;
        }
        
        auto video_frame = std::make_shared<MediaFrame>();
        video_frame->pts = av_frame_->pts;
        // The actual frame data conversion would happen here
        frame_queue_.Push(video_frame);
    }

    if (!sample->is_eos) {
        sendMessage({kWhatRequestInputBuffers});
    }
}
