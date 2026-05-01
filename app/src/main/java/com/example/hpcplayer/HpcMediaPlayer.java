package com.example.hpcplayer;

import android.os.Handler;
import android.os.Looper;
import android.graphics.SurfaceTexture;
import android.view.Surface;

public class HpcMediaPlayer {

    static {
        try {
            System.loadLibrary("hpcplayer-native");
        } catch (UnsatisfiedLinkError e) {
            e.printStackTrace();
        }
    }

    public interface OnMessageListener {
        void onMessage(int msg, int ext1, int ext2);
    }

    private long nativePlayerPtr;
    private OnMessageListener listener;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private Surface currentSurface;
    private boolean tunnelMode;

    public HpcMediaPlayer() {
        try {
            nativePlayerPtr = nativeInit();
            if (nativePlayerPtr != 0) {
                nativeSetupListener(nativePlayerPtr);
            }
        } catch (UnsatisfiedLinkError e) {
            e.printStackTrace();
        }
    }

    // Native methods
    private native long nativeInit();
    private native void nativeSetupListener(long playerPtr);
    private native void nativeRelease(long playerPtr);
    private native void nativeSetDataSource(long playerPtr, String path);
    private native void nativeSetSurface(long playerPtr, Surface surfaceObj, boolean tunnelMode);
    private native void nativePrepare(long playerPtr);
    private native void nativeStart(long playerPtr);
    private native void nativeResume(long playerPtr);
    private native void nativePause(long playerPtr);
    private native void nativeStop(long playerPtr);
    private native void nativeSeekTo(long playerPtr, long msec);
    private native long nativeGetDuration(long playerPtr);
    private native long nativeGetCurrentPosition(long playerPtr);

    public void setOnMessageListener(OnMessageListener listener) {
        this.listener = listener;
    }

    public void setDataSource(String path) {
        if (nativePlayerPtr != 0) {
            try {
                nativeSetDataSource(nativePlayerPtr, path);
            } catch (UnsatisfiedLinkError e) { e.printStackTrace(); }
        }
    }

    public void setSurface(Surface surface) {
        setSurface(surface, tunnelMode);
    }

    public void setSurface(Surface surface, boolean tunnelMode) {
        currentSurface = surface;
        this.tunnelMode = tunnelMode;
        if (nativePlayerPtr != 0) {
            try {
                nativeSetSurface(nativePlayerPtr, surface, tunnelMode);
            } catch (UnsatisfiedLinkError e) { e.printStackTrace(); }
        }
    }

    public void setTunnelMode(boolean enabled) {
        setSurface(currentSurface, enabled);
    }

    public boolean isTunnelMode() {
        return tunnelMode;
    }

    public void prepare() {
        if (nativePlayerPtr != 0) {
            try {
                nativePrepare(nativePlayerPtr);
            } catch (UnsatisfiedLinkError e) { e.printStackTrace(); }
        }
    }

    public void start() {
        if (nativePlayerPtr != 0) {
            try {
                nativeStart(nativePlayerPtr);
            } catch (UnsatisfiedLinkError e) { e.printStackTrace(); }
        }
    }

    public void resume() {
        if (nativePlayerPtr != 0) {
            try {
                nativeResume(nativePlayerPtr);
            } catch (UnsatisfiedLinkError e) { e.printStackTrace(); }
        }
    }

    public void pause() {
        if (nativePlayerPtr != 0) {
            try {
                nativePause(nativePlayerPtr);
            } catch (UnsatisfiedLinkError e) { e.printStackTrace(); }
        }
    }

    public void stop() {
        if (nativePlayerPtr != 0) {
            try {
                nativeStop(nativePlayerPtr);
            } catch (UnsatisfiedLinkError e) { e.printStackTrace(); }
        }
    }

    public void seekTo(long msec) {
        if (nativePlayerPtr != 0) {
            try {
                nativeSeekTo(nativePlayerPtr, msec);
            } catch (UnsatisfiedLinkError e) { e.printStackTrace(); }
        }
    }

    public long getDuration() {
        if (nativePlayerPtr != 0) {
            try {
                return nativeGetDuration(nativePlayerPtr);
            } catch (UnsatisfiedLinkError e) {
                return 0;
            }
        }
        return 0;
    }

    public long getCurrentPosition() {
        if (nativePlayerPtr != 0) {
            try {
                return nativeGetCurrentPosition(nativePlayerPtr);
            } catch (UnsatisfiedLinkError e) {
                return 0;
            }
        }
        return 0;
    }

    public void release() {
        if (nativePlayerPtr != 0) {
            try {
                nativeRelease(nativePlayerPtr);
            } catch (UnsatisfiedLinkError e) { e.printStackTrace(); }
            nativePlayerPtr = 0;
            currentSurface = null;
        }
    }

    // This method is called by JNI from a C++ thread
    private void onMessage(int msg, int ext1, int ext2) {
        if (listener != null) {
            // Post to the main thread to ensure UI safety
            mainHandler.post(() -> listener.onMessage(msg, ext1, ext2));
        }
    }

    private static SurfaceTexture createSurfaceTexture(int textureId) {
        return new SurfaceTexture(textureId);
    }

    private static Surface createSurface(SurfaceTexture surfaceTexture) {
        return new Surface(surfaceTexture);
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            release();
        } finally {
            super.finalize();
        }
    }
}
