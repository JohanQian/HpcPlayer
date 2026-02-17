#include "FfmpegExtractor.h"
#include "common/HpcLog.h"
#include "common/MediaFormat.h"
#include "common/Message.h"
#include "common/MediaSample.h"
#include "../HpcCore.h"

extern "C" {
#include "libavutil/error.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavcodec/bsf.h"
}

constexpr char kTag[] = "FfmpegExtractor";

namespace {
    std::vector<uint8_t> ConvertToAnnexB(AVCodecParameters* params) {
        const char* filter_name = nullptr;
        if (params->codec_id == AV_CODEC_ID_H264) filter_name = "h264_mp4toannexb";
        else if (params->codec_id == AV_CODEC_ID_HEVC) filter_name = "hevc_mp4toannexb";
        
        if (!filter_name) return {};

        const AVBitStreamFilter* bsf = av_bsf_get_by_name(filter_name);
        if (!bsf) return {};

        AVBSFContext* ctx = nullptr;
        if (av_bsf_alloc(bsf, &ctx) < 0) return {};

        avcodec_parameters_copy(ctx->par_in, params);
        if (av_bsf_init(ctx) < 0) {
            av_bsf_free(&ctx);
            return {};
        }

        std::vector<uint8_t> result;
        if (ctx->par_out->extradata_size > 0) {
            result.assign(ctx->par_out->extradata, ctx->par_out->extradata + ctx->par_out->extradata_size);
        }
        
        av_bsf_free(&ctx);
        return result;
    }
}

FfmpegExtractor::FfmpegExtractor(std::weak_ptr<HpcCore> core) 
    : Handler(std::make_shared<Looper>("FfmpegExtractor")), core(core)
{
    looper_->start();
    avformat_network_init();
}

FfmpegExtractor::~FfmpegExtractor() {
    looper_->stop();
    if (formatContext) {
        avformat_close_input(&formatContext);
    }
    if (bsfContext) {
        av_bsf_free(&bsfContext);
    }
    avformat_network_deinit();
}

void FfmpegExtractor::setDataSource(const std::string& path) {
    dataSourcePath = path;
}

void FfmpegExtractor::prepareAsync() {
    sendMessage({kWhatPrepare});
}

void FfmpegExtractor::start() {
    sendMessage({kWhatStart});
}

void FfmpegExtractor::pause() {
    sendMessage({kWhatPause});
}

void FfmpegExtractor::resume() {
    sendMessage({kWhatResume});
}

void FfmpegExtractor::stop() {
    sendMessage({kWhatStop});
}

void FfmpegExtractor::seekTo(int64_t msec) {
    sendMessage({.what = kWhatSeek, .arg1 = msec});
}

void FfmpegExtractor::postReadBuffer() {
    sendMessage({kWhatReadBuffer});
}

std::shared_ptr<MediaSample> FfmpegExtractor::getSample(MediaType type) {
    std::shared_ptr<MediaSample> sample = nullptr;
    bool success = false;
    if (type == MediaType::VIDEO) {
        success = videoPacketQueue.waitAndPop(&sample);
    } else if (type == MediaType::AUDIO) {
        success = audioPacketQueue.waitAndPop(&sample);
    }
    
    if (success) {
        return sample;
    }
    return nullptr;
}

size_t FfmpegExtractor::getTrackCount() {
    std::lock_guard<std::mutex> lock(mutex);
    return formatContext ? formatContext->nb_streams : 0;
}

std::shared_ptr<MediaFormat> FfmpegExtractor::getTrackFormat(size_t index) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!formatContext || index >= formatContext->nb_streams) return nullptr;
    
    AVCodecParameters* params = formatContext->streams[index]->codecpar;
    auto media_format = std::make_shared<MediaFormat>();
    media_format->codec_id = params->codec_id;
    
    // Map codec_id to mime_type
    switch (params->codec_id) {
        case AV_CODEC_ID_H264:
            media_format->mime_type = "video/avc";
            break;
        case AV_CODEC_ID_HEVC:
            media_format->mime_type = "video/hevc";
            break;
        case AV_CODEC_ID_VP8:
            media_format->mime_type = "video/x-vnd.on2.vp8";
            break;
        case AV_CODEC_ID_VP9:
            media_format->mime_type = "video/x-vnd.on2.vp9";
            break;
        case AV_CODEC_ID_AAC:
            media_format->mime_type = "audio/mp4a-latm";
            break;
        case AV_CODEC_ID_MP3:
            media_format->mime_type = "audio/mpeg";
            break;
        case AV_CODEC_ID_FLAC:
            media_format->mime_type = "audio/flac";
            break;
        default:
            // Fallback or unknown
            break;
    }

    if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
        media_format->width = params->width;
        media_format->height = params->height;
    } else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
        media_format->sample_rate = params->sample_rate;
        media_format->channels = params->channels;
    }
    
    if (params->extradata_size > 0) {
        if (params->codec_id == AV_CODEC_ID_H264 || params->codec_id == AV_CODEC_ID_HEVC) {
             auto annexb = ConvertToAnnexB(params);
             if (!annexb.empty()) {
                 media_format->extradata = std::move(annexb);
             } else {
                 media_format->extradata.assign(params->extradata, params->extradata + params->extradata_size);
             }
        } else {
            media_format->extradata.assign(params->extradata, params->extradata + params->extradata_size);
        }
    }
    return media_format;
}

