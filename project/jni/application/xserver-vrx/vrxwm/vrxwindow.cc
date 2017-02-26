#include "vrxwindow.hh"

extern "C" {
#include <vrxexport.h>
}

#include "algebra.h"
#include "gl-utils.h"

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

void VRXWindow::setBorderColor(Display* display, unsigned long color)
{
  XSetWindowAttributes winAttributes = {};
  winAttributes.border_pixel = color;
  
  XChangeWindowAttributes(display, xWindow, CWBorderPixel, &winAttributes);
}


void VRXWindow::allocTexture(unsigned int w, unsigned int h)
{
  texWidth = roundUpPow2(w);
  texHeight = roundUpPow2(h);
  texId = CreateTexture(texWidth, texHeight);
}

void VRXWindow::updateTexture(unsigned int w, unsigned int h)
{
  unsigned int rw = roundUpPow2(w);
  unsigned int rh = roundUpPow2(h);

  if (rw != texWidth or rh != texHeight)
    allocTexture(rw, rh);

  setSize(w, h);
  updateTexCoords();
}

bool VRXWindow::updateTexture(void)
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
