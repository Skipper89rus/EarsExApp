package com.nikitavasilikhin.earsexapp;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.AsyncTask;
import android.os.Environment;
import android.os.Process;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;

class AudioFormatInfo {
    private int[] rates = {44100};
    private int[] channels = {AudioFormat.CHANNEL_IN_STEREO};
    private int[] encodings = {AudioFormat.ENCODING_PCM_16BIT};

    private int mSampleRateInHz;
    private int mChannelConfig;
    private int mEncoding;
    private int mMinBufferSize;

    public AudioFormatInfo() {
        init();
    }

    private void init() {
        for (int enc : encodings) {
            for (int ch : channels) {
                for (int rate : rates) {
                    int bufSize = AudioRecord.getMinBufferSize(rate, ch, enc);
                    if ((bufSize != AudioRecord.ERROR) && (bufSize != AudioRecord.ERROR_BAD_VALUE)) {
                        mSampleRateInHz = rate;
                        mChannelConfig = ch;
                        mEncoding = enc;
                        mMinBufferSize = bufSize;
                    }
                }
            }
        }
    }

    public int getSampleRateInHz() {
        return mSampleRateInHz;
    }

    public int getChannelConfig() {
        return mChannelConfig;
    }

    public int getEncoding() {
        return mEncoding;
    }

    public int getMinBufferSize() {
        return mMinBufferSize;
    }
}

abstract class MicrophoneTaskBase extends AsyncTask<Void, short[], Void> {

    protected AudioFormatInfo mAudioFormat;
    protected AudioRecord mRecord;

    protected final int BUFF_COUNT = 32;

    public MicrophoneTaskBase() {
        mAudioFormat = new AudioFormatInfo();
        mRecord = new AudioRecord(MediaRecorder.AudioSource.MIC,
                mAudioFormat.getSampleRateInHz(),
                mAudioFormat.getChannelConfig(),
                mAudioFormat.getEncoding(),
                mAudioFormat.getMinBufferSize() * 10);
    }

    abstract void preparation();

    @Override
    protected void onPreExecute() {
        super.onPreExecute();

        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);

        if (mRecord.getState() != AudioRecord.STATE_INITIALIZED) {
            System.err.println("AudioRecord initialization error!");
            return;
        }

        mRecord.startRecording();

        preparation();
    }

    @Override
    protected Void doInBackground(Void... params) {
        try {
            int bufIdx = 0;
            while (!isCancelled()) {
                int buffSize = mAudioFormat.getMinBufferSize();
                if (buffSize == AudioRecord.ERROR || buffSize == AudioRecord.ERROR_BAD_VALUE) {
                    System.err.println("MinBufferSize error!");
                    return stop();
                }

                // Работаем с short, поэтому требуем 16-bit
                if (mAudioFormat.getEncoding() != AudioFormat.ENCODING_PCM_16BIT) {
                    System.err.println("Encodin format error!");
                    return stop();
                }

                // Циклический буфер буферов. Чтобы не затереть данные, пока главный поток их обрабатывает
                short[][] buffers = new short[BUFF_COUNT][buffSize >> 1];

                int samplesRead = mRecord.read(buffers[bufIdx], 0, buffers[bufIdx].length);
                if (samplesRead == AudioRecord.ERROR_INVALID_OPERATION || samplesRead == AudioRecord.ERROR_BAD_VALUE) {
                    System.err.println("Audio recording error!");
                    return stop();
                }

                // Посылаем буфер на проигрывание

                publishProgress(buffers[bufIdx]);

                bufIdx = (bufIdx + 1) % BUFF_COUNT;
            }
        } catch (Exception ex) {
            return stop();
        }
        return null;
    }

    abstract void processBuf(short[] buf);

    @Override
    protected void onProgressUpdate(short[]... values) {
        super.onProgressUpdate(values);

        for (short[] buf : values) {
            processBuf(buf);
        }
    }

    abstract Void stop();

    @Override
    protected void onCancelled() {
        super.onCancelled();

        mRecord.stop();
        mRecord.release();
        mRecord = null;

        stop();
    }
}

class MicrophoneRecordSampleTask extends MicrophoneTaskBase {
    private File mSamplesDir;
    private String mSampleFileName;
    private FileOutputStream mSampleFileOutStream;

    static {
        System.loadLibrary("EarsExApp");
    }

