#pragma once

#include <android/log.h>

namespace hpc {

/**
 * A set of logging macros that automatically use a "kTag" variable
 * expected to be defined in the compilation unit.
 *
 * Example Usage in a .cpp file:
 *
 * namespace {
 *     constexpr char kTag[] = "MyCoolClass";
 * } // namespace
 *
 * ...
 * LOG_I("Initialization complete: %d", status);
 * LOG_E("Failed to open file: %s", strerror(errno));
 *
 */

#define LOG_I(...) \
    __android_log_print(ANDROID_LOG_INFO, kTag, __VA_ARGS__)

#define LOG_E(...) \
    __android_log_print(ANDROID_LOG_ERROR, kTag, __VA_ARGS__)

#define LOG_W(...) \
    __android_log_print(ANDROID_LOG_WARN, kTag, __VA_ARGS__)

#define LOG_D(...) \
    __android_log_print(ANDROID_LOG_DEBUG, kTag, __VA_ARGS__)

#define LOG_V(...) \
    __android_log_print(ANDROID_LOG_VERBOSE, kTag, __VA_ARGS__)

} // namespace hpc
