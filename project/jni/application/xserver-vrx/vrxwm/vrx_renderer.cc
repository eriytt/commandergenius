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

#include "vrx_renderer.h"

#include <assert.h>
#include <cmath>
#include <random>

#include "common.h"
#include "algebra.h"
#include "cursor.h"
#include "gl-utils.h"

namespace {
  static const int kTextureFormat = GL_RGB;
  static const int kTextureType = GL_UNSIGNED_BYTE;

  static const float kZNear = 100.0f;
  static const float kZFar = 100000.0f;

  static const int kCoordsPerVertex = 3;

  static const uint64_t kPredictionTimeWithoutVsyncNanos = 50000000;

  static const char* kGridFragmentShader =
    "precision mediump float;\n"
    "varying vec4 v_Color;\n"
    "varying vec3 v_Grid;\n"
    "\n"
    "void main() {\n"
    "    float depth = gl_FragCoord.z / gl_FragCoord.w;\n"
    "    if ((mod(abs(v_Grid.x), 200.0) < 4.0) ||\n"
    "        (mod(abs(v_Grid.z), 200.0) < 4.0)) {\n"
    "      gl_FragColor = max(0.0, (2000.0 - depth) / 2000.0) *\n"
    "                     vec4(1.0, 1.0, 1.0, 1.0) + \n"
    "                     min(1.0, depth / 2000.0) * v_Color;\n"
    "    } else {\n"
    "      gl_FragColor = v_Color;\n"
    "    }\n"
    "}\n";

  static const char* kLightVertexShader =
    "uniform mat4 u_Model;\n"
    "uniform mat4 u_MVP;\n"
    "uniform mat4 u_MVMatrix;\n"
    "uniform vec3 u_LightPos;\n"
    "attribute vec4 a_Position;\n"
    "\n"
    "attribute vec4 a_Color;\n"
    "attribute vec2 a_Texcoord;\n"
    "attribute vec3 a_Normal;\n"
    "\n"
    "varying vec4 v_Color;\n"
    "varying vec3 v_Grid;\n"
    "\n"
    "varying vec2 v_Texcoord;\n"
    "\n"
    "void main() {\n"
    "  v_Texcoord = a_Texcoord;\n"
    "  v_Grid = vec3(u_Model * a_Position);\n"
    "  vec3 modelViewVertex = vec3(u_MVMatrix * a_Position);\n"
    "  vec3 modelViewNormal = vec3(u_MVMatrix * vec4(a_Normal, 0.0));\n"
    "  float distance = length(u_LightPos - modelViewVertex);\n"
    "  vec3 lightVector = normalize(u_LightPos - modelViewVertex);\n"
    "  float diffuse = max(dot(modelViewNormal, lightVector), 0.5);\n"
    "  diffuse = diffuse * (1.0 / (1.0 + (0.00000015 * distance * distance)));\n"
    "  v_Color = a_Color * diffuse;\n"
    "  gl_Position = u_MVP * a_Position;\n"
    "}\n";

  static const char *WindowVertexShader =
    "uniform mat4 u_MVP;\n"
    "attribute vec2 a_Texcoord;\n"
    "attribute vec4 a_Position;\n"
    "varying vec2 v_Texcoord;\n"
    "void main() {\n"
    "  v_Texcoord = a_Texcoord;\n"
    "  gl_Position = u_MVP * a_Position;\n"
    "}\n";

  static const char *WindowFragmentShader =
    "precision mediump float;\n"
    "uniform sampler2D u_Texture;\n"
    "varying vec2 v_Texcoord;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(u_Texture, v_Texcoord);\n"
    "}\n";

  // static const char* kPassthroughFragmentShader =
  //     "precision mediump float;\n"
  //     "\n"
  //     "varying vec4 v_Color;\n"
  //     "\n"
  //     "void main() {\n"
  //     "  gl_FragColor = v_Color;\n"
  //     "}\n";

  static const char* kPassthroughFragmentShader =
    "precision mediump float;\n"
    "uniform sampler2D u_Texture;\n"
    "varying vec2 v_Texcoord;\n"
    "\n"
    "varying vec4 v_Color;\n"
    "\n"
    "void main() {\n"
    //"  gl_FragColor = v_Color;\n"
    //"  gl_FragColor = v_Color * texture2D(u_Texture, v_Texcoord);\n"
    "  gl_FragColor = texture2D(u_Texture, v_Texcoord);\n"
    "}\n";

