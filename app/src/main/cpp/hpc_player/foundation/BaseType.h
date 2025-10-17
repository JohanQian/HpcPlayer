#pragma once

#include <stdint.h>

namespace hpc{
enum media_event_type {
  MEDIA_NOP               = 0, // interface test message
  MEDIA_PREPARED          = 1,
  MEDIA_PLAYBACK_COMPLETE = 2,
  MEDIA_BUFFERING_UPDATE  = 3,
  MEDIA_SEEK_COMPLETE     = 4,
  MEDIA_SET_VIDEO_SIZE    = 5,
  MEDIA_STARTED           = 6,
  MEDIA_PAUSED            = 7,
  MEDIA_STOPPED           = 8,
  MEDIA_SKIPPED           = 9,
  MEDIA_NOTIFY_TIME       = 98,
  MEDIA_TIMED_TEXT        = 99,
  MEDIA_ERROR             = 100,
  MEDIA_INFO              = 200,
  MEDIA_SUBTITLE_DATA     = 201,
  MEDIA_META_DATA         = 202,
  MEDIA_TIME_DISCONTINUITY = 211,
  MEDIA_IMS_RX_NOTICE     = 300,
  MEDIA_AUDIO_ROUTING_CHANGED = 10000,
};

enum SeekMode : int32_t {
  SEEK_PREVIOUS_SYNC = 0,
  SEEK_NEXT_SYNC,
  SEEK_CLOSEST_SYNC,
  SEEK_CLOSEST,
  SEEK_FRAME_INDEX,
};
}