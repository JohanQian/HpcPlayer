package com.example.hpcplayer;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.View;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.util.Locale;

public class MainActivity extends Activity implements GLTextureView.OnSurfaceTextureCreatedListener {

    private static final int REQUEST_PERMISSIONS = 1;
    private static final int SEEK_STEP_MS = 10000; // 10 seconds

    // Media Event Types (Must match native definition)
    private static final int MEDIA_PREPARED = 1;
    private static final int MEDIA_PLAYBACK_COMPLETE = 2;

    private HpcMediaPlayer hpcMediaPlayer;
    private GLTextureView textureView;
    private SurfaceTexture surfaceTexture;
    private Surface displaySurface;

    // UI Components
    private RelativeLayout controlPanel;
    private ImageButton centerPlayButton;
    private ImageButton playPauseButton;
    private ImageButton prevButton;
    private ImageButton nextButton;
    private ImageButton rewindButton;
    private ImageButton fastForwardButton;
    private ImageButton volumeButton;
    private ImageButton fullscreenButton;
    private ImageButton settingsButton;
    private ImageButton playlistButton;
    private SeekBar seekBar;
    private SeekBar volumeSeekBar;
    private LinearLayout volumeLayout;
    private TextView currentTimeText;
    private TextView totalTimeText;
    private TextView videoTitleText;

    private String[] videoPaths = {
        Environment.getExternalStorageDirectory().getPath() + "/VideoEditor/4K.mp4",
        Environment.getExternalStorageDirectory().getPath() + "/VideoEditor/1080_1VNNOX.mp4"
    };
    private int currentVideoIndex = 0;
    private boolean isSurfaceReady = false;
    private boolean isPlaying = false;
    private boolean isTracking = false;
    private boolean isVideoLoaded = false;
    private boolean isFullscreen = false;

    private Handler uiHandler = new Handler(Looper.getMainLooper());
    private Runnable updateProgressRunnable = new Runnable() {
        @Override
        public void run() {
            if (hpcMediaPlayer != null && isPlaying && !isTracking) {
                long current = hpcMediaPlayer.getCurrentPosition();
                long total = hpcMediaPlayer.getDuration();
                
                if (total > 0) {
                    int progress = (int) (current * 100 / total);
                    seekBar.setProgress(progress);
                    currentTimeText.setText(formatTime(current));
                    totalTimeText.setText(formatTime(total));
                }
            }
            if (isPlaying) {
                uiHandler.postDelayed(this, 1000);
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_video_player);

        initViews();
        initPlayer();
        
        if (!hasPermissions()) {
            requestPermissions();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (hpcMediaPlayer != null) {
            hpcMediaPlayer.stop();
            hpcMediaPlayer.release();
            hpcMediaPlayer = null;
        }
        if (displaySurface != null) {
            displaySurface.release();
            displaySurface = null;
        }
        uiHandler.removeCallbacksAndMessages(null);
    }

    private void initViews() {
        textureView = findViewById(R.id.textureView);
        textureView.setOnSurfaceTextureCreatedListener(this);

        controlPanel = findViewById(R.id.controlPanel);
        centerPlayButton = findViewById(R.id.centerPlayButton);
        playPauseButton = findViewById(R.id.playPauseButton);
        prevButton = findViewById(R.id.prevButton);
        nextButton = findViewById(R.id.nextButton);
        rewindButton = findViewById(R.id.rewindButton);
        fastForwardButton = findViewById(R.id.fastForwardButton);
        volumeButton = findViewById(R.id.volumeButton);
        fullscreenButton = findViewById(R.id.fullscreenButton);
        settingsButton = findViewById(R.id.settingsButton);
        playlistButton = findViewById(R.id.playlistButton);
        seekBar = findViewById(R.id.seekBar);
        volumeSeekBar = findViewById(R.id.volumeSeekBar);
        volumeLayout = findViewById(R.id.volumeLayout);
        currentTimeText = findViewById(R.id.currentTime);
        totalTimeText = findViewById(R.id.totalTime);
        videoTitleText = findViewById(R.id.videoTitle);

        // Toggle control panel visibility on texture view click
        textureView.setOnClickListener(v -> toggleControlsVisibility());

        // Center Play Button
        centerPlayButton.setOnClickListener(v -> togglePlayPause());

        // Bottom Play/Pause Button
        playPauseButton.setOnClickListener(v -> togglePlayPause());

        // Next Button
        nextButton.setOnClickListener(v -> {
            if (hasPermissions()) {
                playNextVideo();
            }
        });

        // Prev Button
        prevButton.setOnClickListener(v -> {
            if (hasPermissions()) {
                playPreviousVideo();
            }
        });

        // Rewind Button
        rewindButton.setOnClickListener(v -> seekBackward());

        // Fast Forward Button
        fastForwardButton.setOnClickListener(v -> seekForward());

        // Volume Button
        volumeButton.setOnClickListener(v -> toggleVolumeControl());

        // Fullscreen Button
        fullscreenButton.setOnClickListener(v -> toggleFullscreen());

        // Settings Button
        settingsButton.setOnClickListener(v -> showNotImplemented("Settings"));

        // Playlist Button
        playlistButton.setOnClickListener(v -> showNotImplemented("Playlist"));

        // SeekBar
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    long total = hpcMediaPlayer != null ? hpcMediaPlayer.getDuration() : 0;
                    long target = total * progress / 100;
                    currentTimeText.setText(formatTime(target));
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                isTracking = true;
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                isTracking = false;
                if (hpcMediaPlayer != null) {
                    long total = hpcMediaPlayer.getDuration();
                    long target = total * seekBar.getProgress() / 100;
                    hpcMediaPlayer.seekTo(target);
                    // Update UI immediately after seek
                    currentTimeText.setText(formatTime(target));
                }
            }
        });

