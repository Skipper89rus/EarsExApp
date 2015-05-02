#include <android/log.h>
#include "FftWrapper.h"

JNIEXPORT jfloatArray JNICALL Java_com_nikitavasilikhin_earsexapp_MainActivity_Fft(JNIEnv* env, jclass cls, jfloatArray signal)
{
    jsize signalSz = env->GetArrayLength(signal);

    jfloatArray res = env->NewFloatArray(signalSz);

    audio_real signalReal[signalSz];
    env->GetFloatArrayRegion(signal, 0, signalSz, signalReal);

    Cmplx* spec = new Cmplx[signalSz];

    Fft fft;
    fft.SetMode( FFT_REAL, signalSz );
    fft.FftReal( signalReal, spec );

    float magnitudes[signalSz];
    for (unsigned i = 0; i < signalSz; ++i)
        magnitudes[i] = sqrtf( spec[i].re * spec[i].re + spec[i].im * spec[i].im );

    env->SetFloatArrayRegion(res, 0, signalSz, magnitudes);
    return res;
}