  static std::array<float, 4> MatrixVectorMul(const gvr::Mat4f& matrix,
                                              const std::array<float, 4>& vec) {
    std::array<float, 4> result;
    for (int i = 0; i < 4; ++i) {
      result[i] = 0;
      for (int k = 0; k < 4; ++k) {
        result[i] += matrix.m[i][k] * vec[k];
      }
    }
    return result;
  }

  static Vec4f MatrixVectorMul(const gvr::Mat4f& matrix,
                               const Vec4f& vec) {
    std::array<float, 4> r = MatrixVectorMul(matrix, vec.v);
    return Vec4f(r[0], r[1], r[2], r[3]);
  }

  static gvr::Mat4f PerspectiveMatrixFromView(const gvr::Rectf& fov, float z_near,
                                              float z_far) {
    gvr::Mat4f result;
    const float x_left = -std::tan(fov.left * M_PI / 180.0f) * z_near;
    const float x_right = std::tan(fov.right * M_PI / 180.0f) * z_near;
    const float y_bottom = -std::tan(fov.bottom * M_PI / 180.0f) * z_near;
    const float y_top = std::tan(fov.top * M_PI / 180.0f) * z_near;
    const float zero = 0.0f;

    assert(x_left < x_right && y_bottom < y_top && z_near < z_far &&
           z_near > zero && z_far > zero);
    const float X = (2 * z_near) / (x_right - x_left);
    const float Y = (2 * z_near) / (y_top - y_bottom);
    const float A = (x_right + x_left) / (x_right - x_left);
    const float B = (y_top + y_bottom) / (y_top - y_bottom);
    const float C = (z_near + z_far) / (z_near - z_far);
    const float D = (2 * z_near * z_far) / (z_near - z_far);

    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        result.m[i][j] = 0.0f;
      }
    }
    result.m[0][0] = X;
    result.m[0][2] = A;
    result.m[1][1] = Y;
    result.m[1][2] = B;
    result.m[2][2] = C;
    result.m[2][3] = D;
    result.m[3][2] = -1;

    return result;
  }

  static gvr::Rectf ModulateRect(const gvr::Rectf& rect, float width,
                                 float height) {
    gvr::Rectf result = {rect.left * width, rect.right * width,
                         rect.bottom * height, rect.top * height};
    return result;
  }

  static gvr::Recti CalculatePixelSpaceRect(const gvr::Sizei& texture_size,
                                            const gvr::Rectf& texture_rect) {
    float width = static_cast<float>(texture_size.width);
    float height = static_cast<float>(texture_size.height);
    gvr::Rectf rect = ModulateRect(texture_rect, width, height);
    gvr::Recti result = {
      static_cast<int>(rect.left), static_cast<int>(rect.right),
      static_cast<int>(rect.bottom), static_cast<int>(rect.top)};
    return result;
  }

  // Generate a random floating point number between 0 and 1.
  static float RandomUniformFloat() {
    static std::random_device random_device;
    static std::mt19937 random_generator(random_device());
    static std::uniform_real_distribution<float> random_distribution(0, 1);
    return random_distribution(random_generator);
  }

  static gvr::Sizei HalfPixelCount(const gvr::Sizei& in) {
    // Scale each dimension by sqrt(2)/2 ~= 7/10ths.
    gvr::Sizei out;
    out.width = (7 * in.width) / 10;
    out.height = (7 * in.height) / 10;
    return out;
  }

}  // namespace

VRXRenderer::VRXRenderer(gvr_context* gvr_context)
  : gvr_api_(gvr::GvrApi::WrapNonOwned(gvr_context)),
    scratch_viewport_(gvr_api_->CreateBufferViewport()),
    floor_vertices_(nullptr),
    floor_colors_(nullptr),
    floor_normals_(nullptr),
    cube_vertices_(nullptr),
    cube_colors_(nullptr),
    cube_found_colors_(nullptr),
    cube_normals_(nullptr),
    light_pos_world_space_({0.0f, 200.0f, 0.0f, 1.0f}),
    object_distance_(3.5f),
    floor_depth_(1024.0f),
    wm(nullptr)
{
  pointerWindow.window = nullptr;
  pointerWindow.x = 0;
  pointerWindow.y = 0;
  VRXSetCallbacks(CreateWindow, DestroyWindow, QueryPointer, QueryPointerWindow, this);
}

VRXRenderer::~VRXRenderer() {
}


