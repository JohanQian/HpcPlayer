#pragma once

#include <cstdint>

namespace hpc {

// A simple definition for status_t, commonly used in Android native code.
using status_t = int32_t;

/**
 * @brief Defines messages that the player sends to listeners.
 * This is similar to Android's media player message system.
 */
enum HpcMessage : int32_t {
    // State changes
    MSG_STATE_CHANGED = 1,

    // Operation completion notifications
    MSG_SET_DATA_SOURCE_COMPLETED = 100,
    MSG_PREPARE_COMPLETED = 101,
    MSG_SEEK_COMPLETED = 102,
    MSG_STOP_COMPLETED = 103,
    MSG_PLAYBACK_COMPLETED = 104,

    // Informational notifications
    MSG_DURATION_CHANGED = 200,

    // Generic error
    MSG_ERROR = 900,
};

} // namespace hpc
