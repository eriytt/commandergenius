#ifndef COMMON_H
#define COMMON_H

#include <android/log.h>

#define LOG_TAG "VRXWM"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define CHECK(condition) if (!(condition)) { \
        LOGE("*** CHECK FAILED at %s:%d: %s", __FILE__, __LINE__, #condition); \
        abort(); }

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))

template <typename T>
T* CHECK_NOTNULL(T* ptr)
{
  CHECK_NE(ptr, nullptr);
  return ptr;
}

#endif // COMMON_H
