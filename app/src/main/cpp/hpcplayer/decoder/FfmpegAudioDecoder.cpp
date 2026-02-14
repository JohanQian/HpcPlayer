#include "FfmpegAudioDecoder.h"
#include "common/HpcLog.h"
#include "common/MediaFormat.h"
#include "renderer/Renderer.h"
#include "common/Message.h"

namespace {
    constexpr char kTag[] = "FfmpegAudioDecoder";
}

FfmpegAudioDecoder::FfmpegAudioDecoder(const std::shared_ptr<Looper>& looper) : Decoder(looper) {}

FfmpegAudioDecoder::~FfmpegAudioDecoder() {
    doStop();
}

void FfmpegAudioDecoder::onMessageReceived(const Message& msg) {
    switch (msg.what) {
        case kWhatConfigure: {
            auto format = std::static_pointer_cast<MediaFormat>(msg.obj);
            doConfigure(format);
            break;
        }
        case kWhatSetRenderer: {
            auto renderer = std::static_pointer_cast<Renderer>(msg.obj);
            doSetRenderer(renderer);
            break;
        }
        case kWhatSetExtractor: {
            auto extractor = std::static_pointer_cast<Extractor>(msg.obj);
            doSetExtractor(extractor);
            break;
        }
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

void FfmpegAudioDecoder::doConfigure(const std::shared_ptr<MediaFormat>& format) {
    const AVCodec* codec = avcodec_find_decoder(static_cast<AVCodecID>(format->codec_id));
    if (!codec) {
        LOG_E("Failed to find audio decoder");
         return;
    }

    codec_context_ = avcodec_alloc_context3(codec);
    if (!codec_context_) {
        LOG_E("Failed to allocate AVCodecContext");
        return;
    }
    
    codec_context_->channels = format->channels;
    codec_context_->sample_rate = format->sample_rate;
    if (!format->extradata.empty()) {
        codec_context_->extradata = static_cast<uint8_t*>(av_malloc(format->extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        memcpy(codec_context_->extradata, format->extradata.data(), format->extradata.size());
        codec_context_->extradata_size = format->extradata.size();
    }

    if (avcodec_open2(codec_context_, codec, nullptr) < 0) {
        LOG_E("Failed to open audio codec");
        doStop();
        return;
    }

    av_frame_ = av_frame_alloc();
    swr_context_ = swr_alloc_set_opts(nullptr,
                                      target_channel_layout_, target_sample_fmt_, target_sample_rate_,
                                      av_get_default_channel_layout(codec_context_->channels),
                                      codec_context_->sample_fmt, codec_context_->sample_rate, 0, nullptr);
    swr_init(swr_context_);
}

void FfmpegAudioDecoder::doSetRenderer(const std::shared_ptr<Renderer>& renderer) {
    renderer_ = renderer;
}

void FfmpegAudioDecoder::doSetExtractor(const std::shared_ptr<Extractor>& extractor) {
    extractor_ = extractor;
}

void FfmpegAudioDecoder::doStart() {
    requestInputBuffers();
}

void FfmpegAudioDecoder::doStop() {
    if (swr_context_) {
        swr_free(&swr_context_);
        swr_context_ = nullptr;
    }
    if (av_frame_) {
        av_frame_free(&av_frame_);
        av_frame_ = nullptr;
    }
    if (codec_context_) {
        if (codec_context_->extradata) {
            av_freep(&codec_context_->extradata);
        }
        avcodec_free_context(&codec_context_);
        codec_context_ = nullptr;
    }
}

void FfmpegAudioDecoder::doFlush() {
    if (codec_context_) {
        avcodec_flush_buffers(codec_context_);
    }
    frame_queue_.Flush();
}

void FfmpegAudioDecoder::doRequestInputBuffers() {

    if (!extractor_) {
        return;
    }

    auto sample = extractor_->getSample(MediaType::AUDIO);
    if (!sample) {
        return;
    }

    int ret = 0;
    if (sample->is_eos) {
        ret = avcodec_send_packet(codec_context_, nullptr);
    } else {
        AVPacket packet;
        av_init_packet(&packet);
        packet.data = sample->data.data();
        packet.size = static_cast<int>(sample->data.size());
        packet.pts = sample->pts;
        ret = avcodec_send_packet(codec_context_, &packet);
    }

    if (ret < 0) {
        LOG_E("Error sending packet for decoding");
        return;
    }

    while (true) {
        ret = avcodec_receive_frame(codec_context_, av_frame_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOG_E("Error during decoding");
            break;
        }

        int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_context_, codec_context_->sample_rate) +
                                            av_frame_->nb_samples, target_sample_rate_, codec_context_->sample_rate, AV_ROUND_UP);
        
        auto audio_frame = std::make_shared<MediaFrame>();
        audio_frame->pts = av_frame_->pts;
        audio_frame->sample_rate = target_sample_rate_;
        audio_frame->channels = av_get_channel_layout_nb_channels(target_channel_layout_);
        
        int buffer_size = av_samples_get_buffer_size(nullptr, audio_frame->channels, dst_nb_samples, target_sample_fmt_, 1);
        audio_frame->data = std::make_unique<uint8_t[]>(buffer_size);
        
        uint8_t* out_data[1] = { audio_frame->data.get() };
        int converted_samples = swr_convert(swr_context_, out_data, dst_nb_samples,
                                            (const uint8_t**)av_frame_->data, av_frame_->nb_samples);
        
        if (converted_samples < 0) {
             LOG_E("Error converting audio");
             continue;
        }
        
        audio_frame->size = av_samples_get_buffer_size(nullptr, audio_frame->channels, converted_samples, target_sample_fmt_, 1);
        audio_frame->is_seek_frame = sample->is_seek_frame;
        LOG_E("qjtest frame_queue_ %ld ",audio_frame->pts);
        frame_queue_.Push(audio_frame);
    }

    if (!sample->is_eos) {
        sendMessage({kWhatRequestInputBuffers});
    }
}
