#pragma once

#include <memory>

#include "vr/gvr/capi/include/gvr_types.h"

#include "algebra.h"

class VRXCursor
{
private:
  static const unsigned short texsize = 16;
  static const float distance;

  static const char *vertexShader;

  static const char *fragmentShader;

private:
  static unsigned int texId;
  static std::array<float, 18> coords;
  static std::array<float, 12> texcoords;
  static gvr::Mat4f modelview;
  static unsigned int program;
  static int MVP_param;
  static int tex_param;
  static int pos_param;

public:
  static bool InitGL();
  static void SetCursorPosition(const Vec4f &newpos);
  static void Draw(const gvr::Mat4f &perspective);
};
