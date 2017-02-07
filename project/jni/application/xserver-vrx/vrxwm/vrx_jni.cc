/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/log.h>
#include <jni.h>

#include <memory>
#include <linux/input.h>

extern "C" {
#include <vrxexport.h>
}

#include "vr/gvr/capi/include/gvr.h"
#include "vrx_renderer.h"
#include "common.h"

#define JNI_METHOD(return_type, method_name) \
  JNIEXPORT return_type JNICALL              \
      Java_com_towersmatrix_vrx_VRXActivity_##method_name

namespace {

inline jlong jptr(VRXRenderer *native_vrx_pointer) {
  return reinterpret_cast<intptr_t>(native_vrx_pointer);
}

inline VRXRenderer *native(jlong ptr) {
  return reinterpret_cast<VRXRenderer *>(ptr);
}
}  // anonymous namespace

extern "C" {

JNI_METHOD(jlong, nativeCreateRenderer)(JNIEnv *env, jclass clazz,
                                        jobject class_loader,
                                        jobject android_context,
                                        jlong native_gvr_api) {
  return jptr(new VRXRenderer(reinterpret_cast<gvr_context *>(native_gvr_api)));
}

JNI_METHOD(void, nativeDestroyRenderer)
(JNIEnv *env, jclass clazz, jlong native_vrx_pointer) {
  delete native(native_vrx_pointer);
}

JNI_METHOD(void, nativeInitializeGl)(JNIEnv *env, jobject obj,
                                     jlong native_vrx_pointer) {
  native(native_vrx_pointer)->InitializeGl();
}

JNI_METHOD(void, nativeDrawFrame)(JNIEnv *env, jobject obj,
                                  jlong native_vrx_pointer) {
  native(native_vrx_pointer)->DrawFrame();
}

JNI_METHOD(void, nativeOnTriggerEvent)(JNIEnv *env, jobject obj,
                                       jlong native_vrx_pointer) {
  native(native_vrx_pointer)->OnTriggerEvent();
}

JNI_METHOD(void, nativeOnPause)(JNIEnv *env, jobject obj,
                                jlong native_vrx_pointer) {
  native(native_vrx_pointer)->OnPause();
}

JNI_METHOD(void, nativeOnResume)(JNIEnv *env, jobject obj,
                                 jlong native_vrx_pointer) {
  native(native_vrx_pointer)->OnResume();
}

bool isModifierKey(int keyCode)
{
  switch( keyCode )
  {
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      return true;
      
    default:
      return false;
  }
}

JNI_METHOD(jint, nativeWMEvent)(JNIEnv *env, jobject thiz, jlong native_vrx_pointer, jint scancode, jboolean down) {
  LOGI("nativeWMEvent key event, scancode %d, down = %d", scancode, down);

  VRXRenderer* vrxRenderer = native(native_vrx_pointer);
  vrxRenderer->keyMap().setKey(scancode, down);

  // In command mode: trap everything except modifier keys
  // Ignore key down. action is on key up.
  if (!vrxRenderer->keyMap().isCommandMode)
  {
    //CMD key
    if (scancode==vrxRenderer->keyMap().commandKey && vrxRenderer->keyMap().getKey(KEY_LEFTCTRL))
    {
      if (down){ return 1; }
      
      LOGI("\n\nnativeWMEvent Enter command mode\n\n");
      vrxRenderer->keyMap().isCommandMode = true;
      return 1;
    }

    return 0;    // Normal mode - let key through
  }

  // TODO Handle command mode
  // if valid command :  execute cmd and exit command mode
  // else: exit command mode anyway
  if (isModifierKey(scancode)){ return 0; }
  

  if (down){ return 1; }    // Ignore down

  if (scancode==KEY_A)
  {
    vrxRenderer->focusMRUWindow(1); // Take second window and move to front;
  }

  if (scancode==KEY_B)
  {
    vrxRenderer->changeWindowSize(0.3);
  }
  if (scancode==KEY_S)
  {
    vrxRenderer->changeWindowSize(-0.3);
  }
  
  if (scancode==KEY_C)
  {
    vrxRenderer->changeWindowDistance(-30.0);
  }
  if (scancode==KEY_F)
  {
    vrxRenderer->changeWindowDistance(30.0);
  }

  if (scancode==KEY_M)
  {
    vrxRenderer->toggleMoveFocusedWindow();
  }

  if (scancode == KEY_ENTER)
    {
      VRXMouseButtonEvent(1);
      VRXMouseButtonEvent(0);
    }

  // Not any known command: exit command mode, but trap key
  vrxRenderer->keyMap().isCommandMode = false;
  return 1;
}


}  // extern "C"
