# HPC Player

> **⚠️ AI Coding Experiment**
>
> This project is an experimental media player developed with the assistance of **AI Coding**. It serves as a proof-of-concept for building complex, high-performance native applications using AI tools.

HPC Player is a lightweight, high-performance media player currently built for Android using C++ and FFmpeg.

## 🚀 Roadmap & Goals

*   **Continuous Development**: This project will be actively updated with new features, optimizations, and bug fixes.
*   **Cross-Platform Support**: The primary long-term goal is to evolve this into a **Cross-Platform Player**. We aim to port the C++ core to run on Windows, Linux, macOS, and iOS, sharing the same architecture and logic.

## Features

*   **Native C++ Core**: The core player logic is implemented in C++ for performance and portability.
*   **FFmpeg Integration**: Uses FFmpeg for demuxing and audio decoding.
*   **Hardware Acceleration**: Supports Android `MediaCodec` for hardware-accelerated video decoding.
*   **Efficient Rendering**:
    *   **Video**: Uses OpenGL ES 2.0 for efficient YUV to RGB conversion and rendering.
    *   **Audio**: Uses OpenSL ES for low-latency audio playback.
*   **Architecture**:
    *   **Looper/Handler Model**: Implements a custom Looper/Handler mechanism in C++ for thread communication, similar to Android's Java Handler.
    *   **Modular Design**: Separates Extractor, Decoder, and Renderer components.
    *   **Sync Strategy**: Audio-video synchronization based on a media clock.

## Architecture Overview

The player is structured around a central `HpcCore` class that manages the playback lifecycle and coordinates the following components:

*   **Extractor**: Reads media files and extracts audio/video packets (wraps FFmpeg `AVFormatContext`).
*   **Decoder**: Decodes media packets into frames.
    *   `FfmpegAudioDecoder`: Software audio decoding using FFmpeg.
    *   `MediaCodecVideoDecoder`: Hardware video decoding using Android NDK `MediaCodec`.
    *   `FfmpegVideoDecoder`: Software video decoding (fallback).
*   **Renderer**: Renders decoded frames.
    *   `AudioOpenSLESRenderer`: Plays audio using OpenSL ES.
    *   `MediaCodecRenderer`: Handles video frame rendering timing.
    *   `OpenGLRenderer`: Renders video frames to a surface using OpenGL ES shaders.
*   **Common**: Utility classes for threading (`Looper`, `Handler`), logging, and data structures (`DataQueue`).

## Prerequisites

*   Android Studio
*   Android NDK
*   FFmpeg libraries (pre-built binaries required in `app/src/main/cpp/hpcplayer/ffmpeg`)

## Building

1.  Clone the repository.
2.  Ensure you have the required FFmpeg headers and shared libraries placed in the correct directory structure within `app/src/main/cpp/hpcplayer/ffmpeg`.
3.  Open the project in Android Studio.
4.  Build and run the app.

## Usage

The `HpcCore` class provides the main interface for controlling playback:

```cpp
// Create the player
auto player = HpcCore::create();

// Set data source
player->setDataSource("/sdcard/video.mp4");

// Set display surface (ANativeWindow)
player->setSurface(window);

// Prepare and start
player->prepare();
player->start();

// Control
player->pause();
player->resume();
player->seekTo(10000); // Seek to 10s
player->stop();
player->release();
```

## License

[MIT License](LICENSE)
