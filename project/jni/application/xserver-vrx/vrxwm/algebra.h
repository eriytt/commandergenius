#pragma once

#include <cmath>
#include <memory>

#include "vr/gvr/capi/include/gvr_types.h"
#include "common.h"

template <typename VT>
class Vec4
{
public:
  typedef VT value_type;

  std::array<VT, 4> v;

  Vec4() : v({0.0, 0.0, 0.0, 0.0}) {}
  Vec4(VT x, VT y) : v({x, y, 0.0, 0.0}) {}
  Vec4(VT x, VT y, VT z) : v({x, y, z, 0.0}) {}
  Vec4(VT x, VT y, VT z, VT g) : v({x, y, z, g}) {}
  Vec4(const Vec4<VT> &o) : v({o.x(), o.y(), o.z(), o.g()}) {}

  VT x() const {return v[0];}
  VT y() const {return v[1];}
  VT z() const {return v[2];}
  VT g() const {return v[3];}

  VT operator*(const Vec4<VT> &other) const
  {
    return x() * other.x() + y() * other.y() + z() * other.z() + g() * other.g();
  }

  Vec4<VT> operator*(VT m) const
  {
    return Vec4<VT>(x() * m, y() * m, z() * m, g() * m);
  }

  Vec4<VT> &operator*=(VT m)
  {
    v[0] *= m;
    v[1] *= m;
    v[2] *= m;
    v[3] *= m;
    return *this;
  }

  Vec4<VT> operator/(VT m) const
  {
    return Vec4<VT>(x() / m, y() / m, z() / m, g() / m);
  }

  Vec4<VT> &operator/=(VT m)
  {
    v[0] /= m;
    v[1] /= m;
    v[2] /= m;
    v[3] /= m;
    return *this;
  }

  Vec4<VT> operator+(const Vec4<VT> &other) const
  {
    return Vec4<VT>(x() + other.x(), y() + other.y(), z() + other.z(), g() + other.g());
  }

  Vec4<VT> operator-(const Vec4<VT> &other) const
  {
    return Vec4<VT>(x() - other.x(), y() - other.y(), z() - other.z(), g() - other.g());
  }

  Vec4<VT> operator-() const
  {
    return Vec4<VT>(-x(), -y(), -z(), -g());
  }

  Vec4<VT> &operator+=(const Vec4<VT> &other)
  {
    v[0] += other.v[0];
    v[1] += other.v[1];
    v[2] += other.v[2];
    v[3] += other.v[3];
    return *this;
  }

  Vec4<VT> &operator-=(const Vec4<VT> &other)
  {
    v[0] -= other.v[0];
    v[1] -= other.v[1];
    v[2] -= other.v[2];
    v[3] -= other.v[3];
    return *this;
  }

  VT len2() const {return x() * x() + y() * y() + z() * z() + g() * g();}
  VT len() const {return static_cast<VT>(sqrt(len));}

  bool isNull() const {return x() == 0.0 and y() == 0.0 and z() == 0.0 and g() == 0.0;}

  bool isZero(VT tolerance = 0.0001) const {return len2 <= tolerance;}

  Vec4<VT> &normalize()
  {
    VT l = len();
    assert(l != 0.0);
    *this /= l;
    return *this;
  }

  Vec4<VT> normalized()
  {
    VT l = len();
    assert(l != 0.0);
    return *this / l;
  }

  Vec4<VT> cross(const Vec4<VT> &other) const
  {
    Vec4<VT> res;
    res.v[0] = y() * other.z() - other.y() * z();
    res.v[1] = x() * other.z() - other.x() * z();
    res.v[1] = x() * other.y() - other.x() * y();
    return res;
  }
};

typedef Vec4<float> Vec4f;
typedef Vec4<double> Vec4d;

template <typename VT>
class Plane
{
protected:
  Vec4<VT> normal, point;

public:
  Plane(const Vec4<VT> &normal, const Vec4<VT> &point = Vec4<VT>())
    : normal(normal), point(point) {}

  // Assumes direction is normalized
  Vec4<VT> intersectLine(const Vec4<VT> &direction, const Vec4<VT> &pos = Vec4<VT>())
  {

    VT t2 = normal * direction;
    VT d = point * normal;
    VT t = -((normal * pos) - d) / t2;
    return pos + (direction * t);
  }
};

typedef Plane<float> Planef;

gvr::Mat4f MatrixMul(const gvr::Mat4f& matrix1, const gvr::Mat4f& matrix2);
gvr::Mat4f MatrixInverseRotation(const gvr::Mat4f& matrix);
gvr::Mat4f MatrixTranspose(const gvr::Mat4f& matrix);
std::array<float, 4> MatrixVectorMul(const gvr::Mat4f& matrix,
                                     const std::array<float, 4>& vec);
Vec4f MatrixVectorMul(const gvr::Mat4f& matrix, const Vec4f& vec);
