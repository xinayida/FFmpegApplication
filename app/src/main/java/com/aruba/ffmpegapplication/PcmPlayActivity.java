package com.aruba.ffmpegapplication;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;

public class PcmPlayActivity extends AppCompatActivity {
    static {
        System.loadLibrary("native-lib");
    }

    private AudioTrack audioTrack;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_pcm_play);
    }

    /**
     * 给native层回调
     *
     * @param sampleRateInHz
     * @param channelCount
     */
    private void create(int sampleRateInHz, int channelCount) {
        int channelConfig = AudioFormat.CHANNEL_OUT_MONO; //单声道
        if (channelCount == 2) {
            channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        }

        int buffSize = AudioTrack.getMinBufferSize(sampleRateInHz, channelConfig, AudioFormat.ENCODING_PCM_16BIT);//计算最小缓冲区

//    @Deprecated
//    public AudioTrack(int streamType, int sampleRateInHz, int channelConfig, int audioFormat, int bufferSizeInBytes, int mode) throws IllegalArgumentException {
//            throw new RuntimeException("Stub!");
//        }
        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRateInHz, channelConfig, AudioFormat.ENCODING_PCM_16BIT, buffSize, AudioTrack.MODE_STREAM);
        audioTrack.play();
    }

    /**
     * 给native层回调
     */
    private void play(byte[] bytes, int size) {
        if (audioTrack != null && audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING)
            audioTrack.write(bytes, 0, size);
    }

    public void click(View view) {
        switch (view.getId()) {
            case R.id.btn_audiotrack:
                final File input1 = new File(Environment.getExternalStorageDirectory(), "input.mp3");

                new Thread() {
                    @Override
                    public void run() {
                        playByAudio(input1.getAbsolutePath());
                    }
                }.start();
                break;
            case R.id.btn_opensl:
                final File input2 = new File(Environment.getExternalStorageDirectory(), "input.mp3");

                new Thread() {
                    @Override
                    public void run() {
                        playByOpenSL("http://ivi.bupt.edu.cn/hls/cctv1hd.m3u8");
                    }
                }.start();
                break;
            case R.id.btn_opensl_stop:
                stopByOpenSL();
                break;
        }

    }

    private native void playByAudio(String inputFilePath);

    private native void playByOpenSL(String inputFilePath);

    private native void stopByOpenSL();
}