void FfmpegExtractor::selectTrack(size_t index) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!formatContext || index >= formatContext->nb_streams) return;

    AVCodecParameters* params = formatContext->streams[index]->codecpar;
    if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
        videoStreamIndex = index;
        
        // Initialize Bitstream Filter
        const char* filter_name = nullptr;
        if (params->codec_id == AV_CODEC_ID_H264) filter_name = "h264_mp4toannexb";
        else if (params->codec_id == AV_CODEC_ID_HEVC) filter_name = "hevc_mp4toannexb";
        
        if (filter_name) {
            const AVBitStreamFilter* bsf = av_bsf_get_by_name(filter_name);
            if (bsf && av_bsf_alloc(bsf, &bsfContext) >= 0) {
                avcodec_parameters_copy(bsfContext->par_in, params);
                av_bsf_init(bsfContext);
            }
        }
    } else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
        audioStreamIndex = index;
    }
}

void FfmpegExtractor::onMessageReceived(const Message& msg) {
    switch (msg.what) {
        case kWhatPrepare: {
            doPrepare();
            break;
        }
        case kWhatStart: {
            doStart();
            break;
        }
        case kWhatPause: {
            doPause();
            break;
        }
        case kWhatResume: {
            doResume();
            break;
        }
        case kWhatStop: {
            doStop();
            break;
        }
        case kWhatSeek: {
            doSeekTo(msg.arg1);
            break;
        }
        case kWhatReadBuffer: {
            doReadBuffer();
            break;
        }
    }
}

void FfmpegExtractor::doPrepare() {
    std::lock_guard<std::mutex> lock(mutex);
    if (formatContext) {
        avformat_close_input(&formatContext);
    }
    formatContext = nullptr;
    int ret = avformat_open_input(&formatContext, dataSourcePath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        LOG_E("Failed to open input: %s", av_err2str(ret));
        notifyPrepared(ret);
        return;
    }
    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        LOG_E("Failed to find stream info: %s", av_err2str(ret));
        avformat_close_input(&formatContext);
        formatContext = nullptr;
        notifyPrepared(ret);
        return;
    }
    notifyPrepared(0);
}

void FfmpegExtractor::doStart() {
    isPlaying.store(true);
    videoPacketQueue.reset();
    audioPacketQueue.reset();
    postReadBuffer(); 
}

void FfmpegExtractor::doPause() {
    isPlaying.store(false);
}

void FfmpegExtractor::doResume() {
    isPlaying.store(true);
    postReadBuffer();
}

void FfmpegExtractor::doStop() {
    isPlaying.store(false);
    videoPacketQueue.abort();
    audioPacketQueue.abort();
}

void FfmpegExtractor::doSeekTo(int64_t msec) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!formatContext) return;
    videoPacketQueue.flush();
    audioPacketQueue.flush();
    int64_t seek_ts = av_rescale(msec, AV_TIME_BASE, 1000);
    av_seek_frame(formatContext, -1, seek_ts, AVSEEK_FLAG_BACKWARD);
    if (bsfContext) {
        av_bsf_flush(bsfContext);
    }
    isSeek = true;
    postReadBuffer();
}

void FfmpegExtractor::doReadBuffer() {
    if (!isPlaying.load()) return;

    AVPacket* packet = av_packet_alloc();
    int ret;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(!formatContext){
            av_packet_free(&packet);
            return;
        }
        ret = av_read_frame(formatContext, packet);
    }

    if (ret >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            if (bsfContext) {
                av_bsf_send_packet(bsfContext, packet);
                while (av_bsf_receive_packet(bsfContext, packet) == 0) {
                    auto sample = std::make_shared<MediaSample>();
                    sample->data.resize(packet->size);
                    memcpy(sample->data.data(), packet->data, packet->size);
                    sample->pts = av_rescale_q(packet->pts, formatContext->streams[packet->stream_index]->time_base, {1, 1000000});
                    videoPacketQueue.push(sample);
                    LOG_E("videoPacketQueue %d",videoPacketQueue.size());
                    av_packet_unref(packet);
                }
            } else {
                auto sample = std::make_shared<MediaSample>();
                sample->data.resize(packet->size);
                memcpy(sample->data.data(), packet->data, packet->size);
                sample->pts = av_rescale_q(packet->pts, formatContext->streams[packet->stream_index]->time_base, {1, 1000000});
                videoPacketQueue.push(sample);
            }
        } else if (packet->stream_index == audioStreamIndex) {
            auto sample = std::make_shared<MediaSample>();
            sample->data.resize(packet->size);
            memcpy(sample->data.data(), packet->data, packet->size);
            sample->pts = av_rescale_q(packet->pts, formatContext->streams[packet->stream_index]->time_base, {1, 1000000});
            if (isSeek) {
                sample->is_seek_frame = true;
                isSeek = false;
            }
            audioPacketQueue.push(sample);
            LOG_E("audioPacketQueue %d",audioPacketQueue.size());
        }
        av_packet_unref(packet);
        postReadBuffer(); 
    } else {
        if (ret == AVERROR_EOF) {
            auto eos_sample = std::make_shared<MediaSample>();
            eos_sample->is_eos = true;
            videoPacketQueue.push(eos_sample);
            audioPacketQueue.push(eos_sample);
            isPlaying.store(false);
        }
    }
    av_packet_free(&packet);
}

int64_t FfmpegExtractor::getDuration() {
    std::lock_guard<std::mutex> lock(mutex);
    if (formatContext && formatContext->duration != AV_NOPTS_VALUE) {
        return av_rescale(formatContext->duration, 1000, AV_TIME_BASE);
    }
    return 0;
}

void FfmpegExtractor::notifyPrepared(status_t err) {
    if (auto c = core.lock()) {
        auto msg = std::make_shared<Message>();
        msg->what = Extractor::kWhatPrepared;
        msg->arg1 = err;
        c->sendMessage({.what = HpcCore::kWhatSourceNotify, .obj = msg});
    }
}