void VRXRenderer::InitializeGl() {
  gvr_api_->InitializeGl();

  glClearColor(0.1f, 0.1f, 0.1f, 0.5f);  // Dark background so text shows up.

  cube_vertices_ = world_layout_data_.CUBE_COORDS.data();
  cube_tex_coords_ = world_layout_data_.CUBE_TEX_COORDS.data();
  cube_colors_ = world_layout_data_.CUBE_COLORS.data();
  cube_found_colors_ = world_layout_data_.CUBE_FOUND_COLORS.data();
  cube_normals_ = world_layout_data_.CUBE_NORMALS.data();
  floor_vertices_ = world_layout_data_.FLOOR_COORDS.data();
  floor_normals_ = world_layout_data_.FLOOR_NORMALS.data();
  floor_colors_ = world_layout_data_.FLOOR_COLORS.data();

  int vertex_shader = LoadGLShader(GL_VERTEX_SHADER, &kLightVertexShader);
  int grid_shader = LoadGLShader(GL_FRAGMENT_SHADER, &kGridFragmentShader);
  int pass_through_shader = LoadGLShader(GL_FRAGMENT_SHADER,
                                         &kPassthroughFragmentShader);

  cube_program_ = glCreateProgram();
  glAttachShader(cube_program_, vertex_shader);
  glAttachShader(cube_program_, pass_through_shader);
  glLinkProgram(cube_program_);
  glUseProgram(cube_program_);
  CheckGLError("Cube program");


  
  cube_position_param_ = glGetAttribLocation(cube_program_, "a_Position");
  cube_normal_param_ = glGetAttribLocation(cube_program_, "a_Normal");
  cube_tex_coord_param_ = glGetAttribLocation(cube_program_, "a_Texcoord");
  cube_color_param_ = glGetAttribLocation(cube_program_, "a_Color");

  cube_model_param_ = glGetUniformLocation(cube_program_, "u_Model");
  cube_modelview_param_ = glGetUniformLocation(cube_program_, "u_MVMatrix");
  cube_modelview_projection_param_ =
    glGetUniformLocation(cube_program_, "u_MVP");
  cube_light_pos_param_ = glGetUniformLocation(cube_program_, "u_LightPos");

  CheckGLError("Cube program params");

  floor_program_ = glCreateProgram();
  glAttachShader(floor_program_, vertex_shader);
  glAttachShader(floor_program_, grid_shader);
  glLinkProgram(floor_program_);
  glUseProgram(floor_program_);

  CheckGLError("Floor program");

  floor_position_param_ = glGetAttribLocation(floor_program_, "a_Position");
  floor_normal_param_ = glGetAttribLocation(floor_program_, "a_Normal");
  floor_color_param_ = glGetAttribLocation(floor_program_, "a_Color");

  floor_model_param_ = glGetUniformLocation(floor_program_, "u_Model");
  floor_modelview_param_ = glGetUniformLocation(floor_program_, "u_MVMatrix");
  floor_modelview_projection_param_ =
    glGetUniformLocation(floor_program_, "u_MVP");
  floor_light_pos_param_ = glGetUniformLocation(floor_program_, "u_LightPos");

  CheckGLError("Floor program params");

  int win_vshader = LoadGLShader(GL_VERTEX_SHADER, &WindowVertexShader);
  CheckGLError("Window vertex shader");
  int win_fshader = LoadGLShader(GL_FRAGMENT_SHADER, &WindowFragmentShader);
  CheckGLError("Window fragment shader");
  windowProgram = glCreateProgram();
  CheckGLError("Window program create");
  glAttachShader(windowProgram, win_vshader);
  CheckGLError("Window program attach vertex shader");
  glAttachShader(windowProgram, win_fshader);
  CheckGLError("Window program attach fragment shader");
  glLinkProgram(windowProgram);
  CheckGLError("Window program link");
  glUseProgram(windowProgram);
  CheckGLError("Window program");

  wPos_param = glGetAttribLocation(windowProgram, "a_Position");
  wTex_param = glGetAttribLocation(windowProgram, "a_Texcoord");
  wMVP_param = glGetUniformLocation(windowProgram, "u_MVP");
  
  // Object first appears directly in front of user.
  model_cube_ = {1.0f, 0.0f, 0.0f, 0.0f,
                 0.0f, 0.707f, -0.707f, 0.0f,
                 0.0f, 0.707f, 0.707f, -object_distance_,
                 0.0f, 0.0f, 0.0f, 1.0f};
  model_floor_ = {1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f, -floor_depth_,
                  0.0f, 0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f};

  render_size_ = HalfPixelCount(gvr_api_->GetMaximumEffectiveRenderTargetSize());

  std::vector<gvr::BufferSpec> specs;
  specs.push_back(gvr_api_->CreateBufferSpec());
  specs[0].SetSize(render_size_);
  specs[0].SetColorFormat(GVR_COLOR_FORMAT_RGBA_8888);
  specs[0].SetDepthStencilFormat(GVR_DEPTH_STENCIL_FORMAT_DEPTH_16);
  specs[0].SetSamples(2);
  swapchain_.reset(new gvr::SwapChain(gvr_api_->CreateSwapChain(specs)));

  viewport_list_.reset(new gvr::BufferViewportList(
                                                   gvr_api_->CreateEmptyBufferViewportList()));

  if (not VRXCursor::InitGL())
    LOGE("Failed to initialize cursor");

  LOGI("OpenGL initialized");

  wm = WindowManager::Create(":0");
  // TODO: bail if this happens
  if (!wm)
    LOGE("Failed to initialize window manager");
  wm->Init();
}

// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
// NOTE, 32 bit only!
static inline unsigned int roundUpPow2(unsigned int v)
{
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

const VRXWindow *VRXRenderer::cursorWindow(const Vec4f &view_vector, Vec4f &intersection)
{
  for (auto w: renderWindows)
    {
      Vec4f window_relative_view_vector = MatrixVectorMul(w->head, view_vector);
      if (VRXCursor::IntersectWindow(w, window_relative_view_vector, intersection))
        {
          return w;
        }
    }
  return nullptr;
}

static void logmatrix(const char *name, const gvr::Mat4f &m)
{
  LOGI("%s:", name);
  LOGI("%.2f, %.2f, %.2f, %.2f", m.m[0][0], m.m[0][1], m.m[0][2], m.m[0][3]);
  LOGI("%.2f, %.2f, %.2f, %.2f", m.m[1][0], m.m[1][1], m.m[1][2], m.m[1][3]);
  LOGI("%.2f, %.2f, %.2f, %.2f", m.m[2][0], m.m[2][1], m.m[2][2], m.m[2][3]);
  LOGI("%.2f, %.2f, %.2f, %.2f", m.m[3][0], m.m[3][1], m.m[3][2], m.m[3][3]);
}

#include <unistd.h>
#define M_PI_8 (M_PI_4 / 2.0)
void VRXRenderer::DrawFrame() {
  wm->Run();

  PrepareFramebuffer();

  viewport_list_->SetToRecommendedBufferViewports();
  gvr::Frame frame = swapchain_->AcquireFrame();

  // A client app does its rendering here.
  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos +=
    kPredictionTimeWithoutVsyncNanos;


  head_view_ = gvr_api_->GetHeadSpaceFromStartSpaceRotation(target_time);
  gvr::Mat4f left_eye_matrix = gvr_api_->GetEyeFromHeadMatrix(GVR_LEFT_EYE);
  gvr::Mat4f right_eye_matrix = gvr_api_->GetEyeFromHeadMatrix(GVR_RIGHT_EYE);
  gvr::Mat4f left_eye_view = MatrixMul(left_eye_matrix, head_view_);
  gvr::Mat4f right_eye_view = MatrixMul(right_eye_matrix, head_view_);

  frame.BindBuffer(0);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_SCISSOR_TEST);
  viewport_list_->GetBufferViewport(0, &scratch_viewport_);
  DrawEye(GVR_LEFT_EYE, left_eye_view, scratch_viewport_);
  viewport_list_->GetBufferViewport(1, &scratch_viewport_);
  DrawEye(GVR_RIGHT_EYE, right_eye_view, scratch_viewport_);

  // Bind back to the default framebuffer.
  frame.Unbind();
  frame.Submit(*viewport_list_, head_view_);

  renderWindows.clear();
  windowMutex.lock();
  for (auto wi : windows)
    {
      VRXWindow *w = wi.second;
      unsigned int width, height, mapped;
      void *fb = VRXGetWindowBuffer(w->handle, &width, &height, &mapped);
      if (mapped)
        {
          if (fb != w->buffer /*or width != w->getWidth() or height != w->getHeight()*/)
            {
              LOGI("Window %p has changed buffer from %p to %p of size (%d, %d)",
                   w->handle, w->buffer, fb, width, height);

              // TODO: free texture if there was one already
              if (w->buffer == nullptr)
                {
                  w->texWidth = roundUpPow2(width);
                  w->texHeight = roundUpPow2(height);
                  w->texId = CreateTexture(w->texWidth, w->texHeight);
                }

              w->buffer = fb;
              w->setSize(width, height);
              w->updateTexCoords();
            }

          if (w->buffer != nullptr)
            {
              glBindTexture(GL_TEXTURE_2D, w->texId);
              glTexSubImage2D(GL_TEXTURE_2D, 0,
                              0, 0,
                              width, height,
                              GL_RGBA, GL_UNSIGNED_BYTE,
                              w->buffer);
              // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
              // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
              //glBindTexture(GL_TEXTURE_2D, 0);
              renderWindows.push_back(w);
            }
        }
    }
  windowMutex.unlock();

  for(auto w : renderWindows)
    if (moveFocusedWindow && isFocused(w))
      w->updateTransform(head_view_);


  gvr::Mat4f headInverse = MatrixTranspose(head_view_);
  Vec4f mouse_vector = MatrixVectorMul(headInverse, Vec4f{0.0f, 0.0f, -1.0f, 0.0f});
  Vec4f isect;
  const VRXWindow *hit = cursorWindow(mouse_vector, isect);
  bool send_event = false;
  if (hit)
    {
      VRXCursor::SetCursorMatrix(MatrixMul(hit->headInverse, {1.0, 0.0, 0.0, isect.x(),
              0.0, 1.0, 0.0, isect.y(),
              0.0, 0.0, 1.0, -VRXWindow::DEFAULT_DISTANCE + 10.0,
              0.0, 0.0, 0.0, 1.0}));

      short int x = hit->getHalfWidth() + isect.x();
      short int y = hit->getHalfHeight() - isect.y();
      send_event = hit != pointerWindow.window or x != pointerWindow.x or y != pointerWindow.y;
      pointerWindow.x = x;
      pointerWindow.y = y;
      pointerWindow.window = hit;
    }
  else
    {
      VRXCursor::SetCursorMatrix(MatrixMul(headInverse, { 1.0, 0.0, 0.0, 0.0,
              0.0, 1.0, 0.0, 0.0,
              0.0, 0.0, 1.0, -VRXWindow::DEFAULT_DISTANCE * 1.5f,
              0.0, 0.0, 0.0, 1.0}));

      send_event = pointerWindow.window != nullptr;
      pointerWindow.window = hit;
      pointerWindow.x = pointerWindow.y = -WindowManager::DESKTOP_SIZE / 2;
    }

  if (send_event)
    {
      LOGI("Send motion event {%d, %d}",
           pointerWindow.x + WindowManager::DESKTOP_SIZE / 2,
           pointerWindow.y + WindowManager::DESKTOP_SIZE / 2);
      VRXMouseMotionEvent(pointerWindow.x + WindowManager::DESKTOP_SIZE / 2,
                          pointerWindow.y + WindowManager::DESKTOP_SIZE / 2, false);
    }
  CheckGLError("onDrawFrame");
  //usleep(100000);
}

