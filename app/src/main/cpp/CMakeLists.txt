# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.22.1)

# Declares and names the project.

project("hpcplayer-native")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        hpcplayer-native

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        hpcplayer/HpcPlayer.cpp
        hpcplayer/HpcCore.cpp
        hpcplayer/common/Looper.cpp
        hpcplayer/common/Handler.cpp
        hpcplayer/common/MediaClock.cpp
        hpcplayer/extractor/FfmpegExtractor.cpp
        hpcplayer/decoder/Decoder.cpp
        hpcplayer/decoder/FfmpegVideoDecoder.cpp
        hpcplayer/decoder/FfmpegAudioDecoder.cpp
        hpcplayer/decoder/andriod/MediaCodecVideoDecoder.cpp
        hpcplayer/renderer/Renderer.cpp
        hpcplayer/renderer/andriod/OpenGLRenderer.cpp
        hpcplayer/renderer/andriod/MediaCodecRenderer.cpp
        hpcplayer/renderer/andriod/AudioOpenSLESRenderer.cpp
        hpcplayer-native.cpp
        HpcPlayerInterface.cpp)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library(android-lib android)
find_library(glesv2-lib GLESv2)
find_library(egl-lib EGL)
find_library(opensles-lib OpenSLES)
find_library(log-lib log)
find_library(mediandk-lib mediandk)


set(FFMPEG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/hpcplayer/ffmpeg/include)
set(FFMPEG_LIB_DIR ${CMAKE_SOURCE_DIR}/../../jniLibs/${ANDROID_ABI})

include_directories(${FFMPEG_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/hpcplayer)
include_directories(${CMAKE_SOURCE_DIR}/decoder)
include_directories(${CMAKE_SOURCE_DIR}/renderer)
include_directories(${CMAKE_SOURCE_DIR}/extractor)

add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION ${FFMPEG_LIB_DIR}/libavcodec.so)
add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION ${FFMPEG_LIB_DIR}/libavformat.so)
add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION ${FFMPEG_LIB_DIR}/libavutil.so)
add_library(swscale SHARED IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION ${FFMPEG_LIB_DIR}/libswscale.so)
add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION ${FFMPEG_LIB_DIR}/libswresample.so)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        hpcplayer-native

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib}
        ${android-lib}
        ${glesv2-lib}
        ${egl-lib}
        ${opensles-lib}
        ${mediandk-lib}
        avcodec
        avformat
        avutil
        swscale
        swresample
        )