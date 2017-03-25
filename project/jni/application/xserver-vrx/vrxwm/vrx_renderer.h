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

#ifndef VRX_APP_SRC_MAIN_JNI_VRXRENDERER_H_  // NOLINT
#define VRX_APP_SRC_MAIN_JNI_VRXRENDERER_H_  // NOLINT

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <jni.h>

#include <memory>
#include <string>
#include <thread>  // NOLINT
#include <vector>
#include <list>

#include <linux/input.h>

extern "C" {
#include <vrxexport.h>
}

#include "vr/gvr/capi/include/gvr.h"
#include "vr/gvr/capi/include/gvr_types.h"
#include "wm.h"
#include "algebra.h"
#include "world_layout_data.h"
#include "vrx_types.h"

class KeyMap
{
public:
  int commandKey = KEY_A;
  bool isCommandMode = false;
  void setKey(int code, uint8_t isDown)
  {
    if (code < 256) keys[code] = isDown;
  }
  
  uint8_t getKey(int code)
  {
    if (code < 256) return keys[code];
    
    return 0;
  }
private:
  uint8_t keys[256];
};

class VRXRenderer {
 public:
  /**
   * Create a VRXRenderer using a given |gvr_context|.
   *
   * @param gvr_api The (non-owned) gvr_context.
   */
  VRXRenderer(gvr_context* gvr_context);

  /**
   * Destructor.
   */
  ~VRXRenderer();

  /**
   * Initialize any GL-related objects. This should be called on the rendering
   * thread with a valid GL context.
   */
  void InitializeGl();

  /**
   * Draw the VRX scene. This should be called on the rendering thread.
   */
  void DrawFrame();

  /**
   * Pause head tracking.
   */
  void OnPause();

  /**
   * Resume head tracking, refreshing viewer parameters if necessary.
   */
  void OnResume();

  KeyMap& keyMap();
  
  //void focusMRUWindow(uint16_t num) {wm->focusMRUWindow(num);};
  
  void toggleMoveFocusedWindow();

  WindowManager *getWM() {return wm.get();}
  
  const gvr::Mat4f &getHeadView() const {return head_view;}
  const gvr::Mat4f &getHeadInverse() const {return head_inverse;}
 private:

  /*
   * Prepares the GvrApi framebuffer for rendering, resizing if needed.
   */
  void PrepareFramebuffer();

  /**
   * Draws a frame for an eye.
   *
   * @param eye The eye to render. Includes all required transformations.
   */
  void DrawEye(gvr::Eye eye, const gvr::Mat4f& view_matrix,
               const gvr::BufferViewport& params);

  void DrawWindow(WmWindow *win, const gvr::Mat4f &perspective);

  /**
   * Draw the floor.
   *
   * This feeds in data for the floor into the shader. Note that this doesn't
   * feed in data about position of the light, so if we rewrite our code to
   * draw the floor first, the lighting might look strange.
   */
  void DrawFloor();

  const WmWindow *cursorWindow(const Vec4f &view_vector, Vec4f &intersection);

  std::unique_ptr<gvr::GvrApi> gvr_api_;
  std::unique_ptr<gvr::BufferViewportList> viewport_list_;
  std::unique_ptr<gvr::SwapChain> swapchain_;
  gvr::BufferViewport scratch_viewport_;

  std::vector<float> lightpos_;

  WorldLayoutData world_layout_data_;
  float* floor_vertices_;
  float* floor_colors_;
  float* floor_normals_;

  int floor_program_;
  int floor_position_param_;
  int floor_normal_param_;
  int floor_color_param_;
  int floor_model_param_;
  int floor_modelview_param_;
  int floor_modelview_projection_param_;
  int floor_light_pos_param_;

  std::array<float, 4> light_pos_world_space_;
  std::array<float, 4> light_pos_eye_space_;
  gvr::Mat4f head_view;
  gvr::Mat4f head_inverse;
  gvr::Mat4f camera_;
  gvr::Mat4f view_;
  gvr::Mat4f modelview_projection_floor_;
  gvr::Mat4f modelview_;
  gvr::Mat4f model_floor_;
  gvr::Sizei render_size_;

  int score_;
  float object_distance_;
  float floor_depth_;

  int windowProgram;

  int wMVP_param;
  int wTex_param;
  int wPos_param;

  std::unique_ptr<WindowManager> wm;

  std::list<WmWindow *> renderWindows;

  KeyMap mKeyMap;
  
  bool moveFocusedWindow = false;

};

#endif  // VRX_APP_SRC_MAIN_JNI_VRXRENDERER_H_  // NOLINT
