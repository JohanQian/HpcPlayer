#pragma once

#include <list>
#include "Handler.h"
#include "Error.h"

namespace hpc {

struct Message;

struct MediaClock : public Handler {
  enum {
    TIMER_REASON_REACHED = 0,
    TIMER_REASON_RESET = 1,
  };

  MediaClock();

  MediaClock(const MediaClock &) = delete;
  MediaClock &operator=(const MediaClock &) = delete;

  void init();

  void setStartingTimeMedia(int64_t startingTimeMediaUs);

  void clearAnchor();
  // It's required to use timestamp of just rendered frame as
  // anchor time in paused state.
  void updateAnchor(
      int64_t anchorTimeMediaUs,
      int64_t anchorTimeRealUs,
      int64_t maxTimeMediaUs = INT64_MAX);

  void updateMaxTimeMedia(int64_t maxTimeMediaUs);

  void setPlaybackRate(float rate);
  float getPlaybackRate() const;

  // query media time corresponding to real time |realUs|, and save the
  // result in |outMediaUs|.
  status_t getMediaTime(
      int64_t realUs,
      int64_t *outMediaUs,
      bool allowPastMaxTime = false) const;
  // query real time corresponding to media time |targetMediaUs|.
  // The result is saved in |outRealUs|.
  status_t getRealTimeFor(int64_t targetMediaUs, int64_t *outRealUs) const;

  // request to set up a timer. The target time is |mediaTimeUs|, adjusted by
  // system time of |adjustRealUs|. In other words, the wake up time is
  // mediaTimeUs + (adjustRealUs / playbackRate)
  void addTimer(const std::shared_ptr<Message> &notify, int64_t mediaTimeUs, int64_t adjustRealUs = 0);

  void setNotificationMessage(const std::shared_ptr<Message> &msg);

  void reset();

 protected:
  virtual ~MediaClock();

  virtual void onMessageReceived(const std::shared_ptr<Message> &msg);

 private:
  enum {
    kWhatTimeIsUp = 'tIsU',
  };

  struct Timer {
    Timer(const std::shared_ptr<Message> &notify, int64_t mediaTimeUs, int64_t adjustRealUs);
    const std::shared_ptr<Message> mNotify;
    int64_t mMediaTimeUs;
    int64_t mAdjustRealUs;
  };

  status_t getMediaTime_l(
      int64_t realUs,
      int64_t *outMediaUs,
      bool allowPastMaxTime) const;

  void processTimers_l();

  void updateAnchorTimesAndPlaybackRate_l(
      int64_t anchorTimeMediaUs, int64_t anchorTimeRealUs , float playbackRate);

  void notifyDiscontinuity_l();

  std::shared_ptr<Looper> mLooper;
  mutable std::mutex mLock;

  int64_t mAnchorTimeMediaUs;
  int64_t mAnchorTimeRealUs;
  int64_t mMaxTimeMediaUs;
  int64_t mStartingTimeMediaUs;

  float mPlaybackRate;

  int32_t mGeneration;
  std::list<Timer> mTimers;
  std::shared_ptr<Message> mNotify;

};

}


