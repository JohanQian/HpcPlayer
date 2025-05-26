package com.example.hpcplayer

import android.content.Context
import android.media.AudioManager
import android.net.Uri
import android.view.Surface

class HpcPlayer(context: Context) {
    private val PlayerPtr: Long = 0

    init {
        Init()
        // 设置音频流类型
        val audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        audioManager.setStreamVolume(AudioManager.STREAM_MUSIC,
            audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC) / 2, 0)
    }

    private external fun Init()
    external fun SetDataSource(path: String)
    external fun SetSurface(surface: Surface)
    external fun Prepare()
    external fun Start()
    external fun Pause()
    external fun Stop()
    external fun SeekTo(position: Long)
    external fun GetCurrentPosition(): Long
    external fun GetDuration(): Long
    external fun IsPlaying(): Boolean
    external fun Release()

    fun setDataSource(uri: Uri) {
        SetDataSource(uri.toString())
    }

    fun setSurface(surface: Surface) {
        SetSurface(surface)
    }

    fun prepare() {
        Prepare()
    }

    fun start() {
        Start()
    }

    fun pause() {
        Pause()
    }

    fun stop() {
        Stop()
    }

    fun seekTo(position: Long) {
        SeekTo(position)
    }

    fun getCurrentPosition(): Long {
        return GetCurrentPosition()
    }

    fun getDuration(): Long {
        return GetDuration()
    }

    fun isPlaying(): Boolean {
        return IsPlaying()
    }

    fun release() {
        Release()
    }

    companion object {
        init {
            System.loadLibrary("hpcplayer")
        }
    }
}