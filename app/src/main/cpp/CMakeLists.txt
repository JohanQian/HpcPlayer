cmake_minimum_required(VERSION 3.22.1)

project("hpcplayer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -Wall")

# 设置FFmpeg路径
set(FFMPEG_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../libs/ffmpeg)
SET(BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../bin/${ANDROID_ABI})
include_directories(${FFMPEG_DIR}/include)
file(COPY ${FFMPEG_DIR} DESTINATION ${BIN_DIR})

include_directories(${CMAKE_CURRENT_LIST_DIR}/hpc_player/foundation)
file(GLOB NV_FOUNDATION ${CMAKE_CURRENT_LIST_DIR}/hpc_player/foundation/*.cpp)

include_directories(${CMAKE_CURRENT_LIST_DIR}/hpc_player/source)
file(GLOB NV_SOURCES ${CMAKE_CURRENT_LIST_DIR}/hpc_player/source/*.cpp)

include_directories(${CMAKE_CURRENT_LIST_DIR}/hpc_player/extractor)
file(GLOB EXTRACTOR ${CMAKE_CURRENT_LIST_DIR}/hpc_player/extractor/*.cpp)


# 添加NDK和FFmpeg库
find_library(log-lib log)
find_library(android-lib android)


add_library(
        hpcplayer
        SHARED
        ${NV_SOURCES}
        ${NV_FOUNDATION}
        ${EXTRACTOR}
        native-lib.cpp
        hpc_player/HpcPlayer.cpp
        hpc_player/HpcPlayer.h
        hpc_player/HpcPlayerInternal.cpp
        hpc_player/HpcPlayerInternal.h)

# 链接FFmpeg库
target_link_libraries(
        hpcplayer
        log
        android
        mediandk
        jni_helper
        ${FFMPEG_DIR}/${ANDROID_ABI}/libavcodec.so
        ${FFMPEG_DIR}/${ANDROID_ABI}/libavformat.so
        ${FFMPEG_DIR}/${ANDROID_ABI}/libavutil.so
        ${FFMPEG_DIR}/${ANDROID_ABI}/libswresample.so
        ${FFMPEG_DIR}/${ANDROID_ABI}/libswscale.so)