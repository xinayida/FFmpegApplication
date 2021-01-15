package com.aruba.ffmpegapplication;

import android.content.Context;
import android.graphics.PixelFormat;
import android.util.AttributeSet;
import android.view.Surface;
import android.view.SurfaceView;

import java.lang.ref.WeakReference;

/**
 * Created by aruba on 2020/6/30.
 */
public class YuvPlayView extends SurfaceView {
    static {
        System.loadLibrary("native-lib");
    }

    private PlayThread playThread;

    public YuvPlayView(Context context) {
        this(context, null);
    }

    public YuvPlayView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public YuvPlayView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        setKeepScreenOn(true);
        //设置一个像素占4字节
        getHolder().setFormat(PixelFormat.RGBA_8888);

        playThread = new PlayThread(this);
    }

    /**
     * 开始播放
     */
    public void play(String filePath) {
        if (isPlaying()) {
            playThread.stopPlay();
        }

        playThread = new PlayThread(this);
        playThread.filePath = filePath;
        playThread.start();
    }

    public void stop() {
        if (isPlaying()) {
            playThread.stopPlay();
        }
    }

    public boolean isPlaying() {
        return playThread.isPlaying();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        stop();
    }

    static class PlayThread extends Thread {
        public boolean isPlaying;
        public String filePath;
        private WeakReference<YuvPlayView> playViewWeakReference;

        public PlayThread(YuvPlayView yuvPlayView) {
            this.playViewWeakReference = new WeakReference<>(yuvPlayView);
        }

        @Override
        public void run() {
            isPlaying = true;
            //阻塞方法，不断将视频内容绘制到屏幕
            if (playViewWeakReference.get() != null) {
                int ret = render(filePath, playViewWeakReference.get().getHolder().getSurface());
            }
            isPlaying = false;
        }

        public boolean isPlaying() {
            return isPlaying;
        }

        public native int render(String filePath, Surface surface);

        public native void stopPlay();
    }

}
