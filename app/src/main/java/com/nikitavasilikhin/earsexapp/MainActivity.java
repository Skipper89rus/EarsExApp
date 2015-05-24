package com.nikitavasilikhin.earsexapp;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.widget.Button;
import android.widget.NumberPicker;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;

public class MainActivity extends Activity {

    public NumberPicker m_suppressionRatioNud;

    private File m_samplesDir;

    class NoisePart
    {
        public NoisePart(float freqBeg, float freqEnd)
        {
            FreqBeg = freqBeg;
            FreqEnd = freqEnd;
        }

        public float FreqBeg;
        public float FreqEnd;
    }
    class NoisePrint
    {
        NoisePrint (String name, ArrayList<NoisePart> freqRanges)
        {
            Name = name;
            FreqRanges = freqRanges;
        }

        public String Name;
        public ArrayList<NoisePart> FreqRanges;
    }

    private ArrayList<NoisePrint> m_prints;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main_layout);

        m_suppressionRatioNud = (NumberPicker) findViewById(R.id.suppression_ratio_nud);
        m_suppressionRatioNud.setMinValue(0);
        m_suppressionRatioNud.setMaxValue(5);
        m_suppressionRatioNud.setOnValueChangedListener(suppressionRatioChangedListener);

        String sampleRateString = null, bufferSizeString = null;
        if (Build.VERSION.SDK_INT >= 17) {
            AudioManager audioManager = (AudioManager) this.getSystemService(Context.AUDIO_SERVICE);
            sampleRateString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            bufferSizeString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        }

        if (sampleRateString == null) sampleRateString = "44100";
        if (bufferSizeString == null) bufferSizeString = "512";

        m_sampleRate = Integer.parseInt(sampleRateString);
        m_bufferSize = Integer.parseInt(bufferSizeString);

        System.loadLibrary("AudioProcessor");
    }

    protected void onStart() {
        super.onStart();

        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            System.err.println("SD-card not mounted!");
            return;
        }

        m_samplesDir = Environment.getExternalStorageDirectory();
        m_samplesDir = new File(m_samplesDir.getAbsolutePath() + '/'
                + getResources().getString(R.string.app_name) + '/'
                + getResources().getString(R.string.samples_dir_name));
        if (!m_samplesDir.exists())
            return;

        updateSamples();
    }

    @Override
    protected void onResume() {
        super.onResume();

        updateSamples();
    }

    boolean isSamplesDirAvailable() {
        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            System.err.println("SD-card not mounted!");
            return false;
        }

        return m_samplesDir.exists();
    }

    private void updateSamples() {
        m_prints = new ArrayList<>();

        if (isSamplesDirAvailable()) {
            for (File f : m_samplesDir.listFiles())
            {
                String sampleName = f.getName();

                File sample = new File(m_samplesDir, sampleName);

                try {
                    BufferedReader reader = new BufferedReader( new FileReader(sample) );
                    ArrayList<NoisePart> parts = new ArrayList<>();

                    String line;
                    while ( (line = reader.readLine()) != null )
                    {
                        if ( line.isEmpty() )
                            continue;

                        String[] rangeStrings = line.split("-");
                        if (rangeStrings.length != 2)
                            continue;

                        float beg = Float.parseFloat(rangeStrings[0]);
                        float end = Float.parseFloat(rangeStrings[1]);

                        parts.add( new NoisePart(beg, end) );
                    }
                    reader.close();

                    if (!parts.isEmpty())
                        m_prints.add( new NoisePrint(sampleName, parts) );
                }
                catch (Exception e) {
                    continue;
                }
            }
        }
    }

    private void applyPrintsToSuppress()
    {
        for (NoisePrint print : m_prints)
        {
            for (NoisePart range : print.FreqRanges)
            {
                AddFreqRange(range.FreqBeg, range.FreqEnd);
            }
        }
    }

    NumberPicker.OnValueChangeListener suppressionRatioChangedListener = new NumberPicker.OnValueChangeListener() {
        @Override
        public void onValueChange(NumberPicker picker, int oldVal, int newVal) {
            SetSuppressionRatio( picker.getValue() );
        }
    };

    public void onStartStopBtnClick(View view)
    {
        if (view.getId() == R.id.start_stop_btn)
        {
            Button startStopBtn = (Button) findViewById(R.id.start_stop_btn);

            if (m_isSuppressionStarted) {
                startStopBtn.setText("Start");
                StopSuppression();
            }
            else {
                startStopBtn.setText("Stop");
                applyPrintsToSuppress();
                Suppression(m_sampleRate, m_bufferSize);
            }

            m_isSuppressionStarted = !m_isSuppressionStarted;
        }
    }

    public void onSamplesControlBtn(View v) {
        Intent intent = new Intent(this, SamplesControlActivity.class);
        startActivity(intent);
    }

    private int m_sampleRate;
    private int m_bufferSize;

    private boolean m_isSuppressionStarted;

    // <AudioProcessor>
    private native void Suppression(long sampleRate, long bufferSize);

    private native void StopSuppression();

    private native void SetSuppressionRatio(long suppressionRatio);

    private native void AddFreqRange(float beg, float end);
    // </AudioProcessor>
}