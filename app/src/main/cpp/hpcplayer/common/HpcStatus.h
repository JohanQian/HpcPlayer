#pragma once

#include <cstdint>

// Define a type alias for the underlying status type, similar to Android's status_t
using status_t = int32_t;

/**
 * @brief Defines the status codes used throughout the player engine.
 */
enum class HpcStatus : status_t {
    kOk = 0,

    // General errors
    kUnknownError = -1,
    kNoMemory = -2,
    kBadValue = -3,
    kBadType = -4,
    kNoInit = -5,
    kBadIndex = -6,

    // Stream-related status
    kEof = -100,      // End of stream
    kAgain = -101,    // Operation needs to be tried again (e.g., decoder needs more data)

    // Operation-specific status
    kWouldBlock = -200,
};
