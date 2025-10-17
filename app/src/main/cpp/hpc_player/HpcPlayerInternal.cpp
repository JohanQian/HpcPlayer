#include "HpcPlayerInternal.h"
#include "foundation/Message.h"
#include "foundation/Log.h"
#include "foundation/MetaData.h"
#include "foundation/Surface.h"
#include "source/Source.h"
#include "source/DefaultSource.h"
#include "DecoderBase.h"
#include "HpcPlayer.h"
#include <android/native_window.h>
#include "HpcPlayer.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#define LOG_TAG "HpcPlayerInternal"

namespace hpc {

struct HpcPlayerInternal::Action {
  Action() = default;

  virtual void execute(HpcPlayerInternal *player) = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(Action);
};

struct HpcPlayerInternal::SeekAction : public Action {
  explicit SeekAction(int64_t seekTimeUs, SeekMode mode)
      : mSeekTimeUs(seekTimeUs),
        mMode(mode) {
  }

  void execute(HpcPlayerInternal *player) override {
    player->performSeek(mSeekTimeUs, mMode);
  }

 private:
  int64_t mSeekTimeUs;
  SeekMode mMode;

  DISALLOW_EVIL_CONSTRUCTORS(SeekAction);
};

struct HpcPlayerInternal::ResumeDecoderAction : public Action {
  explicit ResumeDecoderAction(bool needNotify)
      : mNeedNotify(needNotify) {
  }

  void execute(HpcPlayerInternal *player) override {
    player->performResumeDecoders(mNeedNotify);
  }

 private:
  bool mNeedNotify;

  DISALLOW_EVIL_CONSTRUCTORS(ResumeDecoderAction);
};

struct HpcPlayerInternal::SetSurfaceAction : public Action {
  explicit SetSurfaceAction(const std::shared_ptr<Surface> &surface)
      : mSurface(surface) {
  }

  virtual void execute(HpcPlayerInternal *player) {
    player->performSetSurface(mSurface);
  }

 private:
  std::shared_ptr<Surface> mSurface;

  DISALLOW_EVIL_CONSTRUCTORS(SetSurfaceAction);
};

struct HpcPlayerInternal::FlushDecoderAction : public Action {
  FlushDecoderAction(FlushCommand audio, FlushCommand video)
      : mAudio(audio),
        mVideo(video) {
  }

  virtual void execute(HpcPlayerInternal *player) {
    player->performDecoderFlush(mAudio, mVideo);
  }

 private:
  FlushCommand mAudio;
  FlushCommand mVideo;

  DISALLOW_EVIL_CONSTRUCTORS(FlushDecoderAction);
};

struct HpcPlayerInternal::PostMessageAction : public Action {
  explicit PostMessageAction(const std::shared_ptr<Message> &msg)
      : mMessage(msg) {
  }

  virtual void execute(HpcPlayerInternal *) {
    mMessage->post();
  }

 private:
  std::shared_ptr<Message> mMessage;

  DISALLOW_EVIL_CONSTRUCTORS(PostMessageAction);
};

// Use this if there's no state necessary to save in order to execute
// the action.
struct HpcPlayerInternal::SimpleAction : public Action {
  typedef void (HpcPlayerInternal::*ActionFunc)();

  explicit SimpleAction(ActionFunc func)
      : mFunc(func) {
  }

  virtual void execute(HpcPlayerInternal *player) {
    (player->*mFunc)();
  }

 private:
  ActionFunc mFunc;

