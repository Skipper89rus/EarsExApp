package com.nikitavasilikhin.earsexapp;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.RadioGroup;
import android.widget.Spinner;

import java.io.File;
import java.util.ArrayList;

public class MainActivity extends Activity {

    public Button mStartBtn;
    public Button mStopBtn;

    public MicrophonePlaybackTask mMicPlaybackTask;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main_layout);

        mStartBtn = (Button) findViewById(R.id.btn_start);
        mStopBtn = (Button) findViewById(R.id.btn_stop);
    }

    public void onButtonClick(View v) {
        switch (v.getId()) {
            case R.id.btn_start:
                mStartBtn.setEnabled(false); // TODO: нужен рефакт
                mStopBtn.setEnabled(true);

                mMicPlaybackTask = new MicrophonePlaybackTask();
                mMicPlaybackTask.execute();
                break;
            case R.id.btn_stop:
                mStartBtn.setEnabled(true);
                mStopBtn.setEnabled(false);

                if (mMicPlaybackTask != null)
                    mMicPlaybackTask.cancel(false);
                break;
        }
    }

    public void onSamplesControlBtn(View v) {
        Intent intent = new Intent(this, SamplesControlActivity.class);
        startActivity(intent);
    }
}