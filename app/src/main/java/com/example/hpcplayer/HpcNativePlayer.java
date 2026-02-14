package com.example.hpcplayer;

import android.os.Handler;
import android.os.Looper;
import android.view.Surface;

public class HpcNativePlayer {

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

    public HpcNativePlayer() {
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
    private native void nativeSetSurface(long playerPtr, Surface surface);
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
        if (nativePlayerPtr != 0) {
            try {
                nativeSetSurface(nativePlayerPtr, surface);
            } catch (UnsatisfiedLinkError e) { e.printStackTrace(); }
        }
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
        }
    }

    // This method is called by JNI from a C++ thread
    private void onMessage(int msg, int ext1, int ext2) {
        if (listener != null) {
            // Post to the main thread to ensure UI safety
            mainHandler.post(() -> listener.onMessage(msg, ext1, ext2));
        }
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
