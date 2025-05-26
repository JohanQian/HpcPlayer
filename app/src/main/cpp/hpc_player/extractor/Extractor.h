#pragma once

#include <cstdint>
#include <memory>
#include "Error.h"

namespace hpc {

class MediaPacket;
class MetaData;

class Extractor {
 public:
  virtual status_t init(const char* url) = 0;

  virtual int read(std::unique_ptr<MediaPacket> &packet, int index) = 0;

  virtual status_t seek(int64_t position) = 0;

  virtual void flush() = 0;

  virtual void getMetaData(MetaData& meta) = 0;

  virtual void release() = 0;
};

} // hpc

