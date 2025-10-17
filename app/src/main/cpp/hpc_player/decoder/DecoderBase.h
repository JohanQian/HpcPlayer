#pragma once

#include "../HpcPlayer.h"
#include "../foundation/Handler.h"
#include "../foundation/Message.h"

namespace hpc {

struct ABuffer;
struct MediaCodec;
class MediaBuffer;
class MediaCodecBuffer;
class Surface;

struct DecoderBase : public Handler {
  explicit DecoderBase(const std::shared_ptr<Message> &notify);

  void configure(const std::shared_ptr<Message> &format);
  void init();
  void setParameters(const std::shared_ptr<Message> &params);

  // Synchronous call to ensure decoder will not request or send out data.
  void pause();

  void setRenderer(const std::shared_ptr<Renderer> &renderer);
  virtual status_t setVideoSurface(const std::shared_ptr<Surface> &) { return INVALID_OPERATION; }

  void signalFlush();
  void signalResume(bool notifyComplete);
  void initiateShutdown();

  virtual std::shared_ptr<Message> getStats() {
    return mStats;
  }

  virtual status_t releaseCrypto() {
    return INVALID_OPERATION;
  }

  enum {
    kWhatInputDiscontinuity  = 'inDi',
    kWhatVideoSizeChanged    = 'viSC',
    kWhatFlushCompleted      = 'flsC',
    kWhatShutdownCompleted   = 'shDC',
    kWhatResumeCompleted     = 'resC',
    kWhatEOS                 = 'eos ',
    kWhatError               = 'err ',
  };

 protected:

  virtual ~DecoderBase();

  void stopLooper();

  void onMessageReceived(const std::shared_ptr<Message> &msg) override;

  virtual void onConfigure(const std::shared_ptr<Message> &format) = 0;
  virtual void onSetParameters(const std::shared_ptr<Message> &params) = 0;
  virtual void onSetRenderer(const std::shared_ptr<Renderer> &renderer) = 0;
  virtual void onResume(bool notifyComplete) = 0;
  virtual void onFlush() = 0;
  virtual void onShutdown(bool notifyComplete) = 0;

  void onRequestInputBuffers();
  virtual bool doRequestBuffers() = 0;
  virtual void handleError(int32_t err);

  std::shared_ptr<Message> mNotify;
  int32_t mBufferGeneration;
  bool mPaused;
  std::shared_ptr<Message> mStats;
  std::mutex mStatsLock;

 private:
  enum {
    kWhatConfigure           = 'conf',
    kWhatSetParameters       = 'setP',
    kWhatSetRenderer         = 'setR',
    kWhatPause               = 'paus',
    kWhatRequestInputBuffers = 'reqB',
    kWhatFlush               = 'flus',
    kWhatShutdown            = 'shuD',
  };

  std::shared_ptr<Looper> mDecoderLooper;
  bool mRequestInputBuffersPending;

};

}