        // Volume SeekBar
        volumeSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser && hpcMediaPlayer != null) {
                    // Assuming HpcMediaPlayer has setVolume method, otherwise show not implemented
                    // hpcMediaPlayer.setVolume(progress / 100.0f);
                    showNotImplemented("Volume Control");
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
    }

    private void initPlayer() {
        hpcMediaPlayer = new HpcMediaPlayer();
        hpcMediaPlayer.setOnMessageListener((msg, ext1, ext2) -> {
            // Handle native messages
            switch (msg) {
                case MEDIA_PREPARED:
                    // Logic 1: MEDIA_PREPARED以后在开始start播放视频
                    if (hpcMediaPlayer != null) {
                        isVideoLoaded = true;
                        hpcMediaPlayer.start();
                        isPlaying = true;
                        updatePlayPauseIcon();
                        uiHandler.post(updateProgressRunnable);
                    }
                    break;
                case MEDIA_PLAYBACK_COMPLETE:
                    // Logic 2: MEDIA_PLAYBACK_COMPLETE后 播放下一个视频
                    if (hasPermissions()) {
                        playNextVideo();
                    }
                    break;
            }
        });
    }

    private void togglePlayPause() {
        if (hpcMediaPlayer == null) return;

        if (isPlaying) {
            hpcMediaPlayer.pause();
            isPlaying = false;
            updatePlayPauseIcon();
            uiHandler.removeCallbacks(updateProgressRunnable);
        } else {
            if (!isSurfaceReady) return;
            
            if (!isVideoLoaded) {
                playVideo();
            } else {
                hpcMediaPlayer.start();
                isPlaying = true;
                updatePlayPauseIcon();
                uiHandler.post(updateProgressRunnable);
            }
        }
    }

    private void updatePlayPauseIcon() {
        int iconRes = isPlaying ? android.R.drawable.ic_media_pause : android.R.drawable.ic_media_play;
        playPauseButton.setImageResource(iconRes);
        centerPlayButton.setImageResource(iconRes);
        centerPlayButton.setVisibility(isPlaying ? View.GONE : View.VISIBLE);
    }

    private void playVideo() {
        if (!isSurfaceReady || !hasPermissions()) {
            return;
        }

        uiHandler.removeCallbacks(updateProgressRunnable);
        isPlaying = false;
        isVideoLoaded = false;
        
        // Stop and release the existing player instance, then re-initialize
        if (hpcMediaPlayer != null) {
            hpcMediaPlayer.stop();
            hpcMediaPlayer.setOnMessageListener(null);
            hpcMediaPlayer.release();
            hpcMediaPlayer = null;
        }
        initPlayer();
        hpcMediaPlayer.setSurface(displaySurface);
        
        // Reset UI
        seekBar.setProgress(0);
        currentTimeText.setText("00:00");
        totalTimeText.setText("00:00");
        videoTitleText.setText(new File(videoPaths[currentVideoIndex]).getName());
        updatePlayPauseIcon();

        // Setup and start new video
        hpcMediaPlayer.setDataSource(videoPaths[currentVideoIndex]);
        hpcMediaPlayer.prepare();
    }

    private void playNextVideo() {
        currentVideoIndex = (currentVideoIndex + 1) % videoPaths.length;
        playVideo();
    }

    private void playPreviousVideo() {
        currentVideoIndex = (currentVideoIndex - 1 + videoPaths.length) % videoPaths.length;
        playVideo();
    }

    private void seekBackward() {
        if (hpcMediaPlayer != null) {
            long current = hpcMediaPlayer.getCurrentPosition();
            long target = Math.max(0, current - SEEK_STEP_MS);
            hpcMediaPlayer.seekTo(target);
            currentTimeText.setText(formatTime(target));
        }
    }

    private void seekForward() {
        if (hpcMediaPlayer != null) {
            long current = hpcMediaPlayer.getCurrentPosition();
            long total = hpcMediaPlayer.getDuration();
            long target = Math.min(total, current + SEEK_STEP_MS);
            hpcMediaPlayer.seekTo(target);
            currentTimeText.setText(formatTime(target));
        }
    }

    private void toggleVolumeControl() {
        volumeLayout.setVisibility(volumeLayout.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
    }

    private void toggleFullscreen() {
        if (isFullscreen) {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            isFullscreen = false;
            fullscreenButton.setImageResource(android.R.drawable.ic_menu_crop);
        } else {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            isFullscreen = true;
            fullscreenButton.setImageResource(android.R.drawable.ic_menu_close_clear_cancel);
        }
    }

    private void toggleControlsVisibility() {
        if (controlPanel.getVisibility() == View.VISIBLE) {
            controlPanel.setVisibility(View.GONE);
        } else {
            controlPanel.setVisibility(View.VISIBLE);
        }
    }

    private void showNotImplemented(String feature) {
        Toast.makeText(this, feature + " feature not implemented yet", Toast.LENGTH_SHORT).show();
    }

    private String formatTime(long millis) {
        long seconds = millis / 1000;
        long minutes = seconds / 60;
        long remainingSeconds = seconds % 60;
        return String.format(Locale.getDefault(), "%02d:%02d", minutes, remainingSeconds);
    }

    private boolean hasPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            return checkSelfPermission(Manifest.permission.READ_MEDIA_VIDEO) == PackageManager.PERMISSION_GRANTED;
        } else {
            return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
        }
    }

    private void requestPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            requestPermissions(new String[]{Manifest.permission.READ_MEDIA_VIDEO}, REQUEST_PERMISSIONS);
        } else {
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, REQUEST_PERMISSIONS);
        }
    }

    @Override
    public void onSurfaceTextureCreated(SurfaceTexture surfaceTexture) {
        // Called when GLTextureView's SurfaceTexture is available
        isSurfaceReady = true;
        this.surfaceTexture = surfaceTexture;
        if (displaySurface != null) {
            displaySurface.release();
        }
        displaySurface = new Surface(surfaceTexture);

        // Auto-play first video if permissions granted
        if (hasPermissions() && !isVideoLoaded) {
            playVideo();
        }
    }

    @Override
    public void onSurfaceTextureDestroyed() {
        isSurfaceReady = false;
        surfaceTexture = null;
        if (hpcMediaPlayer != null) {
            hpcMediaPlayer.setSurface(null);
        }
        if (displaySurface != null) {
            displaySurface.release();
            displaySurface = null;
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == REQUEST_PERMISSIONS) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                if (isSurfaceReady) {
                    playVideo();
                }
            } else {
                Toast.makeText(this, "Permission Denied. Cannot play video.", Toast.LENGTH_LONG).show();
            }
        }
    }
}
