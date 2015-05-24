#include <jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "SuperpoweredFrequencyDomain.h"
#include "SuperpoweredAndroidAudioIO.h"
#include "SuperpoweredSimple.h"

#include <vector>
#include <algorithm>

struct NoisePart
{
   NoisePart (const float freqBeg, const float freqEnd)
      : FreqBeg(freqBeg)
      , FreqEnd(freqEnd)
   {}

   NoisePart (const float freqBeg, const float freqEnd, const float timeBeg, const float timeEnd)
         : FreqBeg(freqBeg)
         , FreqEnd(freqEnd)
         , TimeBeg(timeBeg)
         , TimeEnd(timeEnd)
      {}

   float FreqBeg;
   float FreqEnd;

   float TimeBeg;
   float TimeEnd;
};

// <Global>
static SuperpoweredAndroidAudioIO* AudioIO;
static SuperpoweredFrequencyDomain* FrequencyDomain;

static float *MagnitudeLeft, *MagnitudeRight, *PhaseLeft, *PhaseRight, *FifoOutput, *InputBufferFloat;
static int FifoOutputFirstSample, FifoOutputLastSample, FifoCapacity;

static int StepSize, BufferSize, SampleRate;

static float SuppressionRatio;

typedef std::vector<NoisePart> Print;
static Print NoisePrint;
// </Global>

#define FFT_LOG_SIZE 11 // 2^11 = 2048

static bool isNoisePart(const float freq, const Print &print)
{
   if (print.size() == 0)
      return false;

   for (Print::const_iterator it = print.begin(); it != print.end(); ++it)
   {
      if (freq >= it->FreqBeg && freq <= it->FreqEnd)
         return true;
   }

   return false;
}

void getCurTime(double& time)
{
    struct timespec res;
    clock_gettime(CLOCK_REALTIME, &res);
    time = 1000.0 * res.tv_sec + (double) res.tv_nsec / 1e6;
}

// This is called periodically by the media server.
static bool audioProcessing(void *clientdata, short int *audioInputOutput, int numberOfSamples, int samplerate)
{
    SuperpoweredShortIntToFloat(audioInputOutput, InputBufferFloat, numberOfSamples); // Converting the 16-bit integer samples to 32-bit floating point.
    FrequencyDomain->addInput(InputBufferFloat, numberOfSamples); // Input goes to the frequency domain.

    // In the frequency domain we are working with 1024 magnitudes and phases for every channel (left, right), if the fft size is 2048.
    while (FrequencyDomain->timeDomainToFrequencyDomain(MagnitudeLeft, MagnitudeRight, PhaseLeft, PhaseRight)) {
        //double begTime;
        //getCurTime(begTime);
        //__android_log_print(ANDROID_LOG_INFO, "Time (ms):", "%f", begTime);

        for (int i = 0; i < FrequencyDomain->fftSize / 2; ++i)
        {
            MagnitudeLeft[i] *= 5;
            MagnitudeRight[i] *= 5;

            const float freq = ((float)i / (float)FrequencyDomain->fftSize) * SampleRate;
            if ( isNoisePart(freq, NoisePrint) )
            {
                MagnitudeLeft[i] *= SuppressionRatio;
                MagnitudeRight[i] *= SuppressionRatio;
            }
        }

        // We are done working with frequency domain data. Let's go back to the time domain.

        // Check if we have enough room in the fifo buffer for the output. If not, move the existing audio data back to the buffer's beginning.
        if (FifoOutputLastSample + StepSize >= FifoCapacity) { // This will be true for every 100th iteration only, so we save precious memory bandwidth.
            int samplesInFifo = FifoOutputLastSample - FifoOutputFirstSample;
            if (samplesInFifo > 0) memmove(FifoOutput, FifoOutput + FifoOutputFirstSample * 2, samplesInFifo * sizeof(float) * 2);
            FifoOutputFirstSample = 0;
            FifoOutputLastSample = samplesInFifo;
        };

        // Transforming back to the time domain.
        FrequencyDomain->frequencyDomainToTimeDomain(MagnitudeLeft, MagnitudeRight, PhaseLeft, PhaseRight, FifoOutput + FifoOutputLastSample * 2);
        FrequencyDomain->advance();
        FifoOutputLastSample += StepSize;
    };

    // If we have enough samples in the fifo output buffer, pass them to the audio output.
    if (FifoOutputLastSample - FifoOutputFirstSample >= numberOfSamples) {
        SuperpoweredFloatToShortInt(FifoOutput + FifoOutputFirstSample * 2, audioInputOutput, numberOfSamples);
        FifoOutputFirstSample += numberOfSamples;
        return true;
    } else return false;
}

