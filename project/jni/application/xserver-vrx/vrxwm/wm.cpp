/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *  Copyright (C) 2013 Chuan Ji <jichuan89@gmail.com>                        *
 *                                                                           *
 *  Licensed under the Apache License, Version 2.0 (the "License");          *
 *  you may not use this file except in compliance with the License.         *
 *  You may obtain a copy of the License at                                  *
 *                                                                           *
 *   http://www.apache.org/licenses/LICENSE-2.0                              *
 *                                                                           *
 *  Unless required by applicable law or agreed to in writing, software      *
 *  distributed under the License is distributed on an "AS IS" BASIS,        *
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
 *  See the License for the specific language governing permissions and      *
 *  limitations under the License.                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "wm.h"
extern "C" {
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
}
#include <cstring>
#include <algorithm>

#include "common.h"
#include "wm-util.h"
#include "world_layout_data.h"
#include "vrx_renderer.h"
#include "algebra.h"
#include "cursor.h"

bool WindowManager::wm_detected_;
std::mutex WindowManager::wm_detected_mutex_;

std::unique_ptr<WindowManager>
WindowManager::Create(const VRXRenderer *renderer, const std::string& display_str) {
  // 1. Open X display.
  const char* display_c_str =
        display_str.empty() ? nullptr : display_str.c_str();
  Display* display = XOpenDisplay(display_c_str);
  if (display == nullptr) {
    LOGE("Failed to open X display %s", XDisplayName(display_c_str));
    return nullptr;
  }
  // 2. Construct WindowManager instance.
  return std::unique_ptr<WindowManager>(new WindowManager(display, renderer));
}

WindowManager::WindowManager(Display* display, const VRXRenderer *renderer)
    : display_(CHECK_NOTNULL(display)),
      root_(DefaultRootWindow(display_)),
      WM_PROTOCOLS(XInternAtom(display_, "WM_PROTOCOLS", false)),
      WM_DELETE_WINDOW(XInternAtom(display_, "WM_DELETE_WINDOW", false)),
      renderer(renderer)
{
  pointerWindow.window = nullptr;
  pointerWindow.x = 0;
  pointerWindow.y = 0;
}

WindowManager::~WindowManager() {
  XCloseDisplay(display_);
}

void WindowManager::Init() {

  if (!XCompositeQueryExtension(display_, &cEventBase, &cErrorBase))
    LOGE("X server does not support compositing");

  // 1. Initialization.
  //   a. Select events on root window. Use a special error handler so we can
  //   exit gracefully if another window manager is already running.
  {
    std::lock_guard<std::mutex> lock(wm_detected_mutex_);

    wm_detected_ = false;
    XSetErrorHandler(&WindowManager::OnWMDetected);
    XSelectInput(
        display_,
        root_,
        SubstructureRedirectMask | SubstructureNotifyMask);
    XSync(display_, false);
    if (wm_detected_) {
      LOGE("Detected another window manager on display %s", XDisplayString(display_));
      return;
    }
  }
  //   b. Set error handler.
  XSetErrorHandler(&WindowManager::OnXError);
  //   c. Grab X server to prevent windows from changing under us.
  XGrabServer(display_);
  //   d. Reparent existing top-level windows.
  //     i. Query existing top-level windows.
  Window returned_root, returned_parent;
  Window* top_level_windows;
  unsigned int num_top_level_windows;
  CHECK(XQueryTree(
      display_,
      root_,
      &returned_root,
      &returned_parent,
      &top_level_windows,
      &num_top_level_windows));
  CHECK_EQ(returned_root, root_);
  //     ii. Frame each top-level window.
  for (unsigned int i = 0; i < num_top_level_windows; ++i) {
    Frame(top_level_windows[i]);
  }
  //     iii. Free top-level window array.
  XFree(top_level_windows);

  XCompositeRedirectSubwindows(display_, root_, CompositeRedirectManual);

  //   e. Ungrab X server.
  XUngrabServer(display_);
}