void VRXRenderer::PrepareFramebuffer() {
  // Because we are using 2X MSAA, we can render to half as many pixels and
  // achieve similar quality.
  gvr::Sizei recommended_size =
    HalfPixelCount(gvr_api_->GetMaximumEffectiveRenderTargetSize());
  if (render_size_.width != recommended_size.width ||
      render_size_.height != recommended_size.height) {
    // We need to resize the framebuffer.
    swapchain_->ResizeBuffer(0, recommended_size);
    render_size_ = recommended_size;
  }
}

void VRXRenderer::OnTriggerEvent() {
  if (IsLookingAtObject()) {
    HideObject();
  }
}

void VRXRenderer::OnPause() {
  gvr_api_->PauseTracking();
}

void VRXRenderer::OnResume() {
  gvr_api_->RefreshViewerProfile();
  gvr_api_->ResumeTracking();
}

/**
 * Converts a raw text file, saved as a resource, into an OpenGL ES shader.
 *
 * @param type  The type of shader we will be creating.
 * @param resId The resource ID of the raw text file.
 * @return The shader object handler.
 */

/**
 * Draws a frame for an eye.
 *
 * @param eye The eye to render. Includes all required transformations.
 */
void VRXRenderer::DrawEye(gvr::Eye eye, const gvr::Mat4f& view_matrix,
                          const gvr::BufferViewport& params) {
  const gvr::Recti pixel_rect =
    CalculatePixelSpaceRect(render_size_, params.GetSourceUv());

  glViewport(pixel_rect.left, pixel_rect.bottom,
             pixel_rect.right - pixel_rect.left,
             pixel_rect.top - pixel_rect.bottom);
  glScissor(pixel_rect.left, pixel_rect.bottom,
            pixel_rect.right - pixel_rect.left,
            pixel_rect.top - pixel_rect.bottom);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  CheckGLError("ColorParam");

  // Set the position of the light
  light_pos_eye_space_  = MatrixVectorMul(view_matrix, light_pos_world_space_);
  gvr::Mat4f perspective =
    PerspectiveMatrixFromView(params.GetSourceFov(), kZNear, kZFar);

  modelview_ = MatrixMul(view_matrix, model_cube_);
  modelview_projection_cube_ = MatrixMul(perspective, modelview_);
  //DrawCube();

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);

  // Set modelview_ for the floor, so we draw floor in the correct location
  modelview_ = MatrixMul(view_matrix, model_floor_);
  modelview_projection_floor_ = MatrixMul(perspective, modelview_);
  DrawFloor();

  gvr::Mat4f mvp = MatrixMul(perspective, view_matrix);

  for (auto w: renderWindows)
    DrawWindow(w, mvp);

  VRXCursor::Draw(mvp);
}