  DISALLOW_EVIL_CONSTRUCTORS(SimpleAction);
};

HpcPlayerInternal::HpcPlayerInternal(const std::shared_ptr<MediaClock> &mediaClock) {

}

HpcPlayerInternal::~HpcPlayerInternal() {

}

void HpcPlayerInternal::setDataSourceAsync(const char *url) {
  std::shared_ptr<Message> msg = std::make_shared<Message>(kWhatSetDataSource, shared_from_this());
  std::shared_ptr<Message> notify = std::make_shared<Message>(kWhatSourceNotify, shared_from_this());

  auto* source = new DefaultSource(notify, mUIDValid, mUID, mMediaClock);
  status_t err = source->setDataSource(url);

  if (err != OK) {
    ALOGE("Failed to set data source!");
    source = nullptr;
  }

  msg->setObject("source", source);
  msg->post();
}


status_t HpcPlayerInternal::setVideoSurface(Surface *surface) {
  return 0;
}

void HpcPlayerInternal::flushDecoder(bool audio, bool needShutdown) {
  ALOGV("[%s] flushDecoder needShutdown=%d",
        audio ? "audio" : "video", needShutdown);

  std::shared_ptr<Decoder> decoder = getDecoder(audio);
  if (decoder == nullptr) {
    ALOGI("flushDecoder %s without decoder present",
          audio ? "audio" : "video");
    return;
  }

  // Make sure we don't continue to scan sources until we finish flushing.
  ++mScanSourcesGeneration;
  if (mScanSourcesPending) {
    if (!needShutdown) {
      mDeferredActions.push_back(
          std::make_shared<SimpleAction>(&HpcPlayerInternal::performScanSources));
    }
    mScanSourcesPending = false;
  }

  decoder->signalFlush();

  FlushStatus newStatus =
      needShutdown ? FLUSHING_DECODER_SHUTDOWN : FLUSHING_DECODER;

  mFlushComplete[audio][false /* isDecoder */] = (mRenderer == NULL);
  mFlushComplete[audio][true /* isDecoder */] = false;
  if (audio) {
    ALOGE_IF(mFlushingAudio != NONE,
             "audio flushDecoder() is called in state %d", mFlushingAudio);
    mFlushingAudio = newStatus;
  } else {
    ALOGE_IF(mFlushingVideo != NONE,
             "video flushDecoder() is called in state %d", mFlushingVideo);
    mFlushingVideo = newStatus;
  }
}

void HpcPlayerInternal::processDeferredActions() {
  while (!mDeferredActions.empty()) {
    // We won't execute any deferred actions until we're no longer in
    // an intermediate state, i.e. one more more decoders are currently
    // flushing or shutting down.

    if (mFlushingAudio != NONE || mFlushingVideo != NONE) {
      // We're currently flushing, postpone the reset until that's
      // completed.

      ALOGV("postponing action mFlushingAudio=%d, mFlushingVideo=%d",
            mFlushingAudio, mFlushingVideo);

      break;
    }

    std::shared_ptr<Action> action = *mDeferredActions.begin();
    mDeferredActions.erase(mDeferredActions.begin());

    action->execute(this);
  }
}

void HpcPlayerInternal::performSeek(int64_t seekTimeUs, SeekMode mode) {
  ALOGV("performSeek seekTimeUs=%lld us (%.2f secs), mode=%d",
        (long long)seekTimeUs, seekTimeUs / 1E6, mode);

  if (mSource == nullptr) {
    // This happens when reset occurs right before the loop mode
    // asynchronously seeks to the start of the stream. 
    if (mAudioDecoder != nullptr || mVideoDecoder != nullptr) {
      ALOGE("mSource is nullptr and decoders not nullptr audio(%p) video(%p)",
            mAudioDecoder.get(), mVideoDecoder.get());
    }
    return;
  }
  mPreviousSeekTimeUs = seekTimeUs;
  mSource->seekTo(seekTimeUs, mode);
  ++mTimedTextGeneration;

  // everything's flushed, continue playback.
}

void HpcPlayerInternal::performDecoderFlush(FlushCommand audio, FlushCommand video) {
  ALOGV("performDecoderFlush audio=%d, video=%d", audio, video);

  if ((audio == FLUSH_CMD_NONE || mAudioDecoder == nullptr)
      && (video == FLUSH_CMD_NONE || mVideoDecoder == nullptr)) {
    return;
  }

  if (audio != FLUSH_CMD_NONE && mAudioDecoder != nullptr) {
    flushDecoder(true /* audio */, (audio == FLUSH_CMD_SHUTDOWN));
  }

  if (video != FLUSH_CMD_NONE && mVideoDecoder != nullptr) {
    flushDecoder(false /* audio */, (video == FLUSH_CMD_SHUTDOWN));
  }
}

void HpcPlayerInternal::performReset() {
  ALOGV("performReset");

  if(mVideoDecoder == nullptr || mAudioDecoder == nullptr) {
    return;
  }

  updatePlaybackTimer(true /* stopping */, "performReset");
  updateRebufferingTimer(true /* stopping */, true /* exiting */);

  cancelPollDuration();

  ++mScanSourcesGeneration;
  mScanSourcesPending = false;

  if (mRendererLooper != nullptr) {
    if (mRenderer != nullptr) {
      mRendererLooper->unregisterHandler(mRenderer->id());
    }
    mRendererLooper->stop();
    mRendererLooper.clear();
  }
  mRenderer.clear();
  ++mRendererGeneration;

  if (mSource != nullptr) {
    mSource->stop();

    std::lock_guard<std::mutex> autoLock(mSourceLock);
    mSource.clear();
  }

  if (!mPlayer.expired()) {
    std::shared_ptr<HpcPlayer> driver = mPlayer.lock();
    if (driver != nullptr) {
      driver->notifyResetComplete();
    }
  }

  mStarted = false;
  mPrepared = false;
  mResetting = false;
  mSourceStarted = false;

  // Modular DRM
  if (mCrypto != nullptr) {
    // decoders will be flushed before this so their mCrypto would go away on their own
    // TODO change to ALOGV
    ALOGD("performReset mCrypto: %p (%d)", mCrypto.get(),
          (mCrypto != nullptr ? mCrypto->getStrongCount() : 0));
    mCrypto.clear();
  }
  mIsDrmProtected = false;
}

void HpcPlayerInternal::performScanSources() {
  ALOGV("performScanSources");

  if (!mStarted) {
    return;
  }

  if (mAudioDecoder == nullptr || mVideoDecoder == nullptr) {
    postScanSources();
  }
}

void HpcPlayerInternal::performSetSurface(const std::shared_ptr<Surface> &surface) {
  ALOGV("performSetSurface");

  mSurface = surface;


  native_window_set_scaling_mode();
  // XXX - ignore error from setVideoScalingMode for now
  setVideoScalingMode(mVideoScalingMode);

  if (mDriver != nullptr) {
    std::shared_ptr<HpcPlayerDriver> driver = mDriver.promote();
    if (driver != nullptr) {
      driver->notifySetSurfaceComplete();
    }
  }
}

void HpcPlayerInternal::performResumeDecoders(bool needNotify) {
  if (needNotify) {
    mResumePending = true;
    if (mVideoDecoder == nullptr) {
      // if audio-only, we can notify seek complete now,
      // as the resume operation will be relatively fast.
      finishResume();
    }
  }

  if (mVideoDecoder != nullptr) {
    // When there is continuous seek, MediaPlayer will cache the seek
    // position, and send down new seek request when previous seek is
    // complete. Let's wait for at least one video output frame before
    // notifying seek complete, so that the video thumbnail gets updated
    // when seekbar is dragged.
    mVideoDecoder->signalResume(needNotify);
  }

  if (mAudioDecoder != nullptr) {
    mAudioDecoder->signalResume(false /* needNotify */);
  }
}

void HpcPlayerInternal::finishResume() {
  if (mResumePending) {
    mResumePending = false;
    notifyDriverSeekComplete();
  }
}

void HpcPlayerInternal::notifyDriverSeekComplete() {
  if (mDriver != nullptr) {
    std::shared_ptr<HpcPlayerDriver> driver = mDriver.promote();
    if (driver != nullptr) {
      driver->notifySeekComplete();
    }
  }
}

void HpcPlayerInternal::onMessageReceived(const std::shared_ptr<Message> &msg) {
  switch (msg->what()) {
    case kWhatSetDataSource:
    {
      ALOGV("kWhatSetDataSource");

      if (mSource == nullptr) {
        ALOGE("source is null");
        break;
      }

      status_t err = OK;
      void *obj;
      if(!msg->findObject("source",&obj)) {
        err = UNKNOWN_ERROR;
      } else {
        std::lock_guard autoLock(mSourceLock);
        mSource.reset(static_cast<hpc::Source *>(obj));
      }

      if(!mPlayer.expired()){
        mPlayer.lock()->notifySetDataSourceCompleted(err);
      }
      break;
    }

    case kWhatPrepare:
    {
      ALOGV("onMessageReceived kWhatPrepare");

      mSource->prepareAsync();
      break;
    }

    case kWhatPollDuration:
    {
      int32_t generation =msg->mArg1;
      if (generation != mPollDurationGeneration) {
        // stale
        break;
      }

      int64_t durationUs;
      if (!mPlayer.expired() && mSource->getDuration(&durationUs) == OK) {
        auto player = mPlayer.lock();
        if (player != nullptr) {
          player->notifyDuration(durationUs);
        }
      }

      msg->post(1000000LL);  // poll again in a second.
      break;
    }

    case kWhatSetVideoSurface:
    {

      void* obj;
      if(!msg->findObject("surface", &obj)) {
        break;
      }

      std::shared_ptr<Surface> surface((Surface*)obj);

      ALOGD("onSetVideoSurface(%p, %s video decoder)",
            surface.get(),
            (mSource != nullptr && mStarted && mSource->getFormat(false /* audio */) != nullptr
                && mVideoDecoder != nullptr) ? "have" : "no");

      // Need to check mStarted before calling mSource->getFormat because HpcPlayerInternal might
      // be in preparing state and it could take long time.
      // When mStarted is true, mSource must have been set.
      if (mSource == nullptr || !mStarted || mSource->getFormat(false /* audio */) == nullptr
          // NOTE: mVideoDecoder's mSurface is always non-null
          || (mVideoDecoder != nullptr && mVideoDecoder->setVideoSurface(surface) == OK)) {
        performSetSurface(surface);
        break;
      }

      mDeferredActions.push_back(
          std::make_shared<FlushDecoderAction>(
              (obj != nullptr ? FLUSH_CMD_FLUSH : FLUSH_CMD_NONE) /* audio */,
              FLUSH_CMD_SHUTDOWN /* video */));

      mDeferredActions.push_back(std::make_shared<SetSurfaceAction>(surface));

      if (obj != nullptr) {
        if (mStarted) {
          // Issue a seek to refresh the video screen only if started otherwise
          // the extractor may not yet be started and will assert.
          // If the video decoder is not set (perhaps audio only in this case)
          // do not perform a seek as it is not needed.
          int64_t currentPositionUs = 0;
          if (getCurrentPosition(&currentPositionUs) == OK) {
            mDeferredActions.push_back(
                std::make_shared<SeekAction>(currentPositionUs,
                               SeekMode::SEEK_PREVIOUS_SYNC /* mode */));
          }
        }

        // If there is a new surface texture, instantiate decoders
        // again if possible.
        mDeferredActions.push_back(
            std::make_shared<SimpleAction>(&HpcPlayerInternal::performScanSources));

        // After a flush without shutdown, decoder is paused.
        // Don't resume it until source seek is done, otherwise it could
        // start pulling stale data too soon.
        mDeferredActions.push_back(
            std::make_shared<ResumeDecoderAction>(false /* needNotify */));
      }

      processDeferredActions();
      break;
    }

    case kWhatStart:
    {
      ALOGV("kWhatStart");
      if (mStarted) {
        // do not resume yet if the source is still buffering
        if (!mPausedForBuffering) {
          onResume();
        }
      } else {
        onStart();
      }
      mPausedByClient = false;
      break;
    }

    case kWhatConfigPlayback:
    {
      std::shared_ptr<AReplyToken> replyID;
      if(msg->senderAwaitsResponse(&replyID)) {
        ALOGE("kWhatConfigPlayback: senderAwaitsResponse is false");
        break;
      }
      float rate /* sanitized */;
       (msg, &rate);
      status_t err = OK;
      if (mRenderer != nullptr) {
        err = mRenderer->setPlaybackSettings(rate);
      }
      if (err == OK) {
        if (rate.mSpeed == 0.f) {
          onPause();
          mPausedByClient = true;
          // save all other settings (using non-paused speed)
          // so we can restore them on start
          AudioPlaybackRate newRate = rate;
          newRate.mSpeed = mPlaybackRate.mSpeed;
          mPlaybackRate = newRate;
        } else { /* rate.mSpeed != 0.f */
          mPlaybackRate = rate;
          if (mStarted) {
            // do not resume yet if the source is still buffering
            if (!mPausedForBuffering) {
              onResume();
            }
          } else if (mPrepared) {
            onStart();
          }

          mPausedByClient = false;
        }
      }

      if (mVideoDecoder != nullptr) {
        std::shared_ptr<Message> params = new Message();
        params->setFloat("playback-speed", mPlaybackRate.mSpeed);
        mVideoDecoder->setParameters(params);
      }

      std::shared_ptr<Message> response = new Message;
      response->setInt32("err", err);
      response->postReply(replyID);
      break;
    }

    case kWhatGetPlaybackSettings:
    {
      std::shared_ptr<AReplyToken> replyID;
      CHECK(msg->senderAwaitsResponse(&replyID));
      float rate = mPlaybackRate;
      status_t err = OK;
      if (mRenderer != nullptr) {
        err = mRenderer->getPlaybackSettings(&rate);
      }
      if (err == OK) {
        // get playback settings used by renderer, as it may be
        // slightly off due to audiosink not taking small changes.
        mPlaybackRate = rate;
        if (mPaused) {
          rate.mSpeed = 0.f;
        }
      }
      std::shared_ptr<Message> response = new Message;
      if (err == OK) {
        writeToMessage(response, rate);
      }
      response->setInt32("err", err);
      response->postReply(replyID);
      break;
    }

    case kWhatConfigSync:
    {
      std::shared_ptr<AReplyToken> replyID;
      CHECK(msg->senderAwaitsResponse(&replyID));

      ALOGV("kWhatConfigSync");
      AVSyncSettings sync;
      float videoFpsHint;
      readFromMessage(msg, &sync, &videoFpsHint);
      status_t err = OK;
      if (mRenderer != nullptr) {
        err = mRenderer->setSyncSettings(sync, videoFpsHint);
      }
      if (err == OK) {
        mSyncSettings = sync;
        mVideoFpsHint = videoFpsHint;
      }
      std::shared_ptr<Message> response = new Message;
      response->setInt32("err", err);
      response->postReply(replyID);
      break;
    }

    case kWhatGetSyncSettings:
    {
      std::shared_ptr<AReplyToken> replyID;
      CHECK(msg->senderAwaitsResponse(&replyID));
      AVSyncSettings sync = mSyncSettings;
      float videoFps = mVideoFpsHint;
      status_t err = OK;
      if (mRenderer != nullptr) {
        err = mRenderer->getSyncSettings(&sync, &videoFps);
        if (err == OK) {
          mSyncSettings = sync;
          mVideoFpsHint = videoFps;
        }
      }
      std::shared_ptr<Message> response = new Message;
      if (err == OK) {
        writeToMessage(response, sync, videoFps);
      }
      response->setInt32("err", err);
      response->postReply(replyID);
      break;
    }

    case kWhatScanSources:
    {
      int32_t generation;
      CHECK(msg->findInt32("generation", &generation));
      if (generation != mScanSourcesGeneration) {
        // Drop obsolete msg.
        break;
      }

      mScanSourcesPending = false;

      ALOGV("scanning sources haveAudio=%d, haveVideo=%d",
            mAudioDecoder != nullptr, mVideoDecoder != nullptr);

      bool mHadAnySourcesBefore =
          (mAudioDecoder != nullptr) || (mVideoDecoder != nullptr);
      bool rescan = false;

      // initialize video before audio because successful initialization of
      // video may change deep buffer mode of audio.
      if (mSurface != nullptr) {
        if (instantiateDecoder(false, &mVideoDecoder) == -EWOULDBLOCK) {
          rescan = true;
        }
      }

      // Don't try to re-open audio sink if there's an existing decoder.
      if (mAudioSink != nullptr && mAudioDecoder == nullptr) {
        if (instantiateDecoder(true, &mAudioDecoder) == -EWOULDBLOCK) {
          rescan = true;
        }
      }

      if (!mHadAnySourcesBefore
          && (mAudioDecoder != nullptr || mVideoDecoder != nullptr)) {
        // This is the first time we've found anything playable.

        if (mSourceFlags & Source::FLAG_DYNAMIC_DURATION) {
          schedulePollDuration();
        }
      }

      status_t err;
      if ((err = mSource->feedMoreTSData()) != OK) {
        if (mAudioDecoder == nullptr && mVideoDecoder == nullptr) {
          // We're not currently decoding anything (no audio or
          // video tracks found) and we just ran out of input data.

          if (err == ERROR_END_OF_STREAM) {
            notifyListener(MEDIA_PLAYBACK_COMPLETE, 0, 0);
          } else {
            notifyListener(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, err);
          }
        }
        break;
      }

      if (rescan) {
        msg->post(100000LL);
        mScanSourcesPending = true;
      }
      break;
    }

    case kWhatVideoNotify:
    case kWhatAudioNotify:
    {
      bool audio = msg->what() == kWhatAudioNotify;

      int32_t currentDecoderGeneration =
          (audio? mAudioDecoderGeneration : mVideoDecoderGeneration);
      int32_t requesterGeneration = currentDecoderGeneration - 1;
      CHECK(msg->findInt32("generation", &requesterGeneration));

      if (requesterGeneration != currentDecoderGeneration) {
        ALOGV("got message from old %s decoder, generation(%d:%d)",
              audio ? "audio" : "video", requesterGeneration,
              currentDecoderGeneration);
        std::shared_ptr<Message> reply;
        if (!(msg->findMessage("reply", &reply))) {
          return;
        }

        reply->setInt32("err", INFO_DISCONTINUITY);
        reply->post();
        return;
      }

      int32_t what;
      CHECK(msg->findInt32("what", &what));

      if (what == DecoderBase::kWhatInputDiscontinuity) {
        int32_t formatChange;
        CHECK(msg->findInt32("formatChange", &formatChange));

        ALOGV("%s discontinuity: formatChange %d",
              audio ? "audio" : "video", formatChange);

        if (formatChange) {
          mDeferredActions.push_back(
              new FlushDecoderAction(
                  audio ? FLUSH_CMD_SHUTDOWN : FLUSH_CMD_NONE,
                  audio ? FLUSH_CMD_NONE : FLUSH_CMD_SHUTDOWN));
        }

        mDeferredActions.push_back(
            new SimpleAction(
                &HpcPlayerInternal::performScanSources));

        processDeferredActions();
      } else if (what == DecoderBase::kWhatEOS) {
        int32_t err;
        CHECK(msg->findInt32("err", &err));

        if (err == ERROR_END_OF_STREAM) {
          ALOGV("got %s decoder EOS", audio ? "audio" : "video");
        } else {
          ALOGV("got %s decoder EOS w/ error %d",
                audio ? "audio" : "video",
                err);
        }

        mRenderer->queueEOS(audio, err);
      } else if (what == DecoderBase::kWhatFlushCompleted) {
        ALOGV("decoder %s flush completed", audio ? "audio" : "video");

        handleFlushComplete(audio, true /* isDecoder */);
        finishFlushIfPossible();
      } else if (what == DecoderBase::kWhatVideoSizeChanged) {
        std::shared_ptr<Message> format;
        CHECK(msg->findMessage("format", &format));

        std::shared_ptr<Message> inputFormat =
            mSource->getFormat(false /* audio */);

        setVideoScalingMode(mVideoScalingMode);
        updateVideoSize(inputFormat, format);
      } else if (what == DecoderBase::kWhatShutdownCompleted) {
        ALOGV("%s shutdown completed", audio ? "audio" : "video");
        if (audio) {
          std::lock_guard<std::mutex> autoLock(mDecoderLock);
          mAudioDecoder.clear();
          mAudioDecoderError = false;
          ++mAudioDecoderGeneration;

          CHECK_EQ((int)mFlushingAudio, (int)SHUTTING_DOWN_DECODER);
          mFlushingAudio = SHUT_DOWN;
        } else {
          std::lock_guard<std::mutex> autoLock(mDecoderLock);
          mVideoDecoder.clear();
          mVideoDecoderError = false;
          ++mVideoDecoderGeneration;

          CHECK_EQ((int)mFlushingVideo, (int)SHUTTING_DOWN_DECODER);
          mFlushingVideo = SHUT_DOWN;
        }

        finishFlushIfPossible();
      } else if (what == DecoderBase::kWhatResumeCompleted) {
        finishResume();
      } else if (what == DecoderBase::kWhatError) {
        status_t err;
        if (!msg->findInt32("err", &err) || err == OK) {
          err = UNKNOWN_ERROR;
        }

        // Decoder errors can be due to Source (e.g. from streaming),
        // or from decoding corrupted bitstreams, or from other decoder
        // MediaCodec operations (e.g. from an ongoing reset or seek).
        // They may also be due to openAudioSink failure at
        // decoder start or after a format change.
        //
        // We try to gracefully shut down the affected decoder if possible,
        // rather than trying to force the shutdown with something
        // similar to performReset(). This method can lead to a hang
        // if MediaCodec functions block after an error, but they should
        // typically return INVALID_OPERATION instead of blocking.

        FlushStatus *flushing = audio ? &mFlushingAudio : &mFlushingVideo;
        ALOGE("received error(%#x) from %s decoder, flushing(%d), now shutting down",
              err, audio ? "audio" : "video", *flushing);

        switch (*flushing) {
          case NONE:
            mDeferredActions.push_back(
                new FlushDecoderAction(
                    audio ? FLUSH_CMD_SHUTDOWN : FLUSH_CMD_NONE,
                    audio ? FLUSH_CMD_NONE : FLUSH_CMD_SHUTDOWN));
            processDeferredActions();
            break;
          case FLUSHING_DECODER:
            *flushing = FLUSHING_DECODER_SHUTDOWN; // initiate shutdown after flush.
            break; // Wait for flush to complete.
          case FLUSHING_DECODER_SHUTDOWN:
            break; // Wait for flush to complete.
          case SHUTTING_DOWN_DECODER:
            break; // Wait for shutdown to complete.
          case FLUSHED:
            getDecoder(audio)->initiateShutdown(); // In the middle of a seek.
            *flushing = SHUTTING_DOWN_DECODER;     // Shut down.
            break;
          case SHUT_DOWN:
            finishFlushIfPossible();  // Should not occur.
            break;                    // Finish anyways.
        }
        if (mSource != nullptr) {
          if (audio) {
            if (mVideoDecoderError || mSource->getFormat(false /* audio */) == nullptr
                || mSurface == nullptr || mVideoDecoder == nullptr) {
              // When both audio and video have error, or this stream has only audio
              // which has error, notify client of error.
              notifyListener(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, err);
            } else {
              // Only audio track has error. Video track could be still good to play.
              if (mVideoEOS) {
                notifyListener(MEDIA_PLAYBACK_COMPLETE, 0, 0);
              } else {
                notifyListener(MEDIA_INFO, MEDIA_INFO_PLAY_AUDIO_ERROR, err);
              }
            }
            mAudioDecoderError = true;
          } else {
            if (mAudioDecoderError || mSource->getFormat(true /* audio */) == nullptr
                || mAudioSink == nullptr || mAudioDecoder == nullptr) {
              // When both audio and video have error, or this stream has only video
              // which has error, notify client of error.
              notifyListener(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, err);
            } else {
              // Only video track has error. Audio track could be still good to play.
              if (mAudioEOS) {
                notifyListener(MEDIA_PLAYBACK_COMPLETE, 0, 0);
              } else {
                notifyListener(MEDIA_INFO, MEDIA_INFO_PLAY_VIDEO_ERROR, err);
              }
            }
            mVideoDecoderError = true;
          }
        }
      } else {
        ALOGV("Unhandled decoder notification %d '%c%c%c%c'.",
              what,
              what >> 24,
              (what >> 16) & 0xff,
              (what >> 8) & 0xff,
              what & 0xff);
      }

      break;
    }

    case kWhatRendererNotify:
    {
      int32_t requesterGeneration = mRendererGeneration - 1;
      CHECK(msg->findInt32("generation", &requesterGeneration));
      if (requesterGeneration != mRendererGeneration) {
        ALOGV("got message from old renderer, generation(%d:%d)",
              requesterGeneration, mRendererGeneration);
        return;
      }

      int32_t what;
      CHECK(msg->findInt32("what", &what));

      if (what == Renderer::kWhatEOS) {
        int32_t audio;
        CHECK(msg->findInt32("audio", &audio));

        int32_t finalResult;
        CHECK(msg->findInt32("finalResult", &finalResult));

        if (audio) {
          mAudioEOS = true;
        } else {
          mVideoEOS = true;
        }

        if (finalResult == ERROR_END_OF_STREAM) {
          ALOGV("reached %s EOS", audio ? "audio" : "video");
        } else {
          ALOGE("%s track encountered an error (%d)",
                audio ? "audio" : "video", finalResult);

          notifyListener(
              MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, finalResult);
        }

        if ((mAudioEOS || mAudioDecoder == nullptr)
            && (mVideoEOS || mVideoDecoder == nullptr)) {
          notifyListener(MEDIA_PLAYBACK_COMPLETE, 0, 0);
        }
      } else if (what == Renderer::kWhatFlushComplete) {
        int32_t audio;
        CHECK(msg->findInt32("audio", &audio));

        if (audio) {
          mAudioEOS = false;
        } else {
          mVideoEOS = false;
        }

        ALOGV("renderer %s flush completed.", audio ? "audio" : "video");
        if (audio && (mFlushingAudio == NONE || mFlushingAudio == FLUSHED
            || mFlushingAudio == SHUT_DOWN)) {
          // Flush has been handled by tear down.
          break;
        }
        handleFlushComplete(audio, false /* isDecoder */);
        finishFlushIfPossible();
      } else if (what == Renderer::kWhatVideoRenderingStart) {
        notifyListener(MEDIA_INFO, MEDIA_INFO_RENDERING_START, 0);
      } else if (what == Renderer::kWhatMediaRenderingStart) {
        ALOGV("media rendering started");
        notifyListener(MEDIA_STARTED, 0, 0);
      } else if (what == Renderer::kWhatAudioTearDown) {
        int32_t reason;
        CHECK(msg->findInt32("reason", &reason));
        ALOGV("Tear down audio with reason %d.", reason);
        if (reason == Renderer::kDueToTimeout && !(mPaused && mOffloadAudio)) {
          // TimeoutWhenPaused is only for offload mode.
          ALOGW("Received a stale message for teardown, mPaused(%d), mOffloadAudio(%d)",
                mPaused, mOffloadAudio);
          break;
        }
        int64_t positionUs;
        if (!msg->findInt64("positionUs", &positionUs)) {
          positionUs = mPreviousSeekTimeUs;
        }

        restartAudio(
            positionUs, reason == Renderer::kForceNonOffload /* forceNonOffload */,
            reason != Renderer::kDueToTimeout /* needsToCreateAudioDecoder */);
      }
      break;
    }

    case kWhatMoreDataQueued:
    {
      break;
    }

    case kWhatReset:
    {
      ALOGV("kWhatReset");

      mResetting = true;
      updatePlaybackTimer(true /* stopping */, "kWhatReset");
      updateRebufferingTimer(true /* stopping */, true /* exiting */);

      mDeferredActions.push_back(
          new FlushDecoderAction(
              FLUSH_CMD_SHUTDOWN /* audio */,
              FLUSH_CMD_SHUTDOWN /* video */));

      mDeferredActions.push_back(
          new SimpleAction(&HpcPlayerInternal::performReset));

      processDeferredActions();
      break;
    }

    case kWhatNotifyTime:
    {
      ALOGV("kWhatNotifyTime");
      int64_t timerUs;
      CHECK(msg->findInt64("timerUs", &timerUs));

      notifyListener(MEDIA_NOTIFY_TIME, timerUs, 0);
      break;
    }

    case kWhatSeek:
    {
      int64_t seekTimeUs;
      int32_t mode;
      int32_t needNotify;
      CHECK(msg->findInt64("seekTimeUs", &seekTimeUs));
      CHECK(msg->findInt32("mode", &mode));
      CHECK(msg->findInt32("needNotify", &needNotify));

      ALOGV("kWhatSeek seekTimeUs=%lld us, mode=%d, needNotify=%d",
            (long long)seekTimeUs, mode, needNotify);

      if (!mStarted) {
        // Seek before the player is started. In order to preview video,
        // need to start the player and pause it. This branch is called
        // only once if needed. After the player is started, any seek
        // operation will go through normal path.
        // Audio-only cases are handled separately.
        onStart(seekTimeUs, (MediaPlayerSeekMode)mode);
        if (mStarted) {
          onPause();
          mPausedByClient = true;
        }
        if (needNotify) {
          notifyDriverSeekComplete();
        }
        break;
      }

      mDeferredActions.push_back(
          new FlushDecoderAction(FLUSH_CMD_FLUSH /* audio */,
                                 FLUSH_CMD_FLUSH /* video */));

      mDeferredActions.push_back(
          new SeekAction(seekTimeUs, (MediaPlayerSeekMode)mode));

      // After a flush without shutdown, decoder is paused.
      // Don't resume it until source seek is done, otherwise it could
      // start pulling stale data too soon.
      mDeferredActions.push_back(
          new ResumeDecoderAction(needNotify));

      processDeferredActions();
      break;
    }

    case kWhatPause:
    {
      onPause();
      mPausedByClient = true;
      break;
    }

    case kWhatSourceNotify:
    {
      onSourceNotify(msg);
      break;
    }

    case kWhatClosedCaptionNotify:
    {
      onClosedCaptionNotify(msg);
      break;
    }

    case kWhatMediaClockNotify:
    {
      ALOGV("kWhatMediaClockNotify");
      int64_t anchorMediaUs, anchorRealUs;
      float playbackRate;
      CHECK(msg->findInt64("anchor-media-us", &anchorMediaUs));
      CHECK(msg->findInt64("anchor-real-us", &anchorRealUs));
      CHECK(msg->findFloat("playback-rate", &playbackRate));

      Parcel in;
      in.writeInt64(anchorMediaUs);
      in.writeInt64(anchorRealUs);
      in.writeFloat(playbackRate);

      notifyListener(MEDIA_TIME_DISCONTINUITY, 0, 0, &in);
      break;
    }
    default:
      break;
  }
}

status_t HpcPlayerInternal::prepareAsync() {
  return 0;
}

void HpcPlayerInternal::start() {

}
void HpcPlayerInternal::pause() {

}
void HpcPlayerInternal::init(const std::weak_ptr<HpcPlayer> &driver) {

}
void HpcPlayerInternal::updateVideoSize(const std::shared_ptr<Message> &inputFormat,
                                        const std::shared_ptr<Message> &outputFormat) {

}
void HpcPlayerInternal::onStart(int64_t startPositionUs, SeekMode mode) {

}

void HpcPlayerInternal::startPlaybackTimer(const char *where) {
  std::lock_guard<std::mutex> autoLock(mPlayingTimeLock);
  if (mLastStartedPlayingTimeNs == 0) {
    mLastStartedPlayingTimeNs = systemTime();
    ALOGV("startPlaybackTimer() time %20" PRId64 " (%s)",  mLastStartedPlayingTimeNs, where);
  }
}

void HpcPlayerInternal::updatePlaybackTimer(bool stopping, const char *where) {
  std::lock_guard<std::mutex> autoLock(mPlayingTimeLock);

  ALOGV("updatePlaybackTimer(%s)  time %20" PRId64 " (%s)",
        stopping ? "stop" : "snap", mLastStartedPlayingTimeNs, where);

  if (mLastStartedPlayingTimeNs != 0) {
    sp<NuPlayerDriver> driver = mDriver.promote();
    int64_t now = systemTime();
    if (driver != NULL) {
      int64_t played = now - mLastStartedPlayingTimeNs;
      ALOGV("updatePlaybackTimer()  log  %20" PRId64 "", played);

      if (played > 0) {
        driver->notifyMorePlayingTimeUs((played+500)/1000);
      }
    }
    if (stopping) {
      mLastStartedPlayingTimeNs = 0;
    } else {
      mLastStartedPlayingTimeNs = now;
    }
  }
}

void HpcPlayerInternal::startRebufferingTimer() {
  std::lock_guard<std::mutex> autoLock(mPlayingTimeLock);
  if (mLastStartedRebufferingTimeNs == 0) {
    mLastStartedRebufferingTimeNs = systemTime();
    ALOGV("startRebufferingTimer() time %20" PRId64 "",  mLastStartedRebufferingTimeNs);
  }
}

void HpcPlayerInternal::updateRebufferingTimer(bool stopping, bool exitingPlayback) {
  std::lock_guard<std::mutex> autoLock(mPlayingTimeLock);

  ALOGV("updateRebufferingTimer(%s)  time %20" PRId64 " (exiting %d)",
        stopping ? "stop" : "snap", mLastStartedRebufferingTimeNs, exitingPlayback);

  if (mLastStartedRebufferingTimeNs != 0) {
    sp<HpcPlayer> driver = mDriver.promote();
    int64_t now = systemTime();
    if (driver != NULL) {
      int64_t rebuffered = now - mLastStartedRebufferingTimeNs;
      ALOGV("updateRebufferingTimer()  log  %20" PRId64 "", rebuffered);

      if (rebuffered > 0) {
        driver->notifyMoreRebufferingTimeUs((rebuffered+500)/1000);
        if (exitingPlayback) {
          driver->notifyRebufferingWhenExit(true);
        }
      }
    }
    if (stopping) {
      mLastStartedRebufferingTimeNs = 0;
    } else {
      mLastStartedRebufferingTimeNs = now;
    }
  }
}

void HpcPlayerInternal::updateInternalTimers() {
  // update values, but ticking clocks keep ticking
  ALOGV("updateInternalTimers()");
  updatePlaybackTimer(false /* stopping */, "updateInternalTimers");
  updateRebufferingTimer(false /* stopping */, false /* exiting */);
}

void HpcPlayerInternal::seekTo(int64_t seekTimeUs) {

}
status_t HpcPlayerInternal::setVideoScalingMode(int32_t mode) {
  return 0;
}
void HpcPlayerInternal::resetAsync() {

}

status_t HpcPlayerInternal::notifyAt(int64_t mediaTimeUs) {
  std::shared_ptr<Message> notify = std::make_shared<Message>(kWhatNotifyTime, shared_from_this());
  notify->setInt(mediaTimeUs);
  mMediaClock->addTimer(notify, mediaTimeUs);
  return OK;
}

void HpcPlayerInternal::postScanSources() {
  if (mScanSourcesPending) {
    return;
  }

  std::shared_ptr<Message> msg = std::make_shared<Message>(kWhatScanSources, shared_from_this());
  msg->setInt(mScanSourcesGeneration);
  msg->post();

  mScanSourcesPending = true;
}

}