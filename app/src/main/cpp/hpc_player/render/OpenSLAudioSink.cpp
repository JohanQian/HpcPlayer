#include "OpenSLAudioSink.h"

#include <cassert>
#include <cstring>

namespace hpc {

OpenSLAudioSink::OpenSLAudioSink() {}

OpenSLAudioSink::~OpenSLAudioSink() {
  Close();
}

status_t OpenSLAudioSink::Open(int sampleRate, int channels, int format) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (mInitialized) return OK;

  mSampleRate = sampleRate;
  mChannels = channels;
  return Initialize(sampleRate, channels, format);
}

status_t OpenSLAudioSink::Initialize(int sampleRate, int channels, int format) {
  // Create OpenSL ES engine
  SLresult result = slCreateEngine(&mEngineObject, 0, nullptr, 0, nullptr, nullptr);
  if (result != SL_RESULT_SUCCESS) return ERROR_UNKNOWN;

  result = (*mEngineObject)->Realize(mEngineObject, SL_BOOLEAN_FALSE);
  if (result != SL_RESULT_SUCCESS) {
    Cleanup();
    return ERROR_UNKNOWN;
  }

  result = (*mEngineObject)->GetInterface(mEngineObject, SL_IID_ENGINE, &mEngine);
  if (result != SL_RESULT_SUCCESS) {
    Cleanup();
    return ERROR_UNKNOWN;
  }

  // Create output mix
  result = (*mEngine)->CreateOutputMix(mEngine, &mOutputMixObject, 0, nullptr, nullptr);
  if (result != SL_RESULT_SUCCESS) {
    Cleanup();
    return ERROR_UNKNOWN;
  }

  result = (*mOutputMixObject)->Realize(mOutputMixObject, SL_BOOLEAN_FALSE);
  if (result != SL_RESULT_SUCCESS) {
    Cleanup();
    return ERROR_UNKNOWN;
  }

  // Configure audio player with buffer queue
  SLDataLocator_AndroidSimpleBufferQueue bufferQueue = {
      SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};  // 2 buffers
  SLDataFormat_PCM pcmFormat = {
      SL_DATAFORMAT_PCM,
      static_cast<SLuint32>(channels),
      static_cast<SLuint32>(sampleRate * 1000),  // Hz to mHz
      SL_PCMSAMPLEFORMAT_FIXED_16,              // 16-bit PCM
      SL_PCMSAMPLEFORMAT_FIXED_16,
      channels == 1 ? SL_SPEAKER_FRONT_CENTER : SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
      SL_BYTEORDER_LITTLEENDIAN};

  SLDataSource audioSource = {&bufferQueue, &pcmFormat};
  SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, mOutputMixObject};
  SLDataSink audioSink = {&outputMix, nullptr};

  const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_PLAY};
  const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
  result = (*mEngine)->CreateAudioPlayer(mEngine, &mPlayerObject, &audioSource, &audioSink,
                                         sizeof(ids) / sizeof(ids[0]), ids, req);
  if (result != SL_RESULT_SUCCESS) {
    Cleanup();
    return ERROR_UNKNOWN;
  }

  result = (*mPlayerObject)->Realize(mPlayerObject, SL_BOOLEAN_FALSE);
  if (result != SL_RESULT_SUCCESS) {
    Cleanup();
    return ERROR_UNKNOWN;
  }

  result = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_PLAY, &mPlayer);
  if (result != SL_RESULT_SUCCESS) {
    Cleanup();
    return ERROR_UNKNOWN;
  }

  result = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &mBufferQueue);
  if (result != SL_RESULT_SUCCESS) {
    Cleanup();
    return ERROR_UNKNOWN;
  }

  // Register buffer queue callback
  result = (*mBufferQueue)->RegisterCallback(mBufferQueue, BufferQueueCallback, this);
  if (result != SL_RESULT_SUCCESS) {
    Cleanup();
    return ERROR_UNKNOWN;
  }

  // Set player to playing state
  result = (*mPlayer)->SetPlayState(mPlayer, SL_PLAYSTATE_PLAYING);
  if (result != SL_RESULT_SUCCESS) {
    Cleanup();
    return ERROR_UNKNOWN;
  }

  mInitialized = true;
  return OK;
}

status_t OpenSLAudioSink::Write(const void* data, size_t size) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mInitialized) return ERROR_INVALID_FORMAT;

  // Enqueue buffer to OpenSL ES
  SLresult result = (*mBufferQueue)->Enqueue(mBufferQueue, data, size);
  if (result != SL_RESULT_SUCCESS) {
    return ERROR_UNKNOWN;
  }

  // Update played samples (assuming 16-bit PCM)
  mPlayedSamples += size / (mChannels * 2);  // 2 bytes per sample
  return OK;
}

status_t OpenSLAudioSink::Pause() {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mInitialized) return OK;

  SLresult result = (*mPlayer)->SetPlayState(mPlayer, SL_PLAYSTATE_PAUSED);
  return result == SL_RESULT_SUCCESS ? OK : ERROR_UNKNOWN;
}

status_t OpenSLAudioSink::Resume() {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mInitialized) return OK;

  SLresult result = (*mPlayer)->SetPlayState(mPlayer, SL_PLAYSTATE_PLAYING);
  return result == SL_RESULT_SUCCESS ? OK : ERROR_UNKNOWN;
}

status_t OpenSLAudioSink::Close() {
  std::lock_guard<std::mutex> lock(mMutex);
  Cleanup();
  mInitialized = false;
  mPlayedSamples = 0;
  return OK;
}

int64_t OpenSLAudioSink::GetPlayedTimeUs() const {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mInitialized || mSampleRate == 0) return 0;
  return (mPlayedSamples * 1000000LL) / mSampleRate;
}

void OpenSLAudioSink::BufferQueueCallback(SLAndroidSimpleBufferQueueItf bufferQueue, void* context) {
  // Empty callback; used to signal buffer completion
  // Actual buffer management handled by AudioRenderer
}

void OpenSLAudioSink::Cleanup() {
  if (mPlayerObject) {
    (*mPlayerObject)->Destroy(mPlayerObject);
    mPlayerObject = nullptr;
    mPlayer = nullptr;
    mBufferQueue = nullptr;
  }
  if (mOutputMixObject) {
    (*mOutputMixObject)->Destroy(mOutputMixObject);
    mOutputMixObject = nullptr;
  }
  if (mEngineObject) {
    (*mEngineObject)->Destroy(mEngineObject);
    mEngineObject = nullptr;
    mEngine = nullptr;
  }
}

}  // namespace hpc