void VRXRenderer::DrawWindow(VRXWindow *win, const gvr::Mat4f &mvp)
{
  glUseProgram(windowProgram);

  // Set the Model in the shader, used to calculate lighting
  // glUniformMatrix4fv(cube_model_param_, 1, GL_FALSE,
  //                    MatrixToGLArray(model_cube_).data());

  // Set the ModelView in the shader, used to calculate lighting
  // glUniformMatrix4fv(cube_modelview_param_, 1, GL_FALSE,
  //                    MatrixToGLArray(modelview_).data());
  
  // Set the position of the window
  glVertexAttribPointer(wPos_param, kCoordsPerVertex, GL_FLOAT, false, 0,
                        win->windowCoords.data());
  glEnableVertexAttribArray(wPos_param);

  CheckGLError("Drawing window: window position");

  // Set texture
  glVertexAttribPointer(wTex_param, 2, GL_FLOAT, false, 0,
                        win->texCoords.data());
  glEnableVertexAttribArray(wTex_param);
  glActiveTexture(GL_TEXTURE0);

  CheckGLError("Drawing window: window texture");

  glBindTexture(GL_TEXTURE_2D, win->texId);

  CheckGLError("Drawing window: window texture bind");

  gvr::Mat4f m = MatrixMul(mvp, win->modelView);
  // Set the ModelViewProjection matrix in the shader.
  glUniformMatrix4fv(wMVP_param, 1, GL_FALSE,
                     MatrixToGLArray(m).data());

  CheckGLError("Drawing window: view matrix");

  // The parameters below are sometimes optimized away, and not available
  // Set the normal positions of the cube, again for shading
  // if( cube_normal_param_ != -1 ){
  //   glVertexAttribPointer( cube_normal_param_, 3, GL_FLOAT, false, 0, cube_normals_);
  //   glEnableVertexAttribArray(cube_normal_param_);
  // }

  // if( cube_color_param_ != -1 ){
  //   glVertexAttribPointer(cube_color_param_, 4, GL_FLOAT, false, 0,
  //                       IsLookingAtObject() ? cube_found_colors_ :cube_colors_);
  //   glEnableVertexAttribArray(cube_color_param_);
  // }


  glDrawArrays(GL_TRIANGLES, 0, 6);
  CheckGLError("Drawing window");
  //glActiveTexture(GL_TEXTURE0);
}

void VRXRenderer::DrawCube() {
  glUseProgram(cube_program_);

  glUniform3fv(cube_light_pos_param_, 1, light_pos_eye_space_.data());

  // Set the Model in the shader, used to calculate lighting
  glUniformMatrix4fv(cube_model_param_, 1, GL_FALSE,
                     MatrixToGLArray(model_cube_).data());

  // Set the ModelView in the shader, used to calculate lighting
  glUniformMatrix4fv(cube_modelview_param_, 1, GL_FALSE,
                     MatrixToGLArray(modelview_).data());

  // Set the position of the cube
  glVertexAttribPointer(cube_position_param_, kCoordsPerVertex, GL_FLOAT,
                        false, 0, cube_vertices_);
  glEnableVertexAttribArray(cube_position_param_);

  // Set texture
  glVertexAttribPointer(cube_tex_coord_param_, 2, GL_FLOAT, false, 0, cube_tex_coords_);
  glEnableVertexAttribArray(cube_tex_coord_param_);
  glActiveTexture(GL_TEXTURE0);

  // Set the ModelViewProjection matrix in the shader.
  glUniformMatrix4fv(cube_modelview_projection_param_, 1, GL_FALSE,
                     MatrixToGLArray(modelview_projection_cube_).data());

  // The parameters below are sometimes optimized away, and not available
  // Set the normal positions of the cube, again for shading
  if( cube_normal_param_ != -1 ){
    glVertexAttribPointer( cube_normal_param_, 3, GL_FLOAT, false, 0, cube_normals_);
    glEnableVertexAttribArray(cube_normal_param_);
  }

  if( cube_color_param_ != -1 ){
    glVertexAttribPointer(cube_color_param_, 4, GL_FLOAT, false, 0,
                          IsLookingAtObject() ? cube_found_colors_ :cube_colors_);
    glEnableVertexAttribArray(cube_color_param_);
  }


  glDrawArrays(GL_TRIANGLES, 0, 36);
  CheckGLError("Drawing cube");
  glActiveTexture(GL_TEXTURE0);
}

