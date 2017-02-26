#include "algebra.h"

gvr::Mat4f MatrixMul(const gvr::Mat4f& matrix1,
		     const gvr::Mat4f& matrix2) {
  gvr::Mat4f result;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.m[i][j] = 0.0f;
      for (int k = 0; k < 4; ++k) {
        result.m[i][j] += matrix1.m[i][k]*matrix2.m[k][j];
      }
    }
  }
  return result;
}

gvr::Mat4f MatrixInverseRotation(const gvr::Mat4f& matrix)
{
  gvr::Mat4f result;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      result.m[i][j] = matrix.m[j][i];
    }
  }
  for (int i = 0; i < 3; ++i) {
    result.m[i][3] = -1*( result.m[i][0]*matrix.m[0][3] 
                        + result.m[i][1]*matrix.m[1][3]
                        + result.m[i][2]*matrix.m[2][3]);
  }
  result.m[3][3] = 1.0;
  return result;
}

gvr::Mat4f MatrixTranspose(const gvr::Mat4f& matrix)
{
  gvr::Mat4f result;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      result.m[i][j] = matrix.m[j][i];

  return result;
}

std::array<float, 4> MatrixVectorMul(const gvr::Mat4f& matrix,
                                     const std::array<float, 4>& vec)
{
  std::array<float, 4> result;
  for (int i = 0; i < 4; ++i) {
    result[i] = 0;
    for (int k = 0; k < 4; ++k) {
      result[i] += matrix.m[i][k] * vec[k];
    }
  }
  return result;
}

Vec4f MatrixVectorMul(const gvr::Mat4f& matrix, const Vec4f& vec)
{
  std::array<float, 4> r = MatrixVectorMul(matrix, vec.v);
  return Vec4f(r[0], r[1], r[2], r[3]);
}
