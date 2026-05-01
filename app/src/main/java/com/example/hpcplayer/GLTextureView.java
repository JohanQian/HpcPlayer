package com.example.hpcplayer;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.util.AttributeSet;
import android.view.TextureView;
import android.util.Log;

/**
 * GLTextureView
 *
 * 自定义 TextureView，用于与 C++ 层的 SurfaceTexture + OES + EGL 渲染集成
 * 所有 OpenGL ES 渲染在 C++ 层的专用渲染线程中进行
 */
public class GLTextureView extends TextureView implements TextureView.SurfaceTextureListener {
    private static final String TAG = "GLTextureView";

    private SurfaceTexture mSurfaceTexture;
    private OnSurfaceTextureCreatedListener mOnSurfaceTextureCreatedListener;

    public interface OnSurfaceTextureCreatedListener {
        void onSurfaceTextureCreated(SurfaceTexture surfaceTexture);
        void onSurfaceTextureDestroyed();
    }

    public GLTextureView(Context context) {
        super(context);
        init();
    }

    public GLTextureView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public GLTextureView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        // Set this as the surface texture listener
        setSurfaceTextureListener(this);
    }

    public void setOnSurfaceTextureCreatedListener(OnSurfaceTextureCreatedListener listener) {
        mOnSurfaceTextureCreatedListener = listener;
    }

    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture surfaceTexture, int width, int height) {
        Log.d(TAG, "Surface texture available: " + width + "x" + height);
        mSurfaceTexture = surfaceTexture;

        if (mOnSurfaceTextureCreatedListener != null) {
            mOnSurfaceTextureCreatedListener.onSurfaceTextureCreated(surfaceTexture);
        }
    }

    @Override
    public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
        Log.d(TAG, "Surface texture size changed: " + width + "x" + height);
    }

    @Override
    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        Log.d(TAG, "Surface texture destroyed");
        mSurfaceTexture = null;
        if (mOnSurfaceTextureCreatedListener != null) {
            mOnSurfaceTextureCreatedListener.onSurfaceTextureDestroyed();
        }
        return true;
    }

    @Override
    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
        // Called when the specified SurfaceTexture is updated through updateTexImage()
        // Rendering is handled by C++ layer
        // Note: This method is called on Android API 26+
    }

    /**
     * Get the current SurfaceTexture
     */
    public SurfaceTexture getSurfaceTexture() {
        return mSurfaceTexture;
    }
}

