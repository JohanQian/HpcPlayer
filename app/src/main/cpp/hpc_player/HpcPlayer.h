//
// Created by Administrator on 2025/4/6.
//

#ifndef HPCPLAYER_HPCPLAYER_H
#define HPCPLAYER_HPCPLAYER_H

#include <stdint.h>
#include "Handler.h"
#include "Error.h"
#include "foundation/BaseType.h"

namespace hpc {
class ANativeWindow;
class HpcPlayerInternal;
class MediaClock;
class Surface;

class HpcPlayer{
 public:
  status_t setDataSource(const char* url);
  status_t setSurface(Surface* surface);
  status_t prepare();
  status_t start();
  status_t pause();
  status_t stop();
  status_t seekTo(
      int64_t seekTimeUs,
      SeekMode mode = SEEK_PREVIOUS_SYNC,
      bool needNotify = false);
  status_t getCurrentPosition(int64_t *postion);
  status_t getDuration(int64_t *duration);
  bool isPlaying();
  void release();

  void notifySetDataSourceCompleted(status_t err);
  void notifyPrepareCompleted(status_t err);
  void notifyResetComplete();
  void notifySetSurfaceComplete();
  void notifyDuration(int64_t durationUs);
  void notifyPlayingTimeUs(int64_t timeUs);
  void notifySeekComplete();
  void notifySeekComplete_l();
  void notifyListener(int msg, int ext1 = 0, int ext2 = 0);

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
  
  bool mAtEOS;
  bool mLooping;
  bool mAutoLoop;

  status_t prepare_l();
  status_t start_l();
  void notifyListener_l(int msg, int ext1 = 0, int ext2 = 0);
};

} // hpc

#endif //HPCPLAYER_HPCPLAYER_H
