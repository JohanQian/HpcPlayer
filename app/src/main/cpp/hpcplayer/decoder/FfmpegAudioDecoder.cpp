#include "FfmpegAudioDecoder.h"
#include "common/HpcLog.h"
#include "common/MediaFormat.h"
#include "renderer/Renderer.h"
#include "common/Message.h"

namespace hpc {

constexpr char kTag[] = "FfmpegAudioDecoder";

FfmpegAudioDecoder::FfmpegAudioDecoder(const std::shared_ptr<Looper> &looper) : Decoder(
        looper) {}

FfmpegAudioDecoder::~FfmpegAudioDecoder() {
    doStop();
}

void FfmpegAudioDecoder::onMessageReceived(const Message &msg) {
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

void FfmpegAudioDecoder::doConfigure(const std::shared_ptr<MediaFormat> &format) {
    doStop();
    const AVCodec *codec = avcodec_find_decoder(static_cast<AVCodecID>(format->codec_id));
    if (!codec) {
        LOG_E("Failed to find audio decoder");
        return;
    }

    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        LOG_E("Failed to allocate AVCodecContext");
        return;
    }

    codecContext->channels = format->channels;
    codecContext->sample_rate = format->sample_rate;
    if (!format->extradata.empty()) {
        codecContext->extradata = static_cast<uint8_t *>(av_malloc(
                format->extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        memcpy(codecContext->extradata, format->extradata.data(), format->extradata.size());
        codecContext->extradata_size = format->extradata.size();
    }

    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        LOG_E("Failed to open audio codec");
        doStop();
        return;
    }

    avFrame = av_frame_alloc();
    swrContext = swr_alloc_set_opts(nullptr,
                                    targetChannelLayout, targetSampleFmt, targetSampleRate,
                                    av_get_default_channel_layout(codecContext->channels),
                                    codecContext->sample_fmt, codecContext->sample_rate, 0,
                                    nullptr);
    swr_init(swrContext);
}

void FfmpegAudioDecoder::doSetRenderer(const std::shared_ptr<Renderer> &renderer) {
    audioRenderer = renderer;
}

void FfmpegAudioDecoder::doSetExtractor(const std::shared_ptr<Extractor> &extractor) {
    extractor_ = extractor;
}

void FfmpegAudioDecoder::doStart() {
    requestInputBuffers();
}

void FfmpegAudioDecoder::doStop() {
    if (swrContext) {
        swr_free(&swrContext);
        swrContext = nullptr;
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

void FfmpegAudioDecoder::doFlush() {
    if (codecContext) {
        avcodec_flush_buffers(codecContext);
    }
    if (avFrame) {
        av_frame_unref(avFrame);
    }
}

void FfmpegAudioDecoder::doRequestInputBuffers() {

    if (!extractor_ || !codecContext || !avFrame) {
        return;
    }

    auto sample = extractor_->getSample(MediaType::AUDIO);
    LOG_E("extractor_->getSample");
    if (!sample) {
        return;
    }

    int ret = 0;
    if (sample->is_eos) {
        // Send flush packet
        AVPacket packet;
        av_init_packet(&packet);
        packet.data = nullptr;
        packet.size = 0;
        ret = avcodec_send_packet(codecContext, &packet);
    } else {
        AVPacket packet;
        av_init_packet(&packet);
        packet.data = sample->data.data();
        packet.size = static_cast<int>(sample->data.size());
        packet.pts = sample->pts;
        ret = avcodec_send_packet(codecContext, &packet);
    }

    if (ret < 0) {
        LOG_E("Error sending packet for decoding");
        return;
    }

    while (true) {
        ret = avcodec_receive_frame(codecContext, avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOG_E("Error during decoding");
            break;
        }
        int dst_nb_samples = av_rescale_rnd(
                swr_get_delay(swrContext, codecContext->sample_rate) +
                avFrame->nb_samples, targetSampleRate, codecContext->sample_rate, AV_ROUND_UP);

        auto audio_frame = std::make_shared<MediaFrame>();
        audio_frame->pts = avFrame->pts;
        audio_frame->sample_rate = targetSampleRate;
        audio_frame->channels = av_get_channel_layout_nb_channels(targetChannelLayout);

        int buffer_size = av_samples_get_buffer_size(nullptr, audio_frame->channels,
                                                     dst_nb_samples, targetSampleFmt, 1);
        audio_frame->data = std::make_unique<uint8_t[]>(buffer_size);

        uint8_t *out_data[1] = {audio_frame->data.get()};
        int converted_samples = swr_convert(swrContext, out_data, dst_nb_samples,
                                            (const uint8_t **) avFrame->data,
                                            avFrame->nb_samples);

        if (converted_samples < 0) {
            LOG_E("Error converting audio");
            continue;
        }
        audio_frame->size = av_samples_get_buffer_size(nullptr, audio_frame->channels,
                                                       converted_samples, targetSampleFmt, 1);
        audio_frame->is_seek_frame = sample->is_seek_frame;

        // Directly queue the buffer to the renderer
        if (audioRenderer) {
            audioRenderer->queueBuffer(audio_frame);
            LOG_E("extractor_->queueBuffer");
        }
    }

    if (!sample->is_eos) {
        sendMessage({kWhatRequestInputBuffers});
    }
}
}