#pragma once
#include "Error.h"
#include "Handler.h"
#include "BaseType.h"

namespace hpc {

struct Looper;
class Source;
class HpcPlayer;
class MediaClock;
class Decoder;
class Renderer;
class AudioSink;
class Surface;

class HpcPlayerInternal : public Handler{
 public:
  explicit HpcPlayerInternal(const std::shared_ptr<MediaClock> &mediaClock);


  void init(const std::weak_ptr<HpcPlayer> &driver);

  void setDataSourceAsync(const char* url);

  void prepareAsync();

  status_t setVideoSurface(Surface* surface);

//  void setAudioSink(const std::shared_ptr<MediaPlayerBase::AudioSink> &sink);
//  status_t setPlaybackSettings(const AudioPlaybackRate &rate);
//  status_t getPlaybackSettings(AudioPlaybackRate *rate /* nonnull */);

  void start();

  void pause();

  // Will notify the driver through "notifyResetComplete" once finished.
  void resetAsync();

  // Request a notification when specified media time is reached.
  status_t notifyAt(int64_t mediaTimeUs);

  // Will notify the driver through "notifySeekComplete" once finished
  // and needNotify is true.
  void seekTo(int64_t seekTimeUs);

  status_t setVideoScalingMode(int32_t mode);
  //status_t getTrackInfo(Parcel* reply) const;
  //status_t getSelectedTrack(int32_t type, Parcel* reply) const;
  //status_t selectTrack(size_t trackIndex, bool select, int64_t timeUs);
  status_t getCurrentPosition(int64_t *mediaUs);
  //void getStats(Vector<std::shared_ptr<AMessage> > *trackStats);

  float getFrameRate();

  void updateInternalTimers();

  void setTargetBitrate(int bitrate /* bps */);

 protected:
  virtual ~HpcPlayerInternal();

  void onMessageReceived(const std::shared_ptr<Message> &msg) override;

 private:
  struct Action;
  struct SeekAction;
  struct SetSurfaceAction;
  struct ResumeDecoderAction;
  struct FlushDecoderAction;
  struct PostMessageAction;
  struct SimpleAction;

  enum {
    kWhatSetDataSource              = '=DaS',
    kWhatPrepare                    = 'prep',
    kWhatSetVideoSurface            = '=VSu',
    kWhatSetAudioSink               = '=AuS',
    kWhatMoreDataQueued             = 'more',
    kWhatConfigPlayback             = 'cfPB',
    kWhatConfigSync                 = 'cfSy',
    kWhatGetPlaybackSettings        = 'gPbS',
    kWhatGetSyncSettings            = 'gSyS',
    kWhatStart                      = 'strt',
    kWhatStop                       = 'stop',
    kWhatScanSources                = 'scan',
    kWhatVideoNotify                = 'vidN',
    kWhatAudioNotify                = 'audN',
    kWhatClosedCaptionNotify        = 'capN',
    kWhatRendererNotify             = 'renN',
    kWhatReset                      = 'rset',
    kWhatNotifyTime                 = 'nfyT',
    kWhatSeek                       = 'seek',
    kWhatPause                      = 'paus',
    kWhatResume                     = 'rsme',
    kWhatPollDuration               = 'polD',
    kWhatSourceNotify               = 'srcN',
    kWhatGetTrackInfo               = 'gTrI',
    kWhatGetSelectedTrack           = 'gSel',
    kWhatSelectTrack                = 'selT',
    kWhatMediaClockNotify           = 'mckN',
  };

  enum FlushStatus {
    NONE,
    FLUSHING_DECODER,
    FLUSHING_DECODER_SHUTDOWN,
    SHUTTING_DOWN_DECODER,
    FLUSHED,
    SHUT_DOWN,
  };

  enum FlushCommand {
    FLUSH_CMD_NONE,
    FLUSH_CMD_FLUSH,
    FLUSH_CMD_SHUTDOWN,
  };

  void updateVideoSize(
      const std::shared_ptr<Message> &inputFormat,
      const std::shared_ptr<Message> &outputFormat = nullptr);

  void notifyListener(int msg, int ext1, int ext2);

  void handleFlushComplete(bool audio, bool isDecoder);
  void finishFlushIfPossible();

  void onStart(
      int64_t startPositionUs = -1,
      SeekMode mode = SEEK_PREVIOUS_SYNC);
  void onResume();
  void onPause();

  bool audioDecoderStillNeeded();

  void finishResume();
  void notifyDriverSeekComplete();

  void postScanSources();

  void schedulePollDuration();
  void cancelPollDuration();

  void updatePlaybackTimer(bool stopping, const char *where);
  void startPlaybackTimer(const char *where);

  int64_t mLastStartedRebufferingTimeNs;
  void startRebufferingTimer();
  void updateRebufferingTimer(bool stopping, bool exitingPlayback);

  void processDeferredActions();

  void flushDecoder(bool audio, bool needShutdown);
  void performSeek(int64_t seekTimeUs, SeekMode mode);
  void performDecoderFlush(FlushCommand audio, FlushCommand video);
  void performReset();
  void performScanSources();
  void performSetSurface(const std::shared_ptr<Surface> &wrapper);
  void performResumeDecoders(bool needNotify);

  inline std::shared_ptr<Decoder> getDecoder(bool audio) {
    return audio ? mAudioDecoder : mVideoDecoder;
  }

  std::weak_ptr<HpcPlayer> mPlayer;
  bool mUIDValid;
  uid_t mUID;
  const std::shared_ptr<MediaClock> mMediaClock;
  std::mutex mSourceLock;  // guard |mSource|.
  std::shared_ptr<Source> mSource;
  uint32_t mSourceFlags;
  std::shared_ptr<Surface> mSurface;
  std::shared_ptr<AudioSink> mAudioSink;
  std::shared_ptr<Decoder> mVideoDecoder;
  std::shared_ptr<Decoder> mAudioDecoder;
  std::mutex mDecoderLock;  // guard |mAudioDecoder| and |mVideoDecoder|.
  std::mutex mPlayingTimeLock;
  std::shared_ptr<Decoder> mCCDecoder;
  std::shared_ptr<Renderer> mRenderer;
  std::shared_ptr<Looper> mRendererLooper;
  int32_t mAudioDecoderGeneration;
  int32_t mVideoDecoderGeneration;
  int32_t mRendererGeneration;
  int64_t mPreviousSeekTimeUs;
  std::list<std::shared_ptr<Action> > mDeferredActions;

  int32_t mPollDurationGeneration;
  int32_t mTimedTextGeneration;
  float mPlaybackRate;

  bool mFlushComplete[2][2];

  FlushStatus mFlushingAudio;
  FlushStatus mFlushingVideo;
  bool mAudioEOS;
  bool mVideoEOS;
  bool mStarted;

  bool mPaused;
  bool mPausedByClient;
  bool mPausedForBuffering;

  bool mScanSourcesPending;
  int32_t mScanSourcesGeneration;


};

}  // namespace android