void WindowManager::Run() {
  while (XPending(display_))
    {
      // 1. Get next event.
      XEvent e;
      XNextEvent(display_, &e);
      LOGI("Received event: %s", ToString(e).c_str());

      // 2. Dispatch event.
      switch (e.type) {
      case CreateNotify:
        OnCreateNotify(e.xcreatewindow);
        break;
      case DestroyNotify:
        OnDestroyNotify(e.xdestroywindow);
        break;
      case ReparentNotify:
        OnReparentNotify(e.xreparent);
        break;
      case MapNotify:
        OnMapNotify(e.xmap);
        break;
      case UnmapNotify:
        OnUnmapNotify(e.xunmap);
        break;
      case ConfigureNotify:
        OnConfigureNotify(e.xconfigure);
        break;
      case MapRequest:
        OnMapRequest(e.xmaprequest);
        break;
      case ConfigureRequest:
        OnConfigureRequest(e.xconfigurerequest);
        break;
      case ButtonPress:
        OnButtonPress(e.xbutton);
        break;
      case ButtonRelease:
        OnButtonRelease(e.xbutton);
        break;
      case MotionNotify:
        // Skip any already pending motion events.
        while (XCheckTypedWindowEvent(
				      display_, e.xmotion.window, MotionNotify, &e)) {}
        OnMotionNotify(e.xmotion);
        break;
      case KeyPress:
        OnKeyPress(e.xkey);
        break;
      case KeyRelease:
        OnKeyRelease(e.xkey);
        break;
      default:
        LOGW("Ignored event");
      }
    }
}

void WindowManager::Frame(Window w) {
  // Visual properties of the frame to create.
  const unsigned int BORDER_WIDTH = 10;
  const unsigned long BORDER_COLOR = 0xd0d0d0;
  const unsigned long BG_COLOR = 0x0000ff;
  LOGI("Framing window %ld", w);
  CHECK(!clients_.count(w));

  // 1. Retrieve attributes of window to frame.
  XWindowAttributes x_window_attrs;
  CHECK(XGetWindowAttributes(display_, w, &x_window_attrs));
  // 2. Create frame.
  const Window frame = XCreateSimpleWindow(
      display_,
      root_,
      DESKTOP_SIZE / 2,
      DESKTOP_SIZE / 2,
      x_window_attrs.width,
      x_window_attrs.height,
      BORDER_WIDTH,
      BORDER_COLOR,
      BG_COLOR);
  // 3. Select events on frame.
  XSelectInput(
      display_,
      frame,
      SubstructureRedirectMask | SubstructureNotifyMask);
  // 4. Add client to save set, so that it will be restored and kept alive if we
  // crash.
  XAddToSaveSet(display_, w);
  // 5. Reparent client window.
  XReparentWindow(
      display_,
      w,
      frame,
      0, 0);  // Offset of client window within frame.
  // 6. Map frame.
  XMapWindow(display_, frame);
  // 7. Save frame handle.
  clients_[w] = frame;
  // 8. Grab universal window management actions on client window.
  //   a. Move windows with alt + left button.
  XGrabButton(
      display_,
      Button1,
      Mod1Mask,
      w,
      false,
      ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
      GrabModeAsync,
      GrabModeAsync,
      None,
      None);
  //   b. Resize windows with alt + right button.
  XGrabButton(
      display_,
      Button3,
      Mod1Mask,
      w,
      false,
      ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
      GrabModeAsync,
      GrabModeAsync,
      None,
      None);
  //   c. Kill windows with alt + f4.
  XGrabKey(
      display_,
      XKeysymToKeycode(display_, XK_F4),
      Mod1Mask,
      w,
      false,
      GrabModeAsync,
      GrabModeAsync);
  //   d. Switch windows with alt + tab.
  XGrabKey(
      display_,
      XKeysymToKeycode(display_, XK_Tab),
      Mod1Mask,
      w,
      false,
      GrabModeAsync,
      GrabModeAsync);

  LOGI("Framed window %ld [%ld]", w, frame);
}