void VRXRenderer::DrawFloor() {
  glUseProgram(floor_program_);

  // Set ModelView, MVP, position, normals, and color.
  glUniform3fv(floor_light_pos_param_, 1, light_pos_eye_space_.data());
  glUniformMatrix4fv(floor_model_param_, 1, GL_FALSE,
                     MatrixToGLArray(model_floor_).data());
  glUniformMatrix4fv(floor_modelview_param_, 1, GL_FALSE,
                     MatrixToGLArray(modelview_).data());
  glUniformMatrix4fv(floor_modelview_projection_param_, 1, GL_FALSE,
                     MatrixToGLArray(modelview_projection_floor_).data());
  glVertexAttribPointer(floor_position_param_, kCoordsPerVertex, GL_FLOAT,
                        false, 0, floor_vertices_);
  glVertexAttribPointer(floor_normal_param_, 3, GL_FLOAT, false, 0,
                        floor_normals_);
  glVertexAttribPointer(
                        floor_color_param_, 4, GL_FLOAT, false, 0, floor_colors_);

  glEnableVertexAttribArray(floor_position_param_);
  glEnableVertexAttribArray(floor_normal_param_);
  glEnableVertexAttribArray(floor_color_param_);

  glDrawArrays(GL_TRIANGLES, 0, 24);

  CheckGLError("Drawing floor");
}

void VRXRenderer::HideObject() {
  static const float kMaxModelDistance = 7.0f;
  static const float kMinModelDistance = 3.0f;

  std::array<float, 4> cube_position = {
    model_cube_.m[0][3], model_cube_.m[1][3], model_cube_.m[2][3], 1.f};

  // First rotate in XZ plane, between pi/2 and 3pi/2 radians away, apply this
  // to model_cube_ to keep the front face of the cube towards the user.
  float angle_xz = M_PI * (RandomUniformFloat() + 0.5f);
  gvr::Mat4f rotation_matrix = {{{cosf(angle_xz), 0.f, -sinf(angle_xz), 0.f},
                                 {0.f, 1.f, 0.f, 0.f},
                                 {sinf(angle_xz), 0.f, cosf(angle_xz), 0.f},
                                 {0.f, 0.f, 0.f, 1.f}}};
  cube_position = MatrixVectorMul(rotation_matrix, cube_position);
  model_cube_ = MatrixMul(rotation_matrix, model_cube_);

  // Pick a new distance for the cube, and apply that scale to the position.
  float old_object_distance = object_distance_;
  object_distance_ =
    RandomUniformFloat() * (kMaxModelDistance - kMinModelDistance) +
    kMinModelDistance;
  float scale = object_distance_ / old_object_distance;
  cube_position[0] *= scale;
  cube_position[1] *= scale;
  cube_position[2] *= scale;

  // Choose a random yaw for the cube between pi/4 and -pi/4.
  float yaw = M_PI * (RandomUniformFloat() - 0.5f) / 2;
  cube_position[1] = tanf(yaw) * object_distance_;

  model_cube_.m[0][3] = cube_position[0];
  model_cube_.m[1][3] = cube_position[1];
  model_cube_.m[2][3] = cube_position[2];
}

bool VRXRenderer::IsLookingAtObject() {
  return false;
}

static
std::array<float, 4> getPointArray( const VrxWindowCoords& windowCoords, uint8_t pointNumber)
{
  std::array<float, 4> result;
  result[0] = windowCoords[3*pointNumber];
  result[1] = windowCoords[3*pointNumber+1];
  result[2] = windowCoords[3*pointNumber+2];
  result[3] = 1.0;
  
  return result;
}

static
void setPointArray( VrxWindowCoords& windowCoords, std::array<float, 4> point, uint8_t pointNumber)
{
  windowCoords[3*pointNumber] = point[0];
  windowCoords[3*pointNumber+1] = point[1];
  windowCoords[3*pointNumber+2] = point[2];
}

void VRXRenderer::handleCreateWindow(struct WindowHandle *w)
{
  windowMutex.lock();
  auto it = windows.find(w);
  if (it != windows.end()){
    windowMutex.unlock();
    return;
  }

  VrxWindowCoords windowCoords = world_layout_data_.WINDOW_COORDS;  // Initial window coordinates
  auto vw = new VRXWindow(w, windowCoords);
  vw->updateTransform(head_view_);

  windows[w] = vw;
  focusedWindows.push_front(vw);
  windowMutex.unlock();

  LOGW("New window: %p", w);
}

