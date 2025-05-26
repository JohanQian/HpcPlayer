#include "DefaultSource.h"
#include "Log.h"
#include "Error.h"
#include "MetaData.h"
#include "Message.h"
#include "Looper.h"


#define LOG_TAG "DefaultSource"


namespace hpc {
DefaultSource::DefaultSource(const std::shared_ptr<Message> &notify,
                             bool uidValid,
                             uid_t uid,
                             const std::shared_ptr<MediaClock> &mediaClock)
    : Source(notify),
      mMediaClock(mediaClock)
{

}

DefaultSource::~DefaultSource() {

}

void DefaultSource::setDataSource(const char *url) {
  mUri = url;
}

void DefaultSource::prepareAsync() {
  std::lock_guard _l(mLock);
  ALOGV("prepareAsync: (looper: %d)", (mLooper != NULL));

  if (mLooper == NULL) {
    mLooper = std::make_shared<Looper>;
    mLooper->setName("generic");
    mLooper->start();

    //mLooper->registerHandler(enable_shared_from_this());
  }

  std::shared_ptr<Message> msg = std::make_shared<Message>(kWhatPrepareAsync);
  msg->post();
}

void DefaultSource::start() {
  std::lock_guard _l(mLock);
  ALOGI("start");

  if (mAudioTrack.mExtractor != NULL) {
    postReadBuffer(MEDIA_TRACK_TYPE_AUDIO);
  }

  if (mVideoTrack.mExtractor != NULL) {
    postReadBuffer(MEDIA_TRACK_TYPE_VIDEO);
  }

  mStarted = true;
}

void DefaultSource::postReadBuffer(media_track_type trackType) {
  if ((mPendingReadBufferTypes & (1 << trackType)) == 0) {
    mPendingReadBufferTypes |= (1 << trackType);
    std::shared_ptr<Message> msg = std::make_shared<Message>(kWhatReadBuffer, this);
    msg->setInt32("trackType", trackType);
    msg->post();
  }
}

void DefaultSource::onReadBuffer(const std::shared_ptr<Message>& msg) {
  int32_t tmpType;
  CHECK(msg->findInt32("trackType", &tmpType));
  media_track_type trackType = (media_track_type)tmpType;
  mPendingReadBufferTypes &= ~(1 << trackType);
  readBuffer(trackType);
}

void DefaultSource::readBuffer(
    media_track_type trackType, int64_t seekTimeUs, MediaPlayerSeekMode mode,
    int64_t *actualTimeUs, bool formatChange) {
  Track *track;
  size_t maxBuffers = 1;
  switch (trackType) {
    case MEDIA_TRACK_TYPE_VIDEO:
      track = &mVideoTrack;
      maxBuffers = 8;  // too large of a number may influence seeks
      break;
    case MEDIA_TRACK_TYPE_AUDIO:
      track = &mAudioTrack;
      maxBuffers = 64;
      break;
    case MEDIA_TRACK_TYPE_SUBTITLE:
      track = &mSubtitleTrack;
      break;
    case MEDIA_TRACK_TYPE_TIMEDTEXT:
      track = &mTimedTextTrack;
      break;
    default:
  }

  if (track->mSource == NULL) {
    return;
  }

  if (actualTimeUs) {
    *actualTimeUs = seekTimeUs;
  }

  MediaSource::ReadOptions options;

  bool seeking = false;
  if (seekTimeUs >= 0) {
    options.setSeekTo(seekTimeUs, mode);
    seeking = true;
  }

  const bool couldReadMultiple = (track->mSource->supportReadMultiple());

  if (couldReadMultiple) {
    options.setNonBlocking();
  }

  int32_t generation = getDataGeneration(trackType);
  for (size_t numBuffers = 0; numBuffers < maxBuffers; ) {
    Vector<MediaBufferBase *> mediaBuffers;
    status_t err = NO_ERROR;

    sp<IMediaSource> source = track->mSource;
    mLock.unlock();
    if (couldReadMultiple) {
      err = source->readMultiple(
          &mediaBuffers, maxBuffers - numBuffers, &options);
    } else {
      MediaBufferBase *mbuf = NULL;
      err = source->read(&mbuf, &options);
      if (err == OK && mbuf != NULL) {
        mediaBuffers.push_back(mbuf);
      }
    }
    mLock.lock();

    options.clearNonPersistent();

    size_t id = 0;
    size_t count = mediaBuffers.size();

    // in case track has been changed since we don't have lock for some time.
    if (generation != getDataGeneration(trackType)) {
      for (; id < count; ++id) {
        mediaBuffers[id]->release();
      }
      break;
    }

    for (; id < count; ++id) {
      int64_t timeUs;
      MediaBufferBase *mbuf = mediaBuffers[id];
      if (!mbuf->meta_data().findInt64(kKeyTime, &timeUs)) {
        mbuf->meta_data().dumpToLog();
        track->mPackets->signalEOS(ERROR_MALFORMED);
        break;
      }
      if (trackType == MEDIA_TRACK_TYPE_AUDIO) {
        mAudioTimeUs = timeUs;
      } else if (trackType == MEDIA_TRACK_TYPE_VIDEO) {
        mVideoTimeUs = timeUs;
      }

      queueDiscontinuityIfNeeded(seeking, formatChange, trackType, track);

      sp<ABuffer> buffer = mediaBufferToABuffer(mbuf, trackType);
      if (numBuffers == 0 && actualTimeUs != nullptr) {
        *actualTimeUs = timeUs;
      }
      if (seeking && buffer != nullptr) {
        sp<AMessage> meta = buffer->meta();
        if (meta != nullptr && mode == MediaPlayerSeekMode::SEEK_CLOSEST
            && seekTimeUs > timeUs) {
          sp<AMessage> extra = new AMessage;
          extra->setInt64("resume-at-mediaTimeUs", seekTimeUs);
          meta->setMessage("extra", extra);
        }
      }

      track->mPackets->queueAccessUnit(buffer);
      formatChange = false;
      seeking = false;
      ++numBuffers;
    }
    if (id < count) {
      // Error, some mediaBuffer doesn't have kKeyTime.
      for (; id < count; ++id) {
        mediaBuffers[id]->release();
      }
      break;
    }

    if (err == WOULD_BLOCK) {
      break;
    } else if (err == INFO_FORMAT_CHANGED) {
#if 0
      track->mPackets->queueDiscontinuity(
                    ATSParser::DISCONTINUITY_FORMATCHANGE,
                    NULL,
                    false /* discard */);
#endif
    } else if (err != OK) {
      queueDiscontinuityIfNeeded(seeking, formatChange, trackType, track);
      track->mPackets->signalEOS(err);
      break;
    }
  }

  if (mIsStreaming
      && (trackType == MEDIA_TRACK_TYPE_VIDEO || trackType == MEDIA_TRACK_TYPE_AUDIO)) {
    status_t finalResult;
    int64_t durationUs = track->mPackets->getBufferedDurationUs(&finalResult);

    // TODO: maxRebufferingMarkMs could be larger than
    // mBufferingSettings.mResumePlaybackMarkMs
    int64_t markUs = (mPreparing ? mBufferingSettings.mInitialMarkMs
                                 : mBufferingSettings.mResumePlaybackMarkMs) * 1000LL;
    if (finalResult == ERROR_END_OF_STREAM || durationUs >= markUs) {
      if (mPreparing || mSentPauseOnBuffering) {
        Track *counterTrack =
            (trackType == MEDIA_TRACK_TYPE_VIDEO ? &mAudioTrack : &mVideoTrack);
        if (counterTrack->mSource != NULL) {
          durationUs = counterTrack->mPackets->getBufferedDurationUs(&finalResult);
        }
        if (finalResult == ERROR_END_OF_STREAM || durationUs >= markUs) {
          if (mPreparing) {
            notifyPrepared();
            mPreparing = false;
          } else {
            sendCacheStats();
            mSentPauseOnBuffering = false;
            sp<AMessage> notify = dupNotify();
            notify->setInt32("what", kWhatResumeOnBufferingEnd);
            notify->post();
          }
        }
      }
      return;
    }

    postReadBuffer(trackType);
  }
}

void DefaultSource::onPrepareAsync() {
  mDisconnectLock.lock();
  ALOGV("onPrepareAsync: mDataSource: %d", (mDataSource != NULL));

  // delayed data source creation
  if (mDataSource == NULL) {
    // set to false first, if the extractor
    // comes back as secure, set it to true then.
    mIsSecure = false;

    if (!mUri.empty()) {
      const char* uri = mUri.c_str();
      String8 contentType;

      //if (!strncasecmp("http://", uri, 7) || !strncasecmp("https://", uri, 8))

      if (property_get_bool("media.stagefright.extractremote", true) &&
          !PlayerServiceFileSource::requiresDrm(
              mFd.get(), mOffset, mLength, nullptr /* mime */)) {
        sp<IBinder> binder =
            defaultServiceManager()->getService(String16("media.extractor"));
        if (binder != nullptr) {
          ALOGD("FileSource remote");
          sp<IMediaExtractorService> mediaExService(
              interface_cast<IMediaExtractorService>(binder));
          sp<IDataSource> source;
          mediaExService->makeIDataSource(base::unique_fd(dup(mFd.get())), mOffset, mLength, &source);
          ALOGV("IDataSource(FileSource): %p %d %lld %lld",
                source.get(), mFd.get(), (long long)mOffset, (long long)mLength);
          if (source.get() != nullptr) {
            mDataSource = CreateDataSourceFromIDataSource(source);
          } else {
            ALOGW("extractor service cannot make data source");
          }
        } else {
          ALOGW("extractor service not running");
        }
      }

    if (mDataSource == NULL) {
      ALOGE("Failed to create data source!");
      mDisconnectLock.unlock();
      notifyPreparedAndCleanup(UNKNOWN_ERROR);
      return;
    }
  }

  if (mDataSource->flags() & DataSource::kIsCachingDataSource) {
    mCachedSource = static_cast<NuCachedSource2 *>(mDataSource.get());
  }

  mDisconnectLock.unlock();

  // For cached streaming cases, we need to wait for enough
  // buffering before reporting prepared.
  mIsStreaming = (mCachedSource != NULL);

  // init extractor from data source
  status_t err = initFromDataSource();

  if (err != OK) {
    ALOGE("Failed to init from data source!");
    notifyPreparedAndCleanup(err);
    return;
  }

  if (mVideoTrack.mSource != NULL) {
    sp<MetaData> meta = getFormatMeta_l(false /* audio */);
    sp<AMessage> msg = new AMessage;
    err = convertMetaDataToMessage(meta, &msg);
    if(err != OK) {
      notifyPreparedAndCleanup(err);
      return;
    }
    notifyVideoSizeChanged(msg);
  }

  notifyFlagsChanged(
      // FLAG_SECURE will be known if/when prepareDrm is called by the app
      // FLAG_PROTECTED will be known if/when prepareDrm is called by the app
      FLAG_CAN_PAUSE |
          FLAG_CAN_SEEK_BACKWARD |
          FLAG_CAN_SEEK_FORWARD |
          FLAG_CAN_SEEK);

  finishPrepareAsync();

  ALOGV("onPrepareAsync: Done");
}

void DefaultSource::postReadBuffer(media_track_type trackType) {
  if ((mPendingReadBufferTypes & (1 << trackType)) == 0) {
    mPendingReadBufferTypes |= (1 << trackType);
    std::shared_ptr<Message> msg = std::make_shared<Message>(kWhatReadBuffer);
    msg->setInt(trackType);
    msg->post();
  }
}

void DefaultSource::stop() {
  std::lock_guard _l(mLock);
  mStarted = false;
}

void DefaultSource::pause() {
  std::lock_guard _l(mLock);
  mStarted = false;
}

void DefaultSource::resume() {
  std::lock_guard _l(mLock);
  mStarted = true;
}
void DefaultSource::disconnect() {
//  sp<DataSource> dataSource, httpSource;
//  {
//    Mutex::Autolock _l_d(mDisconnectLock);
//    dataSource = mDataSource;
//    httpSource = mHttpSource;
//    mDisconnected = true;
//  }
//
//  if (dataSource != NULL) {
//    // disconnect data source
//    if (dataSource->flags() & DataSource::kIsCachingDataSource) {
//      static_cast<NuCachedSource2 *>(dataSource.get())->disconnect();
//    }
//  } else if (httpSource != NULL) {
//    static_cast<HTTPBase *>(httpSource.get())->disconnect();
//  }
}

std::shared_ptr<MetaData> DefaultSource::getFileFormatMeta() const {
  return mFileMeta;
}

status_t DefaultSource::seekTo(int64_t seekTimeUs, SeekMode mode) {
  return Source::seekTo(seekTimeUs, mode);
}

status_t DefaultSource::getDuration(int64_t *durationUs) {
  return mFormatContext->duration;
}

size_t DefaultSource::getTrackCount() const {
  return Source::getTrackCount();
}
bool DefaultSource::isStreaming() const {
  return Source::isStreaming();
}
void DefaultSource::onMessageReceived(const std::shared_ptr<Message> &msg) {
  Source::onMessageReceived(msg);
}
std::shared_ptr<MetaData> DefaultSource::getFormatMeta(bool audio) {
  return Source::getFormatMeta(audio);
}

}