void WindowManager::Unframe(Window w) {
  CHECK(clients_.count(w));

  // We reverse the steps taken in Frame().
  const Window frame = clients_[w];
  windows[frame]->mapped = false;
  // 1. Unmap frame.
  XUnmapWindow(display_, frame);
  // 2. Reparent client window.
  XReparentWindow(
      display_,
      w,
      root_,
      0, 0);  // Offset of client window within root.
  // 3. Remove client window from save set, as it is now unrelated to us.
  XRemoveFromSaveSet(display_, w);
  // 4. Destroy frame.
  XDestroyWindow(display_, frame);
  // 5. Drop reference to frame handle.
  clients_.erase(w);

  LOGI("Unframed window %ld [%ld]", w, frame);
}

void WindowManager::OnCreateNotify(const XCreateWindowEvent& e)
{
  LOGI("OnCreateNotify");
}

void WindowManager::OnDestroyNotify(const XDestroyWindowEvent& e)
{
  LOGI("OnDestroyNotify");
}

void WindowManager::OnReparentNotify(const XReparentEvent& e)
{
  LOGI("OnReparentNotify");
}

void WindowManager::OnMapNotify(const XMapEvent& e)
{
  LOGI("OnMapNotify");

  auto it = windows.find(e.window);
  if (it != windows.end())
      return;

  struct WindowHandle *w = idToHandle(e.window);
  if (not w)
    {
      LOGE("Could not look up handle for window %ld", e.window);
      return;
    }

  // New window
  VrxWindowCoords windowCoords =
    {
      // Front face
      -2.0f,  2.0f,  0.0f,
      -2.0f, -2.0f,  0.0f,
      2.0f,  2.0f,  0.0f,
      -2.0f, -2.0f,  0.0f,
      2.0f, -2.0f,  0.0f,
      2.0f,  2.0f,  0.0f,
    };
  auto vw = new VRXWindow(w, e.window, windowCoords);
  vw->updateTransform(renderer->getHeadView());
  vw->mapped = true;
  windows[e.window] = vw;
}

void WindowManager::OnUnmapNotify(const XUnmapEvent& e) {
  LOGI("OnUnmapNotify");
  // If the window is a client window we manage, unframe it upon UnmapNotify. We
  // need the check because other than a client window, we can receive an
  // UnmapNotify for
  //     - A frame we just destroyed ourselves.
  //     - A pre-existing and mapped top-level window we reparented.
  if (!clients_.count(e.window)) {
    LOGI("Ignore UnmapNotify for non-client window %ld", e.window);
    return;
  }
  if (e.event == root_) {
    LOGI("Ignore UnmapNotify for reparented pre-existing window %ld", e.window);
    return;
  }
  Unframe(e.window);
}

void WindowManager::OnConfigureNotify(const XConfigureEvent& e)
{
  LOGI("OnConfigureNotify");
}

void WindowManager::OnMapRequest(const XMapRequestEvent& e) {
  LOGI("OnMapRequest");
  // 1. Frame or re-frame window (if it has not been framed already,
  //    multiple XMapWindow is allowed but only the first one has any effect).
  if (not clients_.count(e.window))
    Frame(e.window);
  // 2. Actually map window.
  XMapWindow(display_, e.window);
}

void WindowManager::OnConfigureRequest(const XConfigureRequestEvent& e) {
  LOGI("OnConfigureRequest");
  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.width = e.width;
  changes.height = e.height;
  changes.border_width = e.border_width;
  changes.sibling = e.above;
  changes.stack_mode = e.detail;
  if (clients_.count(e.window)) {
    const Window frame = clients_[e.window];
    XConfigureWindow(display_, frame, e.value_mask, &changes);
    LOGI("Resize [%ld] to %s", frame, Size<int>(e.width, e.height).ToString().c_str());
  }
  XConfigureWindow(display_, e.window, e.value_mask, &changes);
  LOGI("Resize %ld to %s", e.window, Size<int>(e.width, e.height).ToString().c_str());
}

