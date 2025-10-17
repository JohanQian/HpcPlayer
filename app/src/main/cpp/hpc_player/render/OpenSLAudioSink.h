c++
c++
#ifndef HPC_PLAYER_OPENSL_AUDIO_SINK_H_
#define HPC_PLAYER_OPENSL_AUDIO_SINK_H_

extern "C" {
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
}

#include <memory>
#include <mutex>

#include "Rendered.h"
#include "AudioSink.h"

namespace hpc {

class OpenSLAudioSink : public AudioSink {
 public:
  OpenSLAudioSink();
  ~OpenSLAudioSink() override;

  status_t Open(int sample_rate, int channels, int format) override;
  status_t Write(const void* data, size_t size) override;
  status_t Pause() override;
  status_t Resume() override;
  status_t Close() override;
  int64_t GetPlayedTimeUs() const override;

  OpenSLAudioSink(const OpenSLAudioSink&) = delete;
  OpenSLAudioSink& operator=(const OpenSLAudioSink&) = delete;

 private:
  status_t Initialize(int sample_rate, int channels, int format);
  static void BufferQueueCallback(SLAndroidSimpleBufferQueueItf buffer_queue,
                                  void* context);
  void Cleanup();

  // OpenSL ES objects
  SLObjectItf mEngineObject = nullptr;
  SLEngineItf mEngineEngine = nullptr; // Or mSlEngineInterface
  SLObjectItf mOutputMixObject = nullptr;
  SLObjectItf mPlayerObject = nullptr;
  SLPlayItf mPlayerPlay = nullptr;     // Or mSlPlayInterface
  SLAndroidSimpleBufferQueueItf mPlayerBufferQueue = nullptr; // Or mSlBufferQueueInterface

  // Audio stream metadata
  int mSampleRate = 0;
  int mChannels = 0;
  int64_t mPlayedSamples = 0;

  mutable std::mutex mMutex;
  bool mInitialized = false;
};

}  // namespace hpc

#endif  // HPC_PLAYER_OPENSL_AUDIO_SINK_H_