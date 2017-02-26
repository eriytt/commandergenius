#pragma once

#include <X11/Xlib.h>

#include "vr/gvr/capi/include/gvr.h"
#include "vr/gvr/capi/include/gvr_types.h"

#include "vrx_types.h"

struct VRXWindow
{
  static const int DEFAULT_DISTANCE = 500;

  struct WindowHandle *handle;
  Window xWindow = 0;
  gvr::Mat4f modelView;
  gvr::Mat4f head;
  gvr::Mat4f headInverse;
  unsigned int texId;
  unsigned int width, height;
  unsigned int texWidth, texHeight;
  VrxWindowCoords windowCoords;
  VrxWindowTexCoords texCoords;
  float scale = 1.0f;
  float distance = DEFAULT_DISTANCE;
  bool mapped = false;

  VRXWindow(struct WindowHandle *w, XID wid, const VrxWindowCoords& initialPosition)
    : handle(w), xWindow(wid), texId(0), width(0), height(0),
      windowCoords(initialPosition) {}

  void setSize(unsigned int w, unsigned int h)
  {
    width = w;
    height = h;
    int ws = scale * w;
    int hs = scale * h;
    windowCoords = {
      -ws / 2.0f,  hs / 2.0f,  0.0f,
      -ws / 2.0f, -hs / 2.0f,  0.0f,
       ws / 2.0f,  hs / 2.0f,  0.0f,
      -ws / 2.0f, -hs / 2.0f,  0.0f,
       ws / 2.0f, -hs / 2.0f,  0.0f,
       ws / 2.0f,  hs / 2.0f,  0.0f,
    };
  }

  void updateTexCoords()
  {
    float w = width / static_cast<float>(texWidth);
    float h = height / static_cast<float>(texHeight);
    texCoords = {
      0.0f, 0.0f, // v0
      0.0f,    h, // v1
         w, 0.0f, // v2
      0.0f,    h, // v1
         w,    h, // v3
         w, 0.0f, // v2
    };
  }

  unsigned int getWidth() const {return width;}
  unsigned int getHeight() const {return height;}
  float getHalfWidth() const {return width / 2.0f;}
  float getHalfHeight() const {return height / 2.0f;}
  void updateTransform(const gvr::Mat4f &head);
  void setBorderColor(Display* display, unsigned long color);
  void allocTexture(unsigned int w, unsigned int h);
  void updateTexture(unsigned int w, unsigned int h);
  bool updateTexture(void);
};
