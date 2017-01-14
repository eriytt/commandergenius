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
#include <map>
#include <list>

extern "C" {
#include <vrxexport.h>
}

#include "vr/gvr/capi/include/gvr.h"
#include "vr/gvr/capi/include/gvr_types.h"
#include "world_layout_data.h"
#include "wm.h"
#include "algebra.h"

#include "vrx_types.h"

struct VRXWindow
{
  struct WindowHandle *handle;
  void *buffer;
  gvr::Mat4f modelView;
  gvr::Mat4f head;
  gvr::Mat4f headInverse;
  unsigned int texId;
  VrxWindowCoords windowCoords;
  VRXWindow(struct WindowHandle *w, const VrxWindowCoords& initialPosition) 
    : handle(w), buffer(nullptr), windowCoords(initialPosition) {}
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
   * Hide the cube if it's being targeted.
   */
  void OnTriggerEvent();

  /**
   * Pause head tracking.
   */
  void OnPause();

  /**
   * Resume head tracking, refreshing viewer parameters if necessary.
   */
  void OnResume();

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

  /**
   * Draw the cube.
   *
   * We've set all of our transformation matrices. Now we simply pass them
   * into the shader.
   */
  void DrawCube();

  void DrawWindow(const VRXWindow *win, const gvr::Mat4f &perspective);

  /**
   * Draw the floor.
   *
   * This feeds in data for the floor into the shader. Note that this doesn't
   * feed in data about position of the light, so if we rewrite our code to
   * draw the floor first, the lighting might look strange.
   */
  void DrawFloor();

  /**
   * Find a new random position for the object.
   *
   * We'll rotate it around the Y-axis so it's out of sight, and then up or
   * down by a little bit.
   */
  void HideObject();

  /**
   * Check if user is looking at object by calculating where the object is
   * in eye-space.
   *
   * @return true if the user is looking at the object.
   */
  bool IsLookingAtObject();

  const VRXWindow *cursorWindow(const Vec4f &view_vector, Vec4f &intersection);

  std::unique_ptr<gvr::GvrApi> gvr_api_;
  std::unique_ptr<gvr::BufferViewportList> viewport_list_;
  std::unique_ptr<gvr::SwapChain> swapchain_;
  gvr::BufferViewport scratch_viewport_;

  std::vector<float> lightpos_;

  WorldLayoutData world_layout_data_;
  float* floor_vertices_;
  float* floor_colors_;
  float* floor_normals_;
  float* cube_vertices_;
  float *cube_tex_coords_;
  float* cube_colors_;
  float* cube_found_colors_;
  float* cube_normals_;

  int cube_program_;
  int floor_program_;
  int cube_position_param_;
  int cube_normal_param_;
  int cube_tex_coord_param_;
  int cube_color_param_;
  int cube_model_param_;
  int cube_modelview_param_;
  int cube_modelview_projection_param_;
  int cube_light_pos_param_;
  int floor_position_param_;
  int floor_normal_param_;
  int floor_color_param_;
  int floor_model_param_;
  int floor_modelview_param_;
  int floor_modelview_projection_param_;
  int floor_light_pos_param_;

  std::array<float, 4> light_pos_world_space_;
  std::array<float, 4> light_pos_eye_space_;
  gvr::Mat4f head_view_;
  gvr::Mat4f model_cube_;
  gvr::Mat4f camera_;
  gvr::Mat4f view_;
  gvr::Mat4f modelview_projection_cube_;
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

  void handleCreateWindow(struct WindowHandle *pWin);
  void handleDestroyWindow(struct WindowHandle *pWin);

  static void CreateWindow(struct WindowHandle *pWin, void *instance)
  {reinterpret_cast<VRXRenderer*>(instance)->handleCreateWindow(pWin);}
  static void DestroyWindow(struct WindowHandle *pWin, void *instance)
  {reinterpret_cast<VRXRenderer*>(instance)->handleDestroyWindow(pWin);}

  std::mutex windowMutex;
  std::map<struct WindowHandle*, VRXWindow *> windows;
  std::list<const VRXWindow *> renderWindows;

  Planef screenplane;
};

#endif  // VRX_APP_SRC_MAIN_JNI_VRXRENDERER_H_  // NOLINT
