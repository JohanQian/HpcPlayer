#pragma once

#include "Renderer.h"
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

// Forward Declarations for Android NDK
class AMediaCodec;
class AMediaFormat;
class ANativeWindow;

namespace anplayer {

/**
 * @class MediaCodecVideoRenderer
 * @brief An implementation of Renderer for video tracks using Android MediaCodec.
 *
 * This class handles decoding and rendering of video data directly to an ANativeWindow
 * for hardware-accelerated playback.
 */
class MediaCodecVideoRenderer : public Renderer {
 public:
  MediaCodecVideoRenderer();
  ~MediaCodecVideoRenderer();

  // Implements Renderer interface
  TrackType getTrackType() const override;
  int32_t supportsFormat(const Format& format) const override;
  void enable(const Format& format, SampleStream* stream, int64_t positionUs) override;
  void start() override;
  void stop() override;
  void disable() override;
  void reset() override;
  void render(int64_t positionUs, int64_t elapsedRealtimeUs) override;
  bool isEnded() const override;
  bool isReady() const override;

  /**
   * @brief Sets the native window surface for rendering.
   *
   * This is a specific method for this renderer, required to provide the
   * rendering target (e.g., from a SurfaceView).
   * @param window The native window to render to.
   */
  void setOutputSurface(ANativeWindow* window);

 private:
  // Internal state
  bool mIsEnabled = false;
  bool mIsStarted = false;
  bool mIsEnded = false;
  SampleStream* mSampleStream = nullptr;
  ANativeWindow* mOutputSurface = nullptr;

  // MediaCodec resources
  AMediaCodec* mCodec = nullptr;

  // Decoding Thread
  std::thread mDecodeThread;
  bool mStopDecodeThread = false;

  void decodeLoop();
};

} // namespace anplayer