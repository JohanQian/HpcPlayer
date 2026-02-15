package com.example.hpcplayer;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.ImageButton;
import android.widget.RelativeLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.util.Locale;

public class MainActivity extends Activity implements SurfaceHolder.Callback {

    private static final int REQUEST_PERMISSIONS = 1;

    // Media Event Types (Must match native definition)
    private static final int MEDIA_PREPARED = 1;
    private static final int MEDIA_PLAYBACK_COMPLETE = 2;

    private HpcNativePlayer hpcNativePlayer;
    private SurfaceView surfaceView;
    private SurfaceHolder surfaceHolder;
    
    // UI Components
    private RelativeLayout controlPanel;
    private ImageButton centerPlayButton;
    private ImageButton playPauseButton;
    private ImageButton prevButton;
    private ImageButton nextButton;
    private SeekBar seekBar;
    private TextView currentTimeText;
    private TextView totalTimeText;
    private TextView videoTitleText;

    private String[] videoPaths = {"/sdcard/VideoEditor/1080_1VNNOX.mp4","/sdcard/VideoEditor/1080_1VNNOX.mp4"};
    private int currentVideoIndex = 0;
    private boolean isSurfaceReady = false;
    private boolean isPlaying = false;
    private boolean isTracking = false;
    private boolean isVideoLoaded = false;
    
    private Handler uiHandler = new Handler(Looper.getMainLooper());
    private Runnable updateProgressRunnable = new Runnable() {
        @Override
        public void run() {
            if (hpcNativePlayer != null && isPlaying && !isTracking) {
                long current = hpcNativePlayer.getCurrentPosition();
                long total = hpcNativePlayer.getDuration();
                
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
        if (hpcNativePlayer != null) {
            hpcNativePlayer.release();
        }
    }

    private void initViews() {
        surfaceView = findViewById(R.id.surfaceView);
        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(this);

        controlPanel = findViewById(R.id.controlPanel);
        centerPlayButton = findViewById(R.id.centerPlayButton);
        playPauseButton = findViewById(R.id.playPauseButton);
        prevButton = findViewById(R.id.prevButton);
        nextButton = findViewById(R.id.nextButton);
        seekBar = findViewById(R.id.seekBar);
        currentTimeText = findViewById(R.id.currentTime);
        totalTimeText = findViewById(R.id.totalTime);
        videoTitleText = findViewById(R.id.videoTitle);

        // Toggle control panel visibility on surface click
        surfaceView.setOnClickListener(v -> {
            if (controlPanel.getVisibility() == View.VISIBLE) {
                controlPanel.setVisibility(View.GONE);
            } else {
                controlPanel.setVisibility(View.VISIBLE);
            }
        });

        // Center Play Button
        centerPlayButton.setOnClickListener(v -> togglePlayPause());

        // Bottom Play/Pause Button
        playPauseButton.setOnClickListener(v -> togglePlayPause());

        // Next Button
        nextButton.setOnClickListener(v -> {
            if (hasPermissions()) {
                currentVideoIndex = (currentVideoIndex + 1) % videoPaths.length;
                playVideo();
            }
        });

        // Prev Button
        prevButton.setOnClickListener(v -> {
            if (hasPermissions()) {
                currentVideoIndex = (currentVideoIndex - 1 + videoPaths.length) % videoPaths.length;
                playVideo();
            }
        });

        // SeekBar
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    long total = hpcNativePlayer.getDuration();
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
                if (hpcNativePlayer != null) {
                    long total = hpcNativePlayer.getDuration();
                    long target = total * seekBar.getProgress() / 100;
                    hpcNativePlayer.seekTo(target);
                    // Update UI immediately after seek
                    currentTimeText.setText(formatTime(target));
                }
            }
        });
    }

    private void initPlayer() {
        hpcNativePlayer = new HpcNativePlayer();
        hpcNativePlayer.setOnMessageListener((msg, ext1, ext2) -> {
            // Handle native messages
            switch (msg) {
                case MEDIA_PREPARED:
                    // Logic 1: MEDIA_PREPARED以后在开始start播放视频
                    if (hpcNativePlayer != null) {
                        hpcNativePlayer.start();
                        isPlaying = true;
                        updatePlayPauseIcon();
                        uiHandler.post(updateProgressRunnable);
                    }
                    break;
                case MEDIA_PLAYBACK_COMPLETE:
                    // Logic 2: MEDIA_PLAYBACK_COMPLETE后 播放下一个视频
                    if (hasPermissions()) {
                        currentVideoIndex = (currentVideoIndex + 1) % videoPaths.length;
                        playVideo();
                    }
                    break;
            }
        });
    }

    private void togglePlayPause() {
        if (hpcNativePlayer == null) return;

        if (isPlaying) {
            hpcNativePlayer.pause();
            isPlaying = false;
            updatePlayPauseIcon();
            uiHandler.removeCallbacks(updateProgressRunnable);
        } else {
            if (!isSurfaceReady) return;
            
            if (!isVideoLoaded) {
                playVideo();
            } else {
                hpcNativePlayer.start();
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

        
        // Reset UI
        seekBar.setProgress(0);
        currentTimeText.setText("00:00");
        totalTimeText.setText("00:00");
        videoTitleText.setText(new File(videoPaths[currentVideoIndex]).getName());

        // Setup and start new video
        hpcNativePlayer.setDataSource(videoPaths[currentVideoIndex]);
        hpcNativePlayer.setSurface(surfaceHolder.getSurface());
        hpcNativePlayer.prepare();
        
        isVideoLoaded = true;
        // isPlaying will be set to true in MEDIA_PREPARED handler
        isPlaying = false; 
        updatePlayPauseIcon();
        uiHandler.removeCallbacks(updateProgressRunnable);
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
    public void surfaceCreated(SurfaceHolder holder) {
        surfaceHolder = holder;
        isSurfaceReady = true;
        // Auto-play first video if permissions granted
        if (hasPermissions()) {
             playVideo();
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        isSurfaceReady = false;
        isPlaying = false;
        isVideoLoaded = false;
        uiHandler.removeCallbacks(updateProgressRunnable);
        if (hpcNativePlayer != null) {
            hpcNativePlayer.release();
            hpcNativePlayer = null;
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
