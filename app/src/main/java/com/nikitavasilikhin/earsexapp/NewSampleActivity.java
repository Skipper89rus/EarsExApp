package com.nikitavasilikhin.earsexapp;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.widget.EditText;

import java.io.File;
import java.util.Arrays;
import java.util.List;

public class NewSampleActivity extends Activity {

    private EditText mSampleName;
    private File mSamplesDir;

    private MicrophoneRecordSampleTask mMicRecordSampleTask;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.new_sample_layout);

        mSampleName = (EditText) findViewById(R.id.editTxt_sample_name);
    }

    @Override
    protected void onStart() {
        super.onStart();

        mSamplesDir = new File(getIntent().getStringExtra("samples_dir"));
        mSampleName.setText(getFreeSampleName());
    }

    boolean isSamplesDirAvailable() {
        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            System.err.println("SD-card not mounted!");
            return false;
        }

        return mSamplesDir.exists();
    }

    boolean isSampleNameFree(String name) {
        if (name == null)
            return false;

        if (!isSamplesDirAvailable())
            return false;

        List<String> files = Arrays.asList(mSamplesDir.list());
        return !files.contains(name);
    }

    String getFreeSampleName() {
        if (!isSamplesDirAvailable())
            return getString(R.string.def_sample_name);

        int idx = 1;
        String name = String.format("Sample %d", idx);

        List<String> files = Arrays.asList(mSamplesDir.list());
        while (files.contains(name))
            name = String.format("Sample %d", ++idx);

        return name;
    }

    public void onButtonClick(View v) {
        switch (v.getId()) {
            case R.id.btn_record:
                if (!isSampleNameFree(mSampleName.getText().toString()))
                    return;
                // TODO: enable/disable btns
                mMicRecordSampleTask = new MicrophoneRecordSampleTask(mSamplesDir, mSampleName.getText().toString());
                mMicRecordSampleTask.execute();
                break;
            case R.id.btn_stop:
                if (mMicRecordSampleTask != null) {
                    mMicRecordSampleTask.cancel(false);
                    mMicRecordSampleTask = null;
                }
                break;
        }
    }
}