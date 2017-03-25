#pragma once
#include <mutex>

#include <X11/Xlib.h>

#include "vr/gvr/capi/include/gvr.h"
#include "vr/gvr/capi/include/gvr_types.h"

#include "vrx_types.h"

class WmWindow;

class ServerWindow
{
private:
  struct WindowHandle *handle;
  Window id;
  std::mutex mtx;
  gvr::Mat4f head;
  unsigned int width = 0, height = 0;  

public:
  WmWindow *wmwin = nullptr;

public:
  ServerWindow(struct WindowHandle *handle, Window id)
    : handle(handle), id (id) {}
  ~ServerWindow();
  struct WindowHandle * getHandle() const {return handle;}
  Window getId() const {return id;}
  void setHead(const gvr::Mat4f &newhead);
  void setSize(unsigned int h, unsigned int w);
  gvr::Mat4f getCollisionParameters(unsigned int *h, unsigned int *w);
};

class WmWindow
{
public:
  static const int DEFAULT_DISTANCE = 500;

private:
  Window id;
  bool mapped = false;
  bool gone = false;
  friend class ServerContext;

public:
  void setGone() {gone = true; srvwin = nullptr;}

public:
  ServerWindow *srvwin = nullptr;
  VrxWindowCoords windowCoords;
  VrxWindowTexCoords texCoords;
  unsigned int texId = 0;
  gvr::Mat4f modelView;

private:
  unsigned int width = 0, height = 0;
  unsigned int texWidth = 0, texHeight = 0;
  float scale = 1.0f;
  float distance = DEFAULT_DISTANCE;
  gvr::Mat4f head;
  gvr::Mat4f headInverse;

  void allocTexture(unsigned int w, unsigned int h);

public:
  WmWindow(Window id);
  void setMapped(bool m) {mapped = m;}
  bool getMapped() const {return mapped;}
  bool isGone() const {return gone;}

  Window getId() const {return id;}
  void updateTransform(const gvr::Mat4f &newhead, const gvr::Mat4f &newHeadInverse);
  void updateTexture(unsigned int w, unsigned int h);
  bool updateTexture(struct WindowHandle *handle);

  void setBorderColor(Display* display, unsigned long color);
  unsigned int getWidth() const {return width;}
  unsigned int getHeight() const {return height;}
  float getScale() const {return scale;}
  void addScale(float diff) {scale += diff; setSize(width, height);}
  float getDistance() const {return distance;}
  void addDistance(float diff) {distance += diff; updateTransform(head, headInverse);}
  const gvr::Mat4f &getHead() const {return head;};
  const gvr::Mat4f &getHeadInverse() const {return headInverse;};  

  void updateTexCoords();
  void setSize(unsigned int w, unsigned int h);
  float getHalfWidth() const {return width / 2.0f;}
  float getHalfHeight() const {return height / 2.0f;}
};
