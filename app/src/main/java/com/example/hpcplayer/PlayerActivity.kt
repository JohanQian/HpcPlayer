package com.example.hpcplayer;

import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.media.MediaPlayer
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowManager
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import java.io.IOException
import java.util.*
import kotlin.math.roundToInt

class PlayerActivity : AppCompatActivity(), SurfaceHolder.Callback,
    MediaPlayer.OnPreparedListener, MediaPlayer.OnErrorListener,
    MediaPlayer.OnCompletionListener, MediaPlayer.OnBufferingUpdateListener,
    MediaPlayer.OnInfoListener, SeekBar.OnSeekBarChangeListener {

    private lateinit var surfaceView: SurfaceView
    private lateinit var surfaceHolder: SurfaceHolder
    private lateinit var mediaPlayer: MediaPlayer

    private lateinit var topControls: LinearLayout
    private lateinit var bottomControls: LinearLayout
    private lateinit var progressBar: ProgressBar
    private lateinit var tvError: TextView
    private lateinit var btnRetry: Button
    private lateinit var btnPlayPause: ImageButton
    private lateinit var btnBack: ImageButton
    private lateinit var btnFullscreen: ImageButton
    private lateinit var tvTitle: TextView
    private lateinit var tvCurrentTime: TextView
    private lateinit var tvTotalTime: TextView
    private lateinit var seekBar: SeekBar

    private var isFullscreen = false
    private var isControlsShowing = true
    private var isSeeking = false
    private var bufferPercentage = 0
    private var videoUrl = ""
    private var videoTitle = ""

    private val handler = Handler(Looper.getMainLooper())
    private val updateProgressAction = Runnable { updateProgress() }
    private val hideControlsAction = Runnable { hideControls() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_player)

        // 获取传递的视频信息
        videoUrl = intent.getStringExtra("video_url") ?: ""
        videoTitle = intent.getStringExtra("video_title") ?: ""

        initViews()
        initMediaPlayer()
        setupListeners()

        if (videoUrl.isNotEmpty()) {
            prepareMediaPlayer()
        } else {
            showError("无效的视频地址")
        }
    }

    private fun initViews() {
        surfaceView = findViewById(R.id.surfaceView)
        surfaceHolder = surfaceView.holder
        surfaceHolder.addCallback(this)

        topControls = findViewById(R.id.topControls)
        bottomControls = findViewById(R.id.bottomControls)
        progressBar = findViewById(R.id.progressBar)
        tvError = findViewById(R.id.tvError)
        btnRetry = findViewById(R.id.btnRetry)
        btnPlayPause = findViewById(R.id.btnPlayPause)
        btnBack = findViewById(R.id.btnBack)
        btnFullscreen = findViewById(R.id.btnFullscreen)
        tvTitle = findViewById(R.id.tvTitle)
        tvCurrentTime = findViewById(R.id.tvCurrentTime)
        tvTotalTime = findViewById(R.id.tvTotalTime)
        seekBar = findViewById(R.id.seekBar)

        tvTitle.text = videoTitle
        seekBar.setOnSeekBarChangeListener(this)
    }

    private fun initMediaPlayer() {
        mediaPlayer = MediaPlayer().apply {
            setOnPreparedListener(this@PlayerActivity)
            setOnErrorListener(this@PlayerActivity)
            setOnCompletionListener(this@PlayerActivity)
            setOnBufferingUpdateListener(this@PlayerActivity)
            setOnInfoListener(this@PlayerActivity)
        }
    }

    private fun setupListeners() {
        btnBack.setOnClickListener { onBackPressed() }
        btnPlayPause.setOnClickListener { togglePlayPause() }
        btnFullscreen.setOnClickListener { toggleFullscreen() }
        btnRetry.setOnClickListener { prepareMediaPlayer() }
        surfaceView.setOnClickListener { toggleControls() }
    }

    private fun prepareMediaPlayer() {
        hideError()
        showLoading()

        try {
            mediaPlayer.reset()
            mediaPlayer.setDataSource(this, Uri.parse(videoUrl))
            mediaPlayer.setDisplay(surfaceHolder)
            mediaPlayer.prepareAsync()
        } catch (e: IOException) {
            e.printStackTrace()
            showError("播放出错")
        } catch (e: IllegalArgumentException) {
            e.printStackTrace()
            showError("无效的视频地址")
        }
    }

    private fun togglePlayPause() {
        if (mediaPlayer.isPlaying) {
            pause()
        } else {
            start()
        }
    }

    private fun start() {
        mediaPlayer.start()
        btnPlayPause.setImageResource(R.drawable.ic_pause)
        updateTotalTime()
        startProgressTimer()
        startHideControlsTimer()
    }

    private fun pause() {
        mediaPlayer.pause()
        btnPlayPause.setImageResource(R.drawable.ic_play)
        cancelHideControlsTimer()
    }

    private fun toggleFullscreen() {
        if (isFullscreen) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT)
        } else {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE)
        }
    }

    private fun toggleControls() {
        if (isControlsShowing) {
            hideControls()
        } else {
            showControls()
            startHideControlsTimer()
        }
    }

    private fun showControls() {
        topControls.visibility = View.VISIBLE
        bottomControls.visibility = View.VISIBLE
        isControlsShowing = true
    }

    private fun hideControls() {
        if (!isFullscreen) {
            topControls.visibility = View.GONE
        }
        bottomControls.visibility = View.GONE
        isControlsShowing = false
    }

    private fun startHideControlsTimer() {
        cancelHideControlsTimer()
        handler.postDelayed(hideControlsAction, 5000)
    }

    private fun cancelHideControlsTimer() {
        handler.removeCallbacks(hideControlsAction)
    }

    private fun showLoading() {
        progressBar.visibility = View.VISIBLE
    }

    private fun hideLoading() {
        progressBar.visibility = View.GONE
    }

    private fun showError(message: String) {
        tvError.text = message
        tvError.visibility = View.VISIBLE
        btnRetry.visibility = View.VISIBLE
    }

    private fun hideError() {
        tvError.visibility = View.GONE
        btnRetry.visibility = View.GONE
    }

    private fun startProgressTimer() {
        cancelProgressTimer()
        handler.post(updateProgressAction)
    }

    private fun cancelProgressTimer() {
        handler.removeCallbacks(updateProgressAction)
    }

    private fun updateProgress() {
        if (mediaPlayer == null || isSeeking) return

        val currentPosition = mediaPlayer.currentPosition
        val duration = mediaPlayer.duration

        if (duration > 0) {
            seekBar.max = duration
            seekBar.progress = currentPosition
            tvCurrentTime.text = formatTime(currentPosition)
        }

        handler.postDelayed(updateProgressAction, 1000)
    }

    private fun updateTotalTime() {
        val duration = mediaPlayer.duration
        tvTotalTime.text = formatTime(duration)
    }

    private fun formatTime(milliseconds: Int): String {
        val seconds = milliseconds / 1000
        val hours = seconds / 3600
        val minutes = (seconds % 3600) / 60
        val secs = seconds % 60

        return if (hours > 0) {
            String.format(Locale.getDefault(), "%02d:%02d:%02d", hours, minutes, secs)
        } else {
            String.format(Locale.getDefault(), "%02d:%02d", minutes, secs)
        }
    }

    // SurfaceHolder.Callback
    override fun surfaceCreated(holder: SurfaceHolder) {
        prepareMediaPlayer()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        mediaPlayer.setDisplay(holder)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        mediaPlayer.setDisplay(null)
    }

    // MediaPlayer listeners
    override fun onPrepared(mp: MediaPlayer) {
        hideLoading()
        start()
    }

    override fun onError(mp: MediaPlayer, what: Int, extra: Int): Boolean {
        hideLoading()
        showError("播放出错")
        return true
    }

    override fun onCompletion(mp: MediaPlayer) {
        btnPlayPause.setImageResource(R.drawable.ic_play)
        cancelProgressTimer()
    }

    override fun onBufferingUpdate(mp: MediaPlayer, percent: Int) {
        bufferPercentage = percent
        // 如果需要显示缓冲进度，可以在这里更新UI
    }

    override fun onInfo(mp: MediaPlayer, what: Int, extra: Int): Boolean {
        when (what) {
            MediaPlayer.MEDIA_INFO_BUFFERING_START -> showLoading()
            MediaPlayer.MEDIA_INFO_BUFFERING_END -> hideLoading()
            MediaPlayer.MEDIA_INFO_VIDEO_RENDERING_START -> hideLoading()
        }
        return false
    }

    // SeekBar listeners
    override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
        if (fromUser) {
            tvCurrentTime.text = formatTime(progress)
        }
    }

    override fun onStartTrackingTouch(seekBar: SeekBar) {
        isSeeking = true
        cancelHideControlsTimer()
    }

    override fun onStopTrackingTouch(seekBar: SeekBar) {
        isSeeking = false
        mediaPlayer.seekTo(seekBar.progress)
        startHideControlsTimer()
    }

    // Configuration changes
    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        isFullscreen = newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE

        if (isFullscreen) {
            window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
            btnFullscreen.setImageResource(R.drawable.ic_fullscreen_exit)
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
            btnFullscreen.setImageResource(R.drawable.ic_fullscreen)
        }
    }

    override fun onPause() {
        super.onPause()
        if (mediaPlayer.isPlaying) {
            mediaPlayer.pause()
        }
        cancelProgressTimer()
        cancelHideControlsTimer()
    }

    override fun onResume() {
        super.onResume()
        if (!mediaPlayer.isPlaying && mediaPlayer.currentPosition > 0) {
            mediaPlayer.start()
            startProgressTimer()
            startHideControlsTimer()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        mediaPlayer.release()
        cancelProgressTimer()
        cancelHideControlsTimer()
    }

    override fun onBackPressed() {
        if (isFullscreen) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT)
        } else {
            super.onBackPressed()
        }
    }
}