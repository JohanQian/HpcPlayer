#include "HpcPlayerInternal.h"
#include "foundation/Message.h"
#include "foundation/Log.h"
#include "foundation/MetaData.h"
#include "source/Source.h"
#include "HpcPlayer.h"
#include <android/native_window.h>

#define LOG_TAG "HpcPlayerInternal"

namespace hpc {
HpcPlayerInternal::HpcPlayerInternal(pid_t pid, const std::shared_ptr<MediaClock> &mediaClock) {

}

HpcPlayerInternal::~HpcPlayerInternal() {

}

void HpcPlayerInternal::setDataSourceAsync(const char *url) {

}


status_t HpcPlayerInternal::setVideoSurface(ANativeWindow *window) {
  return 0;
}

void HpcPlayerInternal::onMessageReceived(const std::shared_ptr<Message> &msg) {
  switch (msg->what()) {
    case kWhatSetDataSource:
    {
      ALOGV("kWhatSetDataSource");

      if (mSource == NULL) {
        ALOGE("source is null");
        break;
      }

      status_t err = OK;
      void *obj;
      if(!(msg->findObject(&obj))) {
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

      sp<RefBase> obj;
      CHECK(msg->findObject("surface", &obj));
      sp<Surface> surface = static_cast<Surface *>(obj.get());

      ALOGD("onSetVideoSurface(%p, %s video decoder)",
            surface.get(),
            (mSource != NULL && mStarted && mSource->getFormat(false /* audio */) != NULL
                && mVideoDecoder != NULL) ? "have" : "no");

      // Need to check mStarted before calling mSource->getFormat because NuPlayer might
      // be in preparing state and it could take long time.
      // When mStarted is true, mSource must have been set.
      if (mSource == NULL || !mStarted || mSource->getFormat(false /* audio */) == NULL
          // NOTE: mVideoDecoder's mSurface is always non-null
          || (mVideoDecoder != NULL && mVideoDecoder->setVideoSurface(surface) == OK)) {
        performSetSurface(surface);
        break;
      }

      mDeferredActions.push_back(
          new FlushDecoderAction(
              (obj != NULL ? FLUSH_CMD_FLUSH : FLUSH_CMD_NONE) /* audio */,
              FLUSH_CMD_SHUTDOWN /* video */));

      mDeferredActions.push_back(new SetSurfaceAction(surface));

      if (obj != NULL) {
        if (mStarted) {
          // Issue a seek to refresh the video screen only if started otherwise
          // the extractor may not yet be started and will assert.
          // If the video decoder is not set (perhaps audio only in this case)
          // do not perform a seek as it is not needed.
          int64_t currentPositionUs = 0;
          if (getCurrentPosition(&currentPositionUs) == OK) {
            mDeferredActions.push_back(
                new SeekAction(currentPositionUs,
                               MediaPlayerSeekMode::SEEK_PREVIOUS_SYNC /* mode */));
          }
        }

        // If there is a new surface texture, instantiate decoders
        // again if possible.
        mDeferredActions.push_back(
            new SimpleAction(&NuPlayer::performScanSources));

        // After a flush without shutdown, decoder is paused.
        // Don't resume it until source seek is done, otherwise it could
        // start pulling stale data too soon.
        mDeferredActions.push_back(
            new ResumeDecoderAction(false /* needNotify */));
      }

      processDeferredActions();
      break;
    }

    case kWhatSetAudioSink:
    {
      ALOGV("kWhatSetAudioSink");

      sp<RefBase> obj;
      CHECK(msg->findObject("sink", &obj));

      mAudioSink = static_cast<MediaPlayerBase::AudioSink *>(obj.get());
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
      sp<AReplyToken> replyID;
      CHECK(msg->senderAwaitsResponse(&replyID));
      AudioPlaybackRate rate /* sanitized */;
      readFromAMessage(msg, &rate);
      status_t err = OK;
      if (mRenderer != NULL) {
        // AudioSink allows only 1.f and 0.f for offload and direct modes.
        // For other speeds, restart audio to fallback to supported paths
        bool audioDirectOutput = (mAudioSink->getFlags() & AUDIO_OUTPUT_FLAG_DIRECT) != 0;
        if ((mOffloadAudio || audioDirectOutput) &&
            ((rate.mSpeed != 0.f && rate.mSpeed != 1.f) || rate.mPitch != 1.f)) {

          int64_t currentPositionUs;
          if (getCurrentPosition(&currentPositionUs) != OK) {
            currentPositionUs = mPreviousSeekTimeUs;
          }

          // Set mPlaybackSettings so that the new audio decoder can
          // be created correctly.
          mPlaybackSettings = rate;
          if (!mPaused) {
            mRenderer->pause();
          }
          restartAudio(
              currentPositionUs, true /* forceNonOffload */,
              true /* needsToCreateAudioDecoder */);
          if (!mPaused) {
            mRenderer->resume();
          }
        }

        err = mRenderer->setPlaybackSettings(rate);
      }
      if (err == OK) {
        if (rate.mSpeed == 0.f) {
          onPause();
          mPausedByClient = true;
          // save all other settings (using non-paused speed)
          // so we can restore them on start
          AudioPlaybackRate newRate = rate;
          newRate.mSpeed = mPlaybackSettings.mSpeed;
          mPlaybackSettings = newRate;
        } else { /* rate.mSpeed != 0.f */
          mPlaybackSettings = rate;
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

      if (mVideoDecoder != NULL) {
        sp<AMessage> params = new AMessage();
        params->setFloat("playback-speed", mPlaybackSettings.mSpeed);
        mVideoDecoder->setParameters(params);
      }

      sp<AMessage> response = new AMessage;
      response->setInt32("err", err);
      response->postReply(replyID);
      break;
    }

    case kWhatGetPlaybackSettings:
    {
      sp<AReplyToken> replyID;
      CHECK(msg->senderAwaitsResponse(&replyID));
      AudioPlaybackRate rate = mPlaybackSettings;
      status_t err = OK;
      if (mRenderer != NULL) {
        err = mRenderer->getPlaybackSettings(&rate);
      }
      if (err == OK) {
        // get playback settings used by renderer, as it may be
        // slightly off due to audiosink not taking small changes.
        mPlaybackSettings = rate;
        if (mPaused) {
          rate.mSpeed = 0.f;
        }
      }
      sp<AMessage> response = new AMessage;
      if (err == OK) {
        writeToAMessage(response, rate);
      }
      response->setInt32("err", err);
      response->postReply(replyID);
      break;
    }

    case kWhatConfigSync:
    {
      sp<AReplyToken> replyID;
      CHECK(msg->senderAwaitsResponse(&replyID));

      ALOGV("kWhatConfigSync");
      AVSyncSettings sync;
      float videoFpsHint;
      readFromAMessage(msg, &sync, &videoFpsHint);
      status_t err = OK;
      if (mRenderer != NULL) {
        err = mRenderer->setSyncSettings(sync, videoFpsHint);
      }
      if (err == OK) {
        mSyncSettings = sync;
        mVideoFpsHint = videoFpsHint;
      }
      sp<AMessage> response = new AMessage;
      response->setInt32("err", err);
      response->postReply(replyID);
      break;
    }

    case kWhatGetSyncSettings:
    {
      sp<AReplyToken> replyID;
      CHECK(msg->senderAwaitsResponse(&replyID));
      AVSyncSettings sync = mSyncSettings;
      float videoFps = mVideoFpsHint;
      status_t err = OK;
      if (mRenderer != NULL) {
        err = mRenderer->getSyncSettings(&sync, &videoFps);
        if (err == OK) {
          mSyncSettings = sync;
          mVideoFpsHint = videoFps;
        }
      }
      sp<AMessage> response = new AMessage;
      if (err == OK) {
        writeToAMessage(response, sync, videoFps);
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
            mAudioDecoder != NULL, mVideoDecoder != NULL);

      bool mHadAnySourcesBefore =
          (mAudioDecoder != NULL) || (mVideoDecoder != NULL);
      bool rescan = false;

      // initialize video before audio because successful initialization of
      // video may change deep buffer mode of audio.
      if (mSurface != NULL) {
        if (instantiateDecoder(false, &mVideoDecoder) == -EWOULDBLOCK) {
          rescan = true;
        }
      }

      // Don't try to re-open audio sink if there's an existing decoder.
      if (mAudioSink != NULL && mAudioDecoder == NULL) {
        if (instantiateDecoder(true, &mAudioDecoder) == -EWOULDBLOCK) {
          rescan = true;
        }
      }

      if (!mHadAnySourcesBefore
          && (mAudioDecoder != NULL || mVideoDecoder != NULL)) {
        // This is the first time we've found anything playable.

        if (mSourceFlags & Source::FLAG_DYNAMIC_DURATION) {
          schedulePollDuration();
        }
      }

      status_t err;
      if ((err = mSource->feedMoreTSData()) != OK) {
        if (mAudioDecoder == NULL && mVideoDecoder == NULL) {
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
        sp<AMessage> reply;
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
                &NuPlayer::performScanSources));

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
        sp<AMessage> format;
        CHECK(msg->findMessage("format", &format));

        sp<AMessage> inputFormat =
            mSource->getFormat(false /* audio */);

        setVideoScalingMode(mVideoScalingMode);
        updateVideoSize(inputFormat, format);
      } else if (what == DecoderBase::kWhatShutdownCompleted) {
        ALOGV("%s shutdown completed", audio ? "audio" : "video");
        if (audio) {
          Mutex::Autolock autoLock(mDecoderLock);
          mAudioDecoder.clear();
          mAudioDecoderError = false;
          ++mAudioDecoderGeneration;

          CHECK_EQ((int)mFlushingAudio, (int)SHUTTING_DOWN_DECODER);
          mFlushingAudio = SHUT_DOWN;
        } else {
          Mutex::Autolock autoLock(mDecoderLock);
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
            if (mVideoDecoderError || mSource->getFormat(false /* audio */) == NULL
                || mSurface == NULL || mVideoDecoder == NULL) {
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
            if (mAudioDecoderError || mSource->getFormat(true /* audio */) == NULL
                || mAudioSink == NULL || mAudioDecoder == NULL) {
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

        if ((mAudioEOS || mAudioDecoder == NULL)
            && (mVideoEOS || mVideoDecoder == NULL)) {
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
          new SimpleAction(&NuPlayer::performReset));

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

    case kWhatPrepareDrm:
    {
      status_t status = onPrepareDrm(msg);

      sp<AMessage> response = new AMessage;
      response->setInt32("status", status);
      sp<AReplyToken> replyID;
      CHECK(msg->senderAwaitsResponse(&replyID));
      response->postReply(replyID);
      break;
    }

    case kWhatReleaseDrm:
    {
      status_t status = onReleaseDrm();

      sp<AMessage> response = new AMessage;
      response->setInt32("status", status);
      sp<AReplyToken> replyID;
      CHECK(msg->senderAwaitsResponse(&replyID));
      response->postReply(replyID);
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

}