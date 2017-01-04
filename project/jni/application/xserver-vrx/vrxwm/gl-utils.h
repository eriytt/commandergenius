#pragma once

#include <memory>

#include <GLES2/gl2.h>

#include "vr/gvr/capi/include/gvr_types.h"

GLuint CreateTexture(int size_x, int size_y, uint8_t *buffer = nullptr);
void CheckGLError(const char* label);
int LoadGLShader(int type, const char** shadercode);
std::array<float, 16> MatrixToGLArray(const gvr::Mat4f& matrix);
