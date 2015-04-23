#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jstring JNICALL Java_com_nikitavasilikhin_earsexapp_MainActivity_stringFromJNI(JNIEnv* env, jclass clazz);

#ifdef __cplusplus
}
#endif