void VRXRenderer::handleDestroyWindow(struct WindowHandle *w)
{
  LOGI("Destroy window: %p", w);
  windowMutex.lock();
  LOGI("Destroy window: size before destroy: %d", windows.size());
  auto it = windows.find(w);
  if (it == windows.end())
    {
      windowMutex.unlock();
      LOGE("We don't know anything about this window!");
      return;
    }
  
  focusedWindows.remove(it->second);
  windows.erase(it);
  LOGI("Destroy window: size after destroy: %d", windows.size());
  windowMutex.unlock();
}


// TODO: This is not really thread safe. We can be sure that window list does not change
//       under our feet, but any of the matrices used in transforms change, and in particular
//       between transforms from eye space to world space and back.
QueryPointerReturn VRXRenderer::handleQueryPointer(struct WindowHandle *w)
{
  QueryPointerReturn r;

  const VRXWindow *vw = windows[w];
  gvr::Mat4f headInverse = MatrixTranspose(head_view_);
  Vec4f mouse_vector = MatrixVectorMul(headInverse, Vec4f{0.0f, 0.0f, -1.0f, 0.0f});
  Vec4f window_relative_view_vector = MatrixVectorMul(vw->head, mouse_vector);

  Vec4f isect;
  if (VRXCursor::IntersectWindow(vw, window_relative_view_vector, isect))
    {
      r.root_x = WindowManager::DESKTOP_SIZE / 2 + vw->getHalfWidth() + isect.x();
      r.root_y = WindowManager::DESKTOP_SIZE / 2 + vw->getHalfHeight() - isect.y();
      r.win_x = vw->getHalfWidth() + isect.x();
      r.win_y = vw->getHalfHeight() - isect.y();
    }
  else
    {
      r.root_x = 0;
      r.root_y = 0;
      r.win_x = -WindowManager::DESKTOP_SIZE / 2;
      r.win_y = -WindowManager::DESKTOP_SIZE / 2;
    }

  return r;
}

struct WindowHandle *VRXRenderer::handleQueryPointerWindow()
{
  if (not pointerWindow.window)
    return nullptr;

  if (std::find_if(windows.begin(), windows.end(),
                   [&](decltype(*windows.end()) p)
                   {return p.second == pointerWindow.window;})
      == windows.end())
    return nullptr;

  return pointerWindow.window->handle;
}

void VRXRenderer::focusMRUWindow(uint16_t num)
{
  // Take win #num in Most Recently Used list and move to front
  if (focusedWindows.size() == 0){ return; }

  num = num % focusedWindows.size();
  
  auto it = focusedWindows.begin();
  while(num>0)
    {
      it++;
      --num;
    }
  
  auto tempWinPtr = *it;
  focusedWindows.erase(it);
  focusedWindows.push_front(tempWinPtr);
  LOGI("Window focused: %p", tempWinPtr->handle);

}


bool VRXRenderer::isFocused(const VRXWindow * win)
{
  return win==focusedWindows.front();
}


KeyMap& VRXRenderer::keyMap()
{
  return mKeyMap;
}
  
void VRXRenderer::toggleMoveFocusedWindow()
{
  moveFocusedWindow = !moveFocusedWindow;
  LOGI("Move Window: %s", moveFocusedWindow? "enabled" : "disabled");
};

void VRXRenderer::changeWindowSize(float sizeDiff)
{
  if (not focusedWindows.size()) return;
  VRXWindow *w = focusedWindows.front();
  if (w->scale + sizeDiff <= 0.0) return;

  w->scale += sizeDiff;
  w->setSize(w->width, w->height);
}

void VRXRenderer::changeWindowDistance(float distanceDiff)
{
  if (not focusedWindows.size()) return;
  VRXWindow *w = focusedWindows.front();
  if (w->distance + distanceDiff <= kZNear) return;
    
  w->distance += distanceDiff;
  w->updateTransform(w->head);
}

void VRXWindow::updateTransform(const gvr::Mat4f &newHead)
{
  gvr::Mat4f trans = {1.0f,   0.0f,    0.0f,      0.0f,
                      0.0f,   1.0,     0.0f,      0.0f,
                      0.0f,   0.0f,    1.0f, -distance,
                      0.0f,   0.0f,    0.0f,      1.0f};
  head = newHead;
  headInverse = MatrixTranspose(head);
  modelView = MatrixMul(headInverse, trans);
}
