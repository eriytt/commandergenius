#include "vrxwindow.hh"

#include "algebra.h"

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
