#pragma once
#include "Error.h"
#include "Handler.h"

namespace hpc {

struct Looper;
class ANativeWindow;
class Source;
class HpcPlayer;
class MediaClock;
class Decoder;
class Renderer;

class HpcPlayerInternal : public Handler{
 public:
  explicit HpcPlayerInternal(pid_t pid, const std::shared_ptr<MediaClock> &mediaClock);


  void init(const std::weak_ptr<HpcPlayer> &driver);

  void setDataSourceAsync(const char* url);

  void prepareAsync();

  status_t setVideoSurface(ANativeWindow* window);

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
    kWhatGetBufferingSettings       = 'gBus',
    kWhatSetBufferingSettings       = 'sBuS',
    kWhatMediaClockNotify           = 'mckN',
  };

  std::weak_ptr<HpcPlayer> mPlayer;
  bool mUIDValid;
  uid_t mUID;
  pid_t mPID;
  const std::shared_ptr<MediaClock> mMediaClock;
  std::mutex mSourceLock;  // guard |mSource|.
  std::shared_ptr<Source> mSource;
  uint32_t mSourceFlags;
  std::shared_ptr<ANativeWindow> mSurface;
  std::shared_ptr<MediaPlayerBase::AudioSink> mAudioSink;
  std::shared_ptr<DecoderBase> mVideoDecoder;
  bool mOffloadAudio;
  std::shared_ptr<DecoderBase> mAudioDecoder;
  std::mutex mDecoderLock;  // guard |mAudioDecoder| and |mVideoDecoder|.
  std::shared_ptr<Decoder> mCCDecoder;
  std::shared_ptr<Renderer> mRenderer;
  std::shared_ptr<Looper> mRendererLooper;
  int32_t mAudioDecoderGeneration;
  int32_t mVideoDecoderGeneration;
  int32_t mRendererGeneration;

  int32_t mPollDurationGeneration;

};

}  // namespace android