extern "C" {
    JNIEXPORT void Java_com_nikitavasilikhin_earsexapp_MainActivity_Suppression(JNIEnv *javaEnvironment, jobject self, jlong sampleRate, jlong bufferSize);

    JNIEXPORT void Java_com_nikitavasilikhin_earsexapp_MainActivity_StopSuppression(JNIEnv *javaEnvironment, jobject self);

    JNIEXPORT void Java_com_nikitavasilikhin_earsexapp_MainActivity_SetSuppressionRatio(JNIEnv *javaEnvironment, jobject self, jlong suppressionRatio);

    JNIEXPORT void Java_com_nikitavasilikhin_earsexapp_MainActivity_AddFreqRange(JNIEnv *javaEnvironment, jobject self, jfloat beg, jfloat end);
}

JNIEXPORT void Java_com_nikitavasilikhin_earsexapp_MainActivity_Suppression(JNIEnv *javaEnvironment, jobject self, jlong sampleRate, jlong bufferSize)
{
    FrequencyDomain = new SuperpoweredFrequencyDomain(FFT_LOG_SIZE);
    StepSize = FrequencyDomain->fftSize / 4; // The default overlap ratio is 4:1, so we will receive this amount of samples from the frequency domain in one step.

    SampleRate = sampleRate;
    BufferSize = bufferSize;

    // Frequency domain data goes into these buffers:
    MagnitudeLeft = (float *)malloc(FrequencyDomain->fftSize * sizeof(float));
    MagnitudeRight = (float *)malloc(FrequencyDomain->fftSize * sizeof(float));
    PhaseLeft = (float *)malloc(FrequencyDomain->fftSize * sizeof(float));
    PhaseRight = (float *)malloc(FrequencyDomain->fftSize * sizeof(float));

    // Time domain result goes into a FIFO (first-in, first-out) buffer
    FifoOutputFirstSample = FifoOutputLastSample = 0;
    FifoCapacity = StepSize * 100; // Let's make the fifo's size 100 times more than the step size, so we save memory bandwidth.
    FifoOutput = (float *)malloc(FifoCapacity * sizeof(float) * 2 + 128);

    InputBufferFloat = (float *)malloc(bufferSize * sizeof(float) * 2 + 128);
    AudioIO = new SuperpoweredAndroidAudioIO(SampleRate, BufferSize, true, true, audioProcessing, NULL, BufferSize * 2); // Start audio input/output.
}

JNIEXPORT void Java_com_nikitavasilikhin_earsexapp_MainActivity_StopSuppression(JNIEnv *javaEnvironment, jobject self)
{
    delete AudioIO;
}

JNIEXPORT void Java_com_nikitavasilikhin_earsexapp_MainActivity_SetSuppressionRatio(JNIEnv *javaEnvironment, jobject self, jlong suppressionRatio)
{
    SuppressionRatio = suppressionRatio;
}

JNIEXPORT void Java_com_nikitavasilikhin_earsexapp_MainActivity_AddFreqRange(JNIEnv *javaEnvironment, jobject self, jfloat beg, jfloat end)
{
    NoisePrint.push_back( NoisePart(beg, end) );
}