void WindowManager::OnButtonPress(const XButtonEvent& e) {
  LOGI("OnButtonPress");
  CHECK(clients_.count(e.window));
  const Window frame = clients_[e.window];

  // 1. Save initial cursor position.
  drag_start_pos_ = Position<int>(e.x_root, e.y_root);

  // 2. Save initial window info.
  Window returned_root;
  int x, y;
  unsigned width, height, border_width, depth;
  CHECK(XGetGeometry(
      display_,
      frame,
      &returned_root,
      &x, &y,
      &width, &height,
      &border_width,
      &depth));
  drag_start_frame_pos_ = Position<int>(x, y);
  drag_start_frame_size_ = Size<int>(width, height);

  // 3. Raise clicked window to top.
  XRaiseWindow(display_, frame);
}

void WindowManager::OnButtonRelease(const XButtonEvent& e)
{
  LOGI("OnButtonRelease");
}

void WindowManager::OnMotionNotify(const XMotionEvent& e) {
  LOGI("OnMotionNotify");
  CHECK(clients_.count(e.window));
  const Window frame = clients_[e.window];
  const Position<int> drag_pos(e.x_root, e.y_root);
  const Vector2D<int> delta = drag_pos - drag_start_pos_;

  if (e.state & Button1Mask ) {
    // alt + left button: Move window.
    const Position<int> dest_frame_pos = drag_start_frame_pos_ + delta;
    XMoveWindow(
        display_,
        frame,
        dest_frame_pos.x, dest_frame_pos.y);
  } else if (e.state & Button3Mask) {
    // alt + right button: Resize window.
    // Window dimensions cannot be negative.
    const Vector2D<int> size_delta(
        std::max(delta.x, -drag_start_frame_size_.width),
        std::max(delta.y, -drag_start_frame_size_.height));
    const Size<int> dest_frame_size = drag_start_frame_size_ + size_delta;
    // 1. Resize frame.
    XResizeWindow(
        display_,
        frame,
        dest_frame_size.width, dest_frame_size.height);
    // 2. Resize client window.
    XResizeWindow(
        display_,
        e.window,
        dest_frame_size.width, dest_frame_size.height);
  }
}

void WindowManager::OnKeyPress(const XKeyEvent& e) {
  if ((e.state & Mod1Mask) &&
      (e.keycode == XKeysymToKeycode(display_, XK_F4))) {
    // alt + f4: Close window.
    //
    // There are two ways to tell an X window to close. The first is to send it
    // a message of type WM_PROTOCOLS and value WM_DELETE_WINDOW. If the client
    // has not explicitly marked itself as supporting this more civilized
    // behavior (using XSetWMProtocols()), we kill it with XKillClient().
    Atom* supported_protocols;
    int num_supported_protocols;
    if (XGetWMProtocols(display_,
                        e.window,
                        &supported_protocols,
                        &num_supported_protocols) &&
        (std::find(supported_protocols,
                   supported_protocols + num_supported_protocols,
                   WM_DELETE_WINDOW) !=
         supported_protocols + num_supported_protocols)) {
      LOGI("Gracefully deleting window %ld", e.window);
      // 1. Construct message.
      XEvent msg;
      memset(&msg, 0, sizeof(msg));
      msg.xclient.type = ClientMessage;
      msg.xclient.message_type = WM_PROTOCOLS;
      msg.xclient.window = e.window;
      msg.xclient.format = 32;
      msg.xclient.data.l[0] = WM_DELETE_WINDOW;
      // 2. Send message to window to be closed.
      CHECK(XSendEvent(display_, e.window, false, 0, &msg));
    } else {
      LOGI("Killing window %ld", e.window);
      XKillClient(display_, e.window);
    }
  } else if ((e.state & Mod1Mask) &&
             (e.keycode == XKeysymToKeycode(display_, XK_Tab))) {
    // alt + tab: Switch window.
    // 1. Find next window.
    auto i = clients_.find(e.window);
    CHECK(i != clients_.end());
    ++i;
    if (i == clients_.end()) {
      i = clients_.begin();
    }
    // 2. Raise and set focus.
    XRaiseWindow(display_, i->second);
    XSetInputFocus(display_, i->first, RevertToPointerRoot, CurrentTime);
  }
}

