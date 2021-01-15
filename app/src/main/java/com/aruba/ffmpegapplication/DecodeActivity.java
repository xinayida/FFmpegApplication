package com.aruba.ffmpegapplication;

import android.os.Bundle;
import android.os.Environment;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;

public class DecodeActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_decode);
    }

    public native void decodeVideo(String inputFilePath, String outputFilePath);

    public native void decodeAudio(String inputFilePath, String outputFilePath);

    public void click(View view) {
        switch (view.getId()) {
            case R.id.decode_video:
                File input = new File(Environment.getExternalStorageDirectory(), "input.mp4");
                File output = new File(Environment.getExternalStorageDirectory(), "output.yuv");
                decodeVideo(input.getAbsolutePath(), output.getAbsolutePath());
                break;
            case R.id.decode_audio:
                final File input1 = new File(Environment.getExternalStorageDirectory(), "input.mp3");
                final File output1 = new File(Environment.getExternalStorageDirectory(), "output.pcm");
                new Thread() {
                    @Override
                    public void run() {
                        decodeAudio(input1.getAbsolutePath(), output1.getAbsolutePath());
                    }
                }.start();
                break;
        }

    }
}
