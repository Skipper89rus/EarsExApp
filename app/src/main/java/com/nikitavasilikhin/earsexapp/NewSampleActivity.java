package com.nikitavasilikhin.earsexapp;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.util.Arrays;
import java.util.List;

public class NewSampleActivity extends Activity {

    private EditText m_sampleName;
    private EditText m_freqRanges;
    private File m_samplesDir;

    private boolean m_isRecStarted;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.new_sample_layout);

        m_sampleName = (EditText) findViewById(R.id.sample_name_txt);
        m_freqRanges = (EditText) findViewById(R.id.freq_ranges_txt);
    }

    @Override
    protected void onStart() {
        super.onStart();

        m_samplesDir = new File(getIntent().getStringExtra("samples_dir"));
        m_sampleName.setText(getFreeSampleName());
    }

    boolean isSamplesDirAvailable() {
        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            System.err.println("SD-card not mounted!");
            return false;
        }

        return m_samplesDir.exists();
    }

    boolean isSampleNameFree(String name) {
        if (name == null)
            return false;

        if (!isSamplesDirAvailable())
            return false;

        List<String> files = Arrays.asList(m_samplesDir.list());
        return !files.contains(name);
    }

    String getFreeSampleName() {
        if (!isSamplesDirAvailable())
            return getString(R.string.def_sample_name);

        int idx = 1;
        String name = String.format("Sample %d", idx);

        List<String> files = Arrays.asList(m_samplesDir.list());
        while (files.contains(name))
            name = String.format("Sample %d", ++idx);

        return name;
    }

    public void onRecStopBtnClick(View v)
    {
        if (v.getId() == R.id.record_stop_btn)
        {
            Button startStopBtn = (Button) findViewById(R.id.record_stop_btn);

            if (m_isRecStarted) {
                startStopBtn.setText("Rec");

                m_freqRanges.setVisibility(View.VISIBLE);
            }
            else {
                startStopBtn.setText("Stop");

                m_freqRanges.setVisibility(View.INVISIBLE);
            }

            m_isRecStarted = !m_isRecStarted;
        }
    }

    public void onSaveBtnClick(View v)
    {
        if ( !isSampleNameFree(m_sampleName.getText().toString()) )
            return;

        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            System.err.println("SD-card not mounted!");
            return;
        }

        if (!m_samplesDir.exists())
            return;

        try
        {
            File sampleFile = new File(m_samplesDir, m_sampleName.getText().toString());
            sampleFile.createNewFile();

            FileOutputStream outStream = new FileOutputStream(sampleFile, true);
            outStream.write(m_freqRanges.getText().toString().getBytes());
            outStream.close();
        }
        catch (Exception ex)
        {
            return;
        }
    }
}