void WindowManager::OnKeyRelease(const XKeyEvent& e) {}

int WindowManager::OnXError(Display* display, XErrorEvent* e) {
  const int MAX_ERROR_TEXT_LENGTH = 1024;
  char error_text[MAX_ERROR_TEXT_LENGTH];
  XGetErrorText(display, e->error_code, error_text, sizeof(error_text));
  LOGE("Received X error:\n"
       "    Request: %d - %s\n"
       "    Error code: %d - %s\n"
       "    Resource ID: %d",
       int(e->request_code), XRequestCodeToString(e->request_code).c_str(),
       int(e->error_code), error_text,
       e->resourceid);
  // The return value is ignored.
  return 0;
}

int WindowManager::OnWMDetected(Display* display, XErrorEvent* e) {
  // In the case of an already running window manager, the error code from
  // XSelectInput is BadAccess. We don't expect this handler to receive any
  // other errors.
  CHECK_EQ(static_cast<int>(e->error_code), BadAccess);
  // Set flag.
  wm_detected_ = true;
  // The return value is ignored.
  return 0;
}

struct WindowHandle *WindowManager::idToHandle(XID wid)
{
  std::lock_guard<std::mutex> lock(windowMutex);
  auto it = whandles.find(wid);
  if (it == whandles.end())
    return nullptr;
  return (*it).second;
}

void WindowManager::handleCreateWindow(struct WindowHandle *w, XID wid)
{
  LOGI("Adding translation %ld -> %p", wid, w);
  std::lock_guard<std::mutex> lock(windowMutex);
  whandles[wid] = w;
}

void WindowManager::handleDestroyWindow(struct WindowHandle *w)
{
  std::lock_guard<std::mutex> lock(windowMutex);
  auto it = std::find_if(whandles.begin(), whandles.end(),
                         [&](decltype(*whandles.end()) wh)
                         {return wh.second == w;});
  if (it == whandles.end())
    return;

  LOGI("Deleting translation for %ld -> %p", (*it).first, w);
  whandles.erase(it);
}

struct WindowHandle *WindowManager::handleQueryPointerWindow()
{
  const VRXWindow *vw = pointerWindow.window;
  if (not vw)
    return nullptr;

  // No need to lock the map, the server thread (this) is the only thread that
  // changes the map.
  // It is safe to dereference window without synchronization, it will not disappear
  // while we are executing server code
  if (windows.find(vw->xWindow) == windows.end())
    return nullptr;

  return vw->handle;
}

void WindowManager::focusMRUWindow(uint16_t num)
{
  // Take win #num in Most Recently Used list and move to front
  if (focusedWindows.size() == 0){ return; }
  if (focusedWindows.size() <= num){ return; }

  num = num % focusedWindows.size();

  auto it = focusedWindows.begin();
  while(num>0)
  {
    it++;
    --num;
  }

  auto tempWinPtr = *it;
  focusedWindows.erase(it);
  if (focusedWindows.size() != 0)   // When we re-focus the only existing window, list may be empty here
  {
    // Unfocus old front
    focusedWindows.front()->setBorderColor(display_, UNFOCUSED_BORDER_COLOR);
  }
  focusedWindows.push_front(tempWinPtr);
  focusedWindows.front()->setBorderColor(display_, FOCUSED_BORDER_COLOR);
  setFocus(focusedWindows.front()->xWindow);

  LOGI("Window focused: %p", tempWinPtr->handle);
}

void WindowManager::mapWindowAndFocus(VRXWindow * win)
{
  if (focusedWindows.size() != 0)
  {
    focusedWindows.front()->setBorderColor(display_, UNFOCUSED_BORDER_COLOR);
  }
  focusedWindows.push_front(win);
  win->setBorderColor(display_, FOCUSED_BORDER_COLOR);
  setFocus(focusedWindows.front()->xWindow);

  LOGI("Window mapped + focused: %p", win->handle);
}

