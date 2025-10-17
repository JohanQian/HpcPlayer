#pragma once

//#include "../HpcPlayerInternal.h"
#include "Source.h"
#include "Error.h"

namespace hpc {

struct AnotherPacketSource;
struct ARTSPController;
class DataSource;
class IDataSource;
struct IMediaHTTPService;
struct Extractor;
class IMediaSource;
class MediaBuffer;
class MediaClock;
struct NuCachedSource2;
struct MetaData;

class DefaultSource : public Source {
 public:
  DefaultSource(const std::shared_ptr<Message> &notify, bool uidValid, uid_t uid,
                const std::shared_ptr<MediaClock> &mediaClock);
  virtual ~DefaultSource();


  status_t setDataSource(const char *url);

  void prepareAsync() override;

  void start() override;
  void stop() override;
  void pause() override;
  void resume() override;

  void disconnect() override;

  std::shared_ptr<MetaData> getFileFormatMeta() const override;

  //virtual status_t dequeueAccessUnit(bool audio, std::shared_ptr<ABuffer> *accessUnit);

  virtual status_t getDuration(int64_t *durationUs);
  virtual size_t getTrackCount() const;
//  virtual ssize_t getSelectedTrack(media_track_type type) const;
//  virtual status_t selectTrack(size_t trackIndex, bool select, int64_t timeUs);
  virtual status_t seekTo(int64_t seekTimeUs,SeekMode mode = SEEK_PREVIOUS_SYNC) override;

  virtual bool isStreaming() const;


 protected:

  virtual void onMessageReceived(const std::shared_ptr<Message> &msg);

  virtual std::shared_ptr<MetaData> getFormatMeta(bool audio);

 private:
  enum {
    kWhatPrepareAsync,
    kWhatFetchSubtitleData,
    kWhatFetchTimedTextData,
    kWhatSendSubtitleData,
    kWhatSendGlobalTimedTextData,
    kWhatSendTimedTextData,
    kWhatChangeAVSource,
    kWhatPollBuffering,
    kWhatSeek,
    kWhatReadBuffer,
    kWhatStart,
    kWhatResume,
    kWhatSecureDecodersInstantiated,
  };

  struct Track {
    size_t mIndex;
    std::shared_ptr<Extractor> mExtractor;
  };

  //AVFormatContext* mFormatContext;
  std::vector<std::shared_ptr<Extractor> > mExtractor;
  Track mAudioTrack;
  int64_t mAudioTimeUs;
  int64_t mAudioLastDequeueTimeUs;
  Track mVideoTrack;
  int64_t mVideoTimeUs;
  int64_t mVideoLastDequeueTimeUs;
  size_t mSubtitleTrack;
  size_t mTimedTextTrack;

  bool mSentPauseOnBuffering;

  int32_t mAudioDataGeneration;
  int32_t mVideoDataGeneration;
  int32_t mFetchSubtitleDataGeneration;
  int32_t mFetchTimedTextDataGeneration;
  int64_t mDurationUs;
  bool mAudioIsVorbis;
  // Secure codec is required.
  bool mIsSecure;
  bool mIsStreaming;
  bool mUIDValid;
  uid_t mUID;
  const std::shared_ptr<MediaClock> mMediaClock;
  std::string mUri;
  //KeyedVector<String8, String8> mUriHeaders;
//  base::unique_fd mFd;
//  int64_t mOffset;
//  int64_t mLength;

  bool mDisconnected;
  std::shared_ptr<DataSource> mDataSource;
  std::shared_ptr<NuCachedSource2> mCachedSource;
  std::shared_ptr<DataSource> mHttpSource;
  std::shared_ptr<MetaData> mFileMeta;
  bool mStarted;
  bool mPreparing;
  int64_t mBitrate;
  uint32_t mPendingReadBufferTypes;

  mutable std::mutex mLock;
  mutable std::mutex mDisconnectLock; // Protects mDataSource, mHttpSource and mDisconnected

  std::shared_ptr<Looper> mLooper;

  void resetDataSource();

  status_t initFromDataSource();
  int64_t getLastReadPosition();

  void notifyPreparedAndCleanup(status_t err);
  void onSecureDecodersInstantiated(status_t err);
  void finishPrepareAsync();
  status_t startSources();

  void onSeek(const std::shared_ptr<AMessage>& msg);
  status_t doSeek(int64_t seekTimeUs, MediaPlayerSeekMode mode);

  void onPrepareAsync();

  void postReadBuffer(media_track_type trackType);
  void onReadBuffer(const std::shared_ptr<AMessage>& msg);
  // When |mode| is MediaPlayerSeekMode::SEEK_CLOSEST, the buffer read shall
  // include an item indicating skipping rendering all buffers with timestamp
  // earlier than |seekTimeUs|.
  // For other modes, the buffer read will not include the item as above in order
  // to facilitate fast seek operation.
  void readBuffer(
      media_track_type trackType,
      int64_t seekTimeUs = -1ll,
      MediaPlayerSeekMode mode = MediaPlayerSeekMode::SEEK_PREVIOUS_SYNC,
      int64_t *actualTimeUs = NULL, bool formatChange = false);

  void queueDiscontinuityIfNeeded(
      bool seeking, bool formatChange, media_track_type trackType, Track *track);

  void schedulePollBuffering();
  void onPollBuffering();
  void notifyBufferingUpdate(int32_t percentage);

  void sendCacheStats();

  std::shared_ptr<MetaData> getFormatMeta_l(bool audio);
  int32_t getDataGeneration(media_track_type type) const;

};

}

