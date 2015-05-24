package com.nikitavasilikhin.earsexapp;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;

import java.io.File;
import java.util.ArrayList;

public class SamplesControlActivity extends Activity {

    public Spinner mSamplesSpinner;
    private File mSamplesDir;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.samples_control_layout);

        mSamplesSpinner = (Spinner) findViewById(R.id.spinner_samples);
    }

    @Override
    protected void onStart() {
        super.onStart();

        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            System.err.println("SD-card not mounted!");
            return;
        }

        mSamplesDir = Environment.getExternalStorageDirectory();
        mSamplesDir = new File(mSamplesDir.getAbsolutePath() + '/'
                + getResources().getString(R.string.app_name) + '/'
                + getResources().getString(R.string.samples_dir_name));
        if (!mSamplesDir.exists())
            return;

        updateSamplesSpinner();
    }

    @Override
    protected void onResume() {
        super.onResume();

        updateSamplesSpinner();
    }

    boolean isSamplesDirAvailable() {
        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            System.err.println("SD-card not mounted!");
            return false;
        }

        return mSamplesDir.exists();
    }

    private void updateSamplesSpinner() {
        ArrayList<String> samplesNames = new ArrayList<>();
        if (isSamplesDirAvailable()) {
            for (File f : mSamplesDir.listFiles())
                samplesNames.add(f.getName());
        }

        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, samplesNames);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mSamplesSpinner.setAdapter(adapter);

        mSamplesSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
                return;
            }
        });
    }

    public void onButtonClick(View v) {
        Intent intent = null;
        switch (v.getId()) {
            case R.id.btn_add:
                intent = new Intent(this, NewSampleActivity.class);
                intent.putExtra("samples_dir", mSamplesDir.toString()); // TODO: mSamplesDir может быть пустым, написать проверку
                break;
            case R.id.btn_delete:
                File sampleFile = new File(mSamplesDir, mSamplesSpinner.getSelectedItem().toString());
                sampleFile.delete();

                updateSamplesSpinner();
                break;
            case R.id.btn_edit:
                intent = new Intent(this, EditSampleActivity.class);
                break;
        }
        if (intent != null)
            startActivity(intent);
    }
}
