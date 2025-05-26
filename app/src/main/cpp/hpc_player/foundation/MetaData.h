#pragma once

#include <string>

namespace hpc {

struct Rect {
  int32_t mLeft, mTop, mRight, mBottom;
};

struct MetaData {
  MetaData() = default;
  MetaData(const MetaData &from);
  MetaData& operator = (const MetaData &);

  int width {0};
  int height {0};
  std::string mime;
  int displayWidth {0};
  int displayHeight {0};
  int channelCount {0};
  int sampleRate {0};
  int frameRate {0};
  int BitRate {0};
  int maxBitRate {0};
  int bitsPerSample {0};
};

} // hpc

