#include "vrxwindow.hh"

extern "C" {
#include <vrxexport.h>
}

#include "algebra.h"
#include "gl-utils.h"

ServerWindow::~ServerWindow()
{
  if (wmwin)
    wmwin->setGone();
}

void ServerWindow::setSize(unsigned int w, unsigned int h)
{
  std::lock_guard<std::mutex> lock(mtx);
  width = w;
  height = h;
}

void ServerWindow::setHead(const gvr::Mat4f &newhead)
{
  std::lock_guard<std::mutex> lock(mtx);
  head = newhead;
}

gvr::Mat4f ServerWindow::getCollisionParameters(unsigned int *h, unsigned int *w)
{
  std::lock_guard<std::mutex> lock(mtx);
  *w = width;
  *h = height;
  return head;
}

WmWindow::WmWindow(Window id) : id(id)
{
 windowCoords =
   {
     // Front face
     -2.0f,  2.0f,  0.0f,
     -2.0f, -2.0f,  0.0f,
      2.0f,  2.0f,  0.0f,
     -2.0f, -2.0f,  0.0f,
      2.0f, -2.0f,  0.0f,
      2.0f,  2.0f,  0.0f,
    };

}

WmWindow::WmWindow(Window id, Window child) : id(id), childId(child)
{
 windowCoords =
   {
     // Front face
     -2.0f,  2.0f,  0.0f,
     -2.0f, -2.0f,  0.0f,
      2.0f,  2.0f,  0.0f,
     -2.0f, -2.0f,  0.0f,
      2.0f, -2.0f,  0.0f,
      2.0f,  2.0f,  0.0f,
    };

}

WmWindow::~WmWindow()
{
  glDeleteTextures(1, &texId);
}

void WmWindow::updateTransform(const gvr::Mat4f &newHead, const gvr::Mat4f &newHeadInverse)
{
  gvr::Mat4f trans = {1.0f,   0.0f,    0.0f,      0.0f,
                      0.0f,   1.0,     0.0f,      0.0f,
                      0.0f,   0.0f,    1.0f, -distance,
                      0.0f,   0.0f,    0.0f,      1.0f};
  head = newHead;
  headInverse = newHeadInverse;
  modelView = MatrixMul(headInverse, trans);
  if (srvwin)
    srvwin->setHead(head);
}

void WmWindow::setSize(unsigned int w, unsigned int h)
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

  if (srvwin)
    srvwin->setSize(w, h);
}


void WmWindow::setBorderColor(Display* display, unsigned long color)
{
  XSetWindowAttributes winAttributes = {};
  winAttributes.border_pixel = color;
  
  XChangeWindowAttributes(display, id, CWBorderPixel, &winAttributes);
}

void WmWindow::updateTexCoords()
{
  float w = width / static_cast<float>(texWidth);
  float h = height / static_cast<float>(texHeight);
  texCoords = {
    0.0f, 0.0f, // v0
    0.0f,    h, // v1
    w,    0.0f, // v2
    0.0f,    h, // v1
    w,       h, // v3
    w,    0.0f, // v2
  };
}

void WmWindow::allocTexture(unsigned int w, unsigned int h)
{
  texId = CreateTexture(w, h);
  texWidth = w;
  texHeight = h;
}

void WmWindow::updateTexture(unsigned int w, unsigned int h)
{
  unsigned int rw = roundUpPow2(w);
  unsigned int rh = roundUpPow2(h);

  if (rw != texWidth or rh != texHeight)
    allocTexture(rw, rh);

  if (w != width or h != height)
    {
      setSize(w, h);
      updateTexCoords();
    }
}

bool WmWindow::updateTexture(struct WindowHandle *handle)
{
  unsigned int w, h, m;
  void *buf = VRXGetWindowBuffer(handle, &w, &h, &m);

  if (not buf)
    return false;

  if (not m)
    return false;

  updateTexture(w, h);

  glBindTexture(GL_TEXTURE_2D, texId);
  glTexSubImage2D(GL_TEXTURE_2D, 0,
                  0, 0,
                  w, h,
                  GL_RGBA, GL_UNSIGNED_BYTE,
                  buf);
  return true;
}