    // Signal size = fft size
    private static native float[] Fft(float[] signal);

    public MicrophoneRecordSampleTask(File samplesDir, String sampleFileName) {
        super();

        mSamplesDir = samplesDir;
        mSampleFileName = sampleFileName;
    }

    @Override
    void preparation() {
        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            System.err.println("SD-card not mounted!");
            return;
        }

        if (!mSamplesDir.exists())
            return;

        try {
            File sampleFile = new File(mSamplesDir, mSampleFileName);
            sampleFile.createNewFile();
            mSampleFileOutStream = new FileOutputStream(sampleFile, true);
        } catch (Exception ex) {
            return;
        }
    }

    @Override
    void processBuf(short[] buf) {
        try {
            float[] fBuf = new float[buf.length];
            for (int i = 0; i < buf.length; ++i)
                fBuf[i] = buf[i];
            float[] res = Fft(fBuf);

            ByteBuffer byteBuf = ByteBuffer.allocate(2 * buf.length);
            for (int i = 0; i < buf.length; ++i)
                byteBuf.putShort(buf[i]);

            mSampleFileOutStream.write(byteBuf.array());
        } catch (Exception ex) {
            return;
        }
    }

    @Override
    Void stop() {
        try {
            mSampleFileOutStream.close();
        } catch (Exception ex) {
            return null;
        }
        mSampleFileOutStream = null;

        return null;
    }
}

class MicrophonePlaybackTask extends MicrophoneTaskBase {
    private AudioTrack mTrack;

    public MicrophonePlaybackTask() {
        super();

        mTrack = new AudioTrack(AudioManager.STREAM_MUSIC,
                mAudioFormat.getSampleRateInHz(),
                mAudioFormat.getChannelConfig(),
                mAudioFormat.getEncoding(),
                mAudioFormat.getMinBufferSize() * 10,
                AudioTrack.MODE_STREAM);
    }

    @Override
    void preparation() {
        if (mTrack.getState() != AudioTrack.STATE_INITIALIZED) {
            System.err.println("AudioTrack initialization error!");
            return;
        }

        mTrack.play();
    }

    @Override
    void processBuf(short[] buf) {
        for (int i = 0; i < buf.length; ++i)
            buf[i] *= 20;

        mTrack.write(buf, 0, buf.length);
    }

    @Override
    Void stop() {
        mTrack.stop();
        mTrack.release();
        mTrack = null;

        return null;
    }
}

class PlaySampleTask extends AsyncTask<Void, Void, Void> {
    protected AudioFormatInfo mAudioFormat;
    private AudioTrack mTrack;

    private File mSamplesDir;
    private String mSampleFileName;
    private FileInputStream mSampleFileInStream;

    public PlaySampleTask(File samplesDir, String sampleFileName) {
        super();

        mSamplesDir = samplesDir;
        mSampleFileName = sampleFileName;

        mAudioFormat = new AudioFormatInfo();
        mTrack = new AudioTrack(AudioManager.STREAM_MUSIC,
                mAudioFormat.getSampleRateInHz(),
                mAudioFormat.getChannelConfig(),
                mAudioFormat.getEncoding(),
                mAudioFormat.getMinBufferSize() * 10,
                AudioTrack.MODE_STREAM);
    }

    @Override
    protected void onPreExecute() {
        super.onPreExecute();

        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);

        if (mTrack.getState() != AudioTrack.STATE_INITIALIZED) {
            System.err.println("AudioTrack initialization error!");
            return;
        }
    }

    @Override
    protected Void doInBackground(Void... params) {
        try {
            if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
                System.err.println("SD-card not mounted!");
                return null;
            }

            if (!mSamplesDir.exists())
                return null;

            File sampleFile = new File(mSamplesDir, mSampleFileName);
            mSampleFileInStream = new FileInputStream(sampleFile);
            byte[] buf = new byte[(int)sampleFile.length()];
            mSampleFileInStream.read(buf);

            mTrack.play();
            mTrack.write(buf, 0, buf.length);
        } catch (Exception ex) {
            return null;
        }

        return null;
    }

    @Override
    protected void onCancelled() {
        super.onCancelled();

        mTrack.stop();
        mTrack.release();
        mTrack = null;

        try {
            mSampleFileInStream.close();
        } catch (Exception ex) {
            return;
        }
        mSampleFileInStream = null;
    }
}