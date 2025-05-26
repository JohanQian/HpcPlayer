//
// Created by Administrator on 2025/4/6.
//

#include "HpcPlayer.h"
#include "HpcPlayerInternal.h"
#include "Log.h"

#define LOG_TAG "HpcPlayer"

namespace hpc {
HpcPlayer::~HpcPlayer() {
  ALOGV("~NuPlayerDriver(%p)", this);
  mLooper->stop();
}

status_t HpcPlayer::setDataSource(const char *url) {
  if (mState != STATE_IDLE) {
    return INVALID_OPERATION;
  }
  std::unique_lock lck(mLock);
  mState = STATE_SET_DATASOURCE_PENDING;

  mPlayer->setDataSourceAsync(url);

  mCondition.wait(lck,[this](){return mState == STATE_SET_DATASOURCE_PENDING;});

  return mAsyncResult;
}

status_t HpcPlayer::setSurface(ANativeWindow *window) {
  ALOGV("setVideoSurfaceTexture(%p)", this);
  std::lock_guard autoLock(mLock);

  switch (mState) {
    case STATE_SET_DATASOURCE_PENDING:
    case STATE_RESET_IN_PROGRESS:
      return INVALID_OPERATION;

    default:
      break;
  }

  mPlayer->setVideoSurface(window);
  return OK;
}

status_t HpcPlayer::prepare() {
  ALOGV("prepare(%p)", this);
  std::lock_guard autoLock(mLock);
  status_t ret = prepare_l();
  return ret;
}

status_t HpcPlayer::prepare_l() {
  std::unique_lock lck(mLock);
  switch (mState) {
    case STATE_UNPREPARED:
      mState = STATE_PREPARING;

      // Make sure we're not posting any notifications, success or
      // failure information is only communicated through our result
      // code.
      mIsAsyncPrepare = false;
      mPlayer->prepareAsync();
      mCondition.wait(lck,[this](){return mState == STATE_SET_DATASOURCE_PENDING;});
      return (mState == STATE_PREPARED) ? OK : UNKNOWN_ERROR;
    case STATE_STOPPED:
      // this is really just paused. handle as seek to start
      mAtEOS = false;
      mState = STATE_STOPPED_AND_PREPARING;
      mIsAsyncPrepare = false;
      mPlayer->seekTo(0);
      mCondition.wait(lck,[this](){return mState == STATE_STOPPED_AND_PREPARING;});
      return (mState == STATE_STOPPED_AND_PREPARED) ? OK : UNKNOWN_ERROR;
    default:
      return INVALID_OPERATION;
  };
}

status_t HpcPlayer::start() {
  ALOGV("start(%p), state is %d, eos is %d", this, mState, mAtEOS);
  std::lock_guard autoLock(mLock);
  status_t ret = start_l();
  return ret;
}

status_t HpcPlayer::start_l() {
  switch (mState) {
    case STATE_UNPREPARED:
    {
      status_t err = prepare_l();

      if (err != OK) {
        return err;
      }

      if (mState != STATE_PREPARED) {
        return UNKNOWN_ERROR;
      }
    }

    case STATE_PAUSED:
    case STATE_STOPPED_AND_PREPARED:
    case STATE_PREPARED:
    {
      mPlayer->start();

      break;
    }

    case STATE_RUNNING:
    {
      if (mAtEOS) {
        mPlayer->seekTo(0);
        mAtEOS = false;
        mPositionUs = -1;
      }
      break;
    }

    default:
      return INVALID_OPERATION;
  }

  mState = STATE_RUNNING;

  return OK;
}

status_t HpcPlayer::pause() {
  ALOGD("pause(%p)", this);
  // The NuPlayerRenderer may get flushed if pause for long enough, e.g. the pause timeout tear
  // down for audio offload mode. If that happens, the NuPlayerRenderer will no longer know the
  // current position. So similar to seekTo, update |mPositionUs| to the pause position by calling
  // getCurrentPosition here.
//  int unused;
//  getCurrentPosition(&unused);

  std::lock_guard autoLock(mLock);

  switch (mState) {
    case STATE_PAUSED:
    case STATE_PREPARED:
      return OK;

    case STATE_RUNNING:
      mState = STATE_PAUSED;
      notifyListener_l(MEDIA_PAUSED);
      mPlayer->pause();
      break;

    default:
      return INVALID_OPERATION;
  }

  return OK;
}

status_t HpcPlayer::stop() {
  ALOGD("stop(%p)", this);
  std::lock_guard autoLock(mLock);

  switch (mState) {
    case STATE_RUNNING:
      mPlayer->pause();
      break;

    case STATE_PAUSED:
      mState = STATE_STOPPED;
      notifyListener_l(MEDIA_STOPPED);
      break;

    case STATE_PREPARED:
    case STATE_STOPPED:
    case STATE_STOPPED_AND_PREPARING:
    case STATE_STOPPED_AND_PREPARED:
      mState = STATE_STOPPED;
      break;

    default:
      return INVALID_OPERATION;
  }

  return OK;
}

status_t HpcPlayer::seekTo(int64_t msec) {
  ALOGV("seekTo(%p) (%d ms, %d) at state %d", this, msec, mode, mState);
  std::lock_guard autoLock(mLock);

  int64_t seekTimeUs = msec * 1000LL;

  switch (mState) {
    case STATE_PREPARED:
    case STATE_STOPPED_AND_PREPARED:
    case STATE_PAUSED:
    case STATE_RUNNING:
    {
      mAtEOS = false;
      // seeks can take a while, so we essentially paused
      notifyListener_l(MEDIA_PAUSED);
      mPlayer->seekTo(seekTimeUs);
      break;
    }

    default:
      return INVALID_OPERATION;
  }

  mPositionUs = seekTimeUs;
  return OK;
}

status_t HpcPlayer::getCurrentPosition(int64_t *postionMs) {
  int64_t tempUs = 0;
  {
    std::lock_guard autoLock(mLock);
    if (mState == STATE_PAUSED && !mAtEOS) {
      tempUs = (mPositionUs <= 0) ? 0 : mPositionUs;
      *postionMs =  (tempUs + 500LL) / 1000;
      return OK;
    }
  }

  status_t ret = mPlayer->getCurrentPosition(&tempUs);

  std::lock_guard autoLock(mLock);
  // We need to check mSeekInProgress here because mPlayer->seekToAsync is an async call, which
  // means getCurrentPosition can be called before seek is completed. Iow, renderer may return a
  // position value that's different the seek to position.
  if (ret != OK) {
    tempUs = (mPositionUs <= 0) ? 0 : mPositionUs;
  } else {
    mPositionUs = tempUs;
  }
  *postionMs = (tempUs + 500LL) / 1000;
  return OK;
}

status_t HpcPlayer::getDuration(int64_t *durationMs) {
  std::lock_guard autoLock(mLock);

  if (mDurationUs < 0) {
    return UNKNOWN_ERROR;
  }

  *durationMs = (mDurationUs + 500LL) / 1000;

  return OK;
}

bool HpcPlayer::isPlaying() {
  return mState == STATE_RUNNING && !mAtEOS;
}

status_t HpcPlayer::release() {

}

std::string HpcPlayer::stateString(HpcPlayer::State state) {
  return std::string();
}

void HpcPlayer::notifySetDataSourceCompleted(status_t err) {

}

void HpcPlayer::notifyDuration(int64_t durationUs) {

}

} // hpc