//
// Created by Administrator on 2025/4/6.
//

#ifndef HPCPLAYER_HPCPLAYER_H
#define HPCPLAYER_HPCPLAYER_H

#include <stdint.h>
#include "Handler.h"
#include "Error.h"

namespace hpc {
class ANativeWindow;
class HpcPlayerInternal;
class MediaClock;

enum media_event_type {
  MEDIA_NOP               = 0, // interface test message
  MEDIA_PREPARED          = 1,
  MEDIA_PLAYBACK_COMPLETE = 2,
  MEDIA_BUFFERING_UPDATE  = 3,
  MEDIA_SEEK_COMPLETE     = 4,
  MEDIA_SET_VIDEO_SIZE    = 5,
  MEDIA_STARTED           = 6,
  MEDIA_PAUSED            = 7,
  MEDIA_STOPPED           = 8,
  MEDIA_SKIPPED           = 9,
  MEDIA_NOTIFY_TIME       = 98,
  MEDIA_TIMED_TEXT        = 99,
  MEDIA_ERROR             = 100,
  MEDIA_INFO              = 200,
  MEDIA_SUBTITLE_DATA     = 201,
  MEDIA_META_DATA         = 202,
  MEDIA_TIME_DISCONTINUITY = 211,
  MEDIA_IMS_RX_NOTICE     = 300,
  MEDIA_AUDIO_ROUTING_CHANGED = 10000,
};

enum SeekMode : int32_t {
  SEEK_PREVIOUS_SYNC = 0,
  SEEK_NEXT_SYNC,
  SEEK_CLOSEST_SYNC,
  SEEK_CLOSEST,
  SEEK_FRAME_INDEX,
};


class HpcPlayer{
 public:
  status_t setDataSource(const char* url);
  status_t setSurface(ANativeWindow* window);
  status_t prepare();
  status_t start();
  status_t pause();
  status_t stop();
  status_t seekTo(int64_t mesc);
  status_t getCurrentPosition(int64_t *postion);
  status_t getDuration(int64_t *duration);
  bool isPlaying();
  void release();

  void notifySetDataSourceCompleted(status_t err);
//  void notifyPrepareCompleted(status_t err);
//  void notifyResetComplete();
//  void notifySetSurfaceComplete();
  void notifyDuration(int64_t durationUs);
//  void notifyMorePlayingTimeUs(int64_t timeUs);
//  void notifyMoreRebufferingTimeUs(int64_t timeUs);
//  void notifyRebufferingWhenExit(bool status);
//  void notifySeekComplete();
//  void notifySeekComplete_l();
//  void notifyListener(int msg, int ext1 = 0, int ext2 = 0);
//  void notifyFlagsChanged(uint32_t flags);

 protected:
  virtual ~HpcPlayer();

 private:
  enum State {
    STATE_IDLE,
    STATE_SET_DATASOURCE_PENDING,
    STATE_UNPREPARED,
    STATE_PREPARING,
    STATE_PREPARED,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_RESET_IN_PROGRESS,
    STATE_STOPPED,                  // equivalent to PAUSED
    STATE_STOPPED_AND_PREPARING,    // equivalent to PAUSED, but seeking
    STATE_STOPPED_AND_PREPARED,     // equivalent to PAUSED, but seek complete
  };
  std::string stateString(State state);

  mutable std::mutex mLock;
  std::condition_variable mCondition;

  State mState;

  bool mIsAsyncPrepare;
  status_t mAsyncResult;

  int64_t mDurationUs;
  int64_t mPositionUs;
  int64_t mPlayingTimeUs;

  std::shared_ptr<Looper> mLooper;
  const std::shared_ptr<MediaClock> mMediaClock;
  const std::shared_ptr<HpcPlayerInternal> mPlayer;
  uint32_t mPlayerFlags;

//  mutable Mutex mAudioSinkLock;
//  sp<AudioSink> mAudioSink GUARDED_BY(mAudioSinkLock);
//  int32_t mCachedPlayerIId GUARDED_BY(mAudioSinkLock);

  bool mAtEOS;
  bool mLooping;
  bool mAutoLoop;

  status_t prepare_l();
  status_t start_l();
  void notifyListener_l(int msg, int ext1 = 0, int ext2 = 0);
};

} // hpc

#endif //HPCPLAYER_HPCPLAYER_H