void WindowManager::unmapWindow(VRXWindow * win)
{
  LOGI("Window unmapped: %p", win->handle);
  focusedWindows.remove(win);
  focusMRUWindow(0);
}

bool VRXRenderer::isFocused(const VRXWindow * win)
{
  if (wm->focusedWindows.size() == 0){ return false; }

  return win==wm->focusedWindows.front();
}

void WindowManager::setFocus(Window frame)
{
  LOGI("WindowManager::setFocus frame %ld", frame);
  Window win = 0;
  for (auto& client : clients_){
    if (client.second == frame ){
      win = client.first;
      break;
    }
  }

  if (win == 0) {
    LOGE("WindowManager::setFocus frame not found: %ld", frame);
    return;
  }

  XRaiseWindow(display_, frame);
  XSetInputFocus(display_, win, RevertToPointerRoot, CurrentTime);

  LOGI("WindowManager::setFocus frame %ld => window %ld", frame, win);
}

void WindowManager::prepareRenderWindows(std::list<struct VRXWindow *> &renderWindows)
{
  for (auto wi : windows)
    {
      VRXWindow *w = wi.second;
      if (not w->mapped)
        continue;

      // 1. Lock translation
      windowMutex.lock();

      // 2. Verify translation is still accurate
      auto it = whandles.find(w->xWindow);

      if (it == whandles.end() or (*it).second != w->handle)
        {
          // TODO: this window must be gone, it should be deleted!
          windowMutex.unlock();
          continue;
        }

      bool valid = w->updateTexture();
      windowMutex.unlock();

      if (valid)
        renderWindows.push_back(w);
    }

  updateCursorWindow(renderWindows);
}

void WindowManager::updateCursorWindow(std::list<struct VRXWindow *> &renderWindows)
{

  Vec4f intersection;
  const VRXWindow *hit = nullptr;
  bool send_event = false;
  const gvr::Mat4f &headInverse = renderer->getHeadInverse();
  Vec4f mouse_vector = MatrixVectorMul(headInverse, Vec4f{0.0f, 0.0f, -1.0f, 0.0f});

  for (auto w: renderWindows)
    {
      Vec4f window_relative_view_vector = MatrixVectorMul(w->head, mouse_vector);
      if (VRXCursor::IntersectWindow(w, window_relative_view_vector, intersection))
        {
          hit =  w;
          break;
        }
    }

  if (hit)
    {
      VRXCursor::SetCursorMatrix(MatrixMul(hit->headInverse, {1.0, 0.0, 0.0, intersection.x(),
              0.0, 1.0, 0.0, intersection.y(),
              0.0, 0.0, 1.0, -VRXWindow::DEFAULT_DISTANCE + 10.0,
              0.0, 0.0, 0.0, 1.0}));

      short int x = hit->getHalfWidth() + intersection.x();
      short int y = hit->getHalfHeight() - intersection.y();
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
      pointerWindow.x = pointerWindow.y = -DESKTOP_SIZE / 2;
    }

  if (send_event)
    {
      VRXMouseMotionEvent(pointerWindow.x + DESKTOP_SIZE / 2,
                          pointerWindow.y + DESKTOP_SIZE / 2, false);
    }
}

// TODO: This is not really thread safe. We can be sure that window list does not change
//       under our feet, but any of the matrices used in transforms change, and in particular
//       between transforms from eye space to world space and back.
QueryPointerReturn WindowManager::handleQueryPointer(struct WindowHandle *w)
{
  QueryPointerReturn r;

  auto it = std::find_if(windows.begin(), windows.end(),
                         [&](decltype(*windows.end()) win)
                         {return win.second->handle == w;});
  if (it == windows.end())
    {
      r.root_x = 0;
      r.root_y = 0;
      r.win_x = -DESKTOP_SIZE / 2;
      r.win_y = -DESKTOP_SIZE / 2;
      return r;
    }

  const VRXWindow *vw = (*it).second; // wm->windows[w];
  const gvr::Mat4f &headInverse = renderer->getHeadInverse(); // MatrixTranspose(head_view);
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
