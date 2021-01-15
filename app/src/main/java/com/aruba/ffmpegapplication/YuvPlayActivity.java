package com.aruba.ffmpegapplication;

import android.os.Bundle;
import android.os.Environment;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;

public class YuvPlayActivity extends AppCompatActivity {
    private YuvPlayView playView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_yuv_play);

        playView = findViewById(R.id.playView);
    }

    public void click(View view) {
        File input = new File(Environment.getExternalStorageDirectory(), "input.mp4");
        playView.play(input.getAbsolutePath());
    }
}
