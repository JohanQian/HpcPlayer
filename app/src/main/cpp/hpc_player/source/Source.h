#pragma once

#include "../HpcPlayer.h"
#include "Handler.h"
#include "Error.h"

namespace hpc {

struct Message;
struct MetaData;

enum media_track_type {
  MEDIA_TRACK_TYPE_UNKNOWN = 0,
  MEDIA_TRACK_TYPE_VIDEO = 1,
  MEDIA_TRACK_TYPE_AUDIO = 2,
  MEDIA_TRACK_TYPE_TIMEDTEXT = 3,
  MEDIA_TRACK_TYPE_SUBTITLE = 4,
  MEDIA_TRACK_TYPE_METADATA = 5,
};

enum {
  kWhatPrepared,
  kWhatFlagsChanged,
  kWhatVideoSizeChanged,
  kWhatPauseOnBufferingStart,
  kWhatResumeOnBufferingEnd,
  kWhatCacheStats,
  kWhatSubtitleData,
  kWhatTimedTextData,
  kWhatTimedMetaData,
  kWhatQueueDecoderShutdown,
  kWhatInstantiateSecureDecoders,
};


class Source : public Handler {
 public:
  // The provides message is used to notify the player about various
  // events.
  explicit Source(const std::shared_ptr <Message> &notify)
      : mNotify(notify) {
  }
  virtual ~Source() = default;

  Source(const Source &) = delete;
  Source &operator=(const Source &) = delete;
  virtual void prepareAsync() {};

  virtual void start() {};
  virtual void stop() {}
  virtual void pause() {}
  virtual void resume() {}

  // Explicitly disconnect the underling data source
  virtual void disconnect() {}

  virtual std::shared_ptr<MetaData> getFormatMeta(bool /* audio */) { return nullptr; }

//  virtual status_t dequeueAccessUnit(
//      bool audio, std::shared_ptr<ABuffer> *accessUnit) = 0;

  virtual status_t getDuration(int64_t * /* durationUs */) {
    return INVALID_OPERATION;
  }

  virtual size_t getTrackCount() const {
    return 0;
  }

  virtual std::shared_ptr<Message> getTrackInfo(size_t /* trackIndex */) const {
    return nullptr;
  }

  virtual status_t seekTo(
      int64_t /* seekTimeUs */,
      SeekMode mode = SEEK_PREVIOUS_SYNC) {
    return INVALID_OPERATION;
  }

  virtual bool isRealTime() const {
    return false;
  }

  virtual bool isStreaming() const {
    return true;
  }


  std::shared_ptr<Message> dupNotify() const;
  void onMessageReceived(const std::shared_ptr<Message> &msg) override;
  void notifyFlagsChanged(uint32_t flags);
  void notifyVideoSizeChanged(const std::shared_ptr<Message> &format = nullptr);
  void notifyInstantiateSecureDecoders(const std::shared_ptr<Message> &reply);
  void notifyPrepared(status_t err = OK);

 private:
  std::shared_ptr<Message> mNotify;
};
}