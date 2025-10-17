#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "../HpcPlayerInternal.h"

namespace hpc {

// Forward Declarations
class Format;
class SampleStream;

class Renderer {
 public:
  /**
   * @brief Virtual destructor to ensure proper cleanup of derived classes.
   */
  virtual ~Renderer() = default;

  virtual void enable(const Format& format, SampleStream* stream, int64_t positionUs) = 0;

  virtual void start() = 0;

  virtual void stop() = 0;

  virtual void disable() = 0;

  virtual void reset() = 0;

  virtual void render(int64_t positionUs, int64_t elapsedRealtimeUs) = 0;

  virtual bool isEnded() const = 0;

  virtual bool isReady() const = 0;

  virtual status_t setPlaybackSettings(float rate) = 0;

};

} // namespace hpc