#include "FfmpegExtractor.h"
#include "../common/HpcLog.h"
#include "../decoder/Decoder.h"
#include "../common/MediaSample.h"

extern "C" {
#include "libavutil/error.h"
}

namespace {
    constexpr char kTag[] = "FfmpegExtractor";

    HpcStatus FfmpegErrorToHpcStatus(int err) {
        if (err >= 0) return HpcStatus::kOk;
        if (err == AVERROR_EOF) return HpcStatus::kEof;
        if (err == AVERROR(ENOMEM)) return HpcStatus::kNoMemory;
        if (err == AVERROR(EINVAL)) return HpcStatus::kBadValue;
        return HpcStatus::kUnknownError;
    }
} // namespace

FfmpegExtractor::FfmpegExtractor()
    : Handler(looper_) {
    looper_ = std::make_shared<Looper>("FfmpegExtractor");
    looper_->start();
    avformat_network_init();
}

FfmpegExtractor::~FfmpegExtractor() {
    Stop(); // Ensure the loop is stopped
    looper_->stop();
    if (format_context_) {
        avformat_close_input(&format_context_);
    }
    avformat_network_deinit();
}

void FfmpegExtractor::SetDecoder(const std::shared_ptr<Decoder>& decoder) {
    decoders_.push_back(decoder);
}

void FfmpegExtractor::SetDataSource(const std::string& path) {
    sendMessage({.what = kWhatSetDataSource, .arg1 = reinterpret_cast<intptr_t>(new std::string(path))});
}

void FfmpegExtractor::Prepare() {
    sendMessage({.what = kWhatPrepare});
}

void FfmpegExtractor::Start(int32_t streamIndex) {
    sendMessage({.what = kWhatStart, .arg1 = streamIndex});
}

void FfmpegExtractor::Stop() {
    sendMessage({.what = kWhatStop});
}

void FfmpegExtractor::SeekTo(int64_t msec) {
    sendMessage({.what = kWhatSeek, .arg1 = static_cast<intptr_t>(msec)});
}

int32_t FfmpegExtractor::GetStreamCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return format_context_ ? format_context_->nb_streams : 0;
}

std::shared_ptr<MediaFormat> FfmpegExtractor::GetStreamFormat(int32_t streamIndex) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!format_context_ || streamIndex >= format_context_->nb_streams) {
        return nullptr;
    }
    AVStream* stream = format_context_->streams[streamIndex];
    auto media_format = std::make_shared<MediaFormat>();

    // Copy codec parameters
    media_format->codec_params = stream->codecpar;

    // You might want to populate more fields from AVStream or AVCodecParameters
    // For example:
    // media_format->SetInt(MediaFormat::KEY_WIDTH, stream->codecpar->width);
    // media_format->SetInt(MediaFormat::KEY_HEIGHT, stream->codecpar->height);
    
    return media_format;
}

int32_t FfmpegExtractor::FindBestStream(int32_t mediaType) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!format_context_) return -1;
    return av_find_best_stream(format_context_, (AVMediaType)mediaType, -1, -1, nullptr, 0);
}

int64_t FfmpegExtractor::GetDuration() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!format_context_) return 0;
    // FFmpeg duration is in AV_TIME_BASE units (microseconds), convert to milliseconds
    return format_context_->duration / 1000;
}

void FfmpegExtractor::onMessageReceived(const Message& msg) {
    switch (msg.what) {
        case kWhatSetDataSource: {
            auto* path = reinterpret_cast<std::string*>(msg.arg1);
            DoSetDataSource(*path);
            delete path;
            break;
        }
        case kWhatPrepare: DoPrepare(); break;
        case kWhatStart: DoStart(msg.arg1); break;
        case kWhatStop: DoStop(); break;
        case kWhatSeek: DoSeekTo(msg.arg1); break;
        case kWhatDoLoop: DoLoop(); break;
    }
}

void FfmpegExtractor::DoSetDataSource(const std::string& path) {
    data_source_path_ = path;
}

void FfmpegExtractor::DoPrepare() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (format_context_) {
        avformat_close_input(&format_context_);
        format_context_ = nullptr;
    }
    int ret = avformat_open_input(&format_context_, data_source_path_.c_str(), nullptr, nullptr);
    if (ret < 0) {
        HPC_LOG_ERROR(kTag, "Failed to open input: %s", av_err2str(ret));
        return;
    }
    ret = avformat_find_stream_info(format_context_, nullptr);
    if (ret < 0) {
        HPC_LOG_ERROR(kTag, "Failed to find stream info: %s", av_err2str(ret));
        avformat_close_input(&format_context_);
        format_context_ = nullptr;
    }
}

void FfmpegExtractor::DoStart(int32_t streamIndex) {
    selected_stream_index_ = streamIndex;
    is_playing_.store(true);
    is_running_.store(true);
    post({.what = kWhatDoLoop});
}

void FfmpegExtractor::DoStop() {
    is_playing_.store(false);
}

void FfmpegExtractor::DoSeekTo(int64_t msec) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!format_context_) return;

    int64_t seek_ts = av_rescale(msec, AV_TIME_BASE, 1000);
    av_seek_frame(format_context_, -1, seek_ts, AVSEEK_FLAG_BACKWARD);

    for(const auto& decoder: decoders_){
       // decoder->PostMessage(kMsgFlush); // This needs to be adapted to the new Decoder interface
    }

    if (!is_playing_.load()) {
        is_playing_.store(true);
        post({.what = kWhatDoLoop});
    }
}

void FfmpegExtractor::DoLoop() {
    if (!is_running_.load() || !is_playing_.load()) {
        return;
    }

    AVPacket* packet = av_packet_alloc();
    int ret;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!format_context_){
            av_packet_free(&packet);
            return;
        }
        ret = av_read_frame(format_context_, packet);
    }


    if (ret >= 0) {
        if (packet->stream_index == selected_stream_index_) {
            auto sample = std::make_shared<MediaSample>();
            // av_packet_ref(sample->packet, packet); // A better way to avoid copying
            
            // This is a deep copy, might be slow. Consider using av_packet_ref.
            sample->AllocateBuffer(packet->size);
            memcpy(sample->GetData(), packet->data, packet->size);
            sample->pts = av_rescale_q(packet->pts, format_context_->streams[packet->stream_index]->time_base, {1, 1000000});

            for (const auto& decoder : decoders_) {
                decoder->Decode(sample);
            }
        }
        av_packet_unref(packet);
        post({.what = kWhatDoLoop});
    } else {
        for (const auto& decoder : decoders_) {
           // decoder->PostMessage(kMsgEOS);
        }
        is_playing_.store(false);
    }
    av_packet_free(&packet);
}
