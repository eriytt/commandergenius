#pragma once

#include <memory>

#include <GLES2/gl2.h>

#include "vr/gvr/capi/include/gvr_types.h"

// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
// NOTE, 32 bit only!
inline unsigned int roundUpPow2(unsigned int v)
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

GLuint CreateTexture(int size_x, int size_y, uint8_t *buffer = nullptr);
void CheckGLError(const char* label);
int LoadGLShader(int type, const char** shadercode);
std::array<float, 16> MatrixToGLArray(const gvr::Mat4f& matrix);
