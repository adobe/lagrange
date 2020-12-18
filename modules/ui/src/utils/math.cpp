/*
 * Copyright 2020 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/ui/utils/math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lagrange {
namespace ui {


float eval_minor(const Eigen::Matrix4f& m, int r0, int r1, int r2, int c0, int c1, int c2)
{
    return m(4 * r0 + c0) * (m(4 * r1 + c1) * m(4 * r2 + c2) - m(4 * r2 + c1) * m(4 * r1 + c2)) -
           m(4 * r0 + c1) * (m(4 * r1 + c0) * m(4 * r2 + c2) - m(4 * r2 + c0) * m(4 * r1 + c2)) +
           m(4 * r0 + c2) * (m(4 * r1 + c0) * m(4 * r2 + c1) - m(4 * r2 + c0) * m(4 * r1 + c1));
}


Eigen::Matrix4f normal_matrix(const Eigen::Affine3f& transform)
{
    Eigen::Matrix4f result;
    result(0) = eval_minor(transform.matrix(), 1, 2, 3, 1, 2, 3);
    result(1) = -eval_minor(transform.matrix(), 1, 2, 3, 0, 2, 3);
    result(2) = eval_minor(transform.matrix(), 1, 2, 3, 0, 1, 3);
    result(3) = -eval_minor(transform.matrix(), 1, 2, 3, 0, 1, 2);
    result(4) = -eval_minor(transform.matrix(), 0, 2, 3, 1, 2, 3);
    result(5) = eval_minor(transform.matrix(), 0, 2, 3, 0, 2, 3);
    result(6) = -eval_minor(transform.matrix(), 0, 2, 3, 0, 1, 3);
    result(7) = eval_minor(transform.matrix(), 0, 2, 3, 0, 1, 2);
    result(8) = eval_minor(transform.matrix(), 0, 1, 3, 1, 2, 3);
    result(9) = -eval_minor(transform.matrix(), 0, 1, 3, 0, 2, 3);
    result(10) = eval_minor(transform.matrix(), 0, 1, 3, 0, 1, 3);
    result(11) = -eval_minor(transform.matrix(), 0, 1, 3, 0, 1, 2);
    result(12) = -eval_minor(transform.matrix(), 0, 1, 2, 1, 2, 3);
    result(13) = eval_minor(transform.matrix(), 0, 1, 2, 0, 2, 3);
    result(14) = -eval_minor(transform.matrix(), 0, 1, 2, 0, 1, 3);
    result(15) = eval_minor(transform.matrix(), 0, 1, 2, 0, 1, 2);
    return result;
}

Eigen::Projective3f perspective(float fovy, float aspect, float zNear, float zFar)
{
    float tanHalfFovy = std::tan(fovy / 2.0f);

    Eigen::Matrix4f result = Eigen::Matrix4f::Zero();

    result(0, 0) = 1.0f / (aspect * tanHalfFovy);
    result(1, 1) = 1.0f / (tanHalfFovy);
    result(2, 2) = -(zFar + zNear) / (zFar - zNear);
    result(3, 2) = -1.0f;
    result(2, 3) = -(2.0f * zFar * zNear) / (zFar - zNear);

    return Eigen::Projective3f(result);
}

Eigen::Matrix4f look_at(
    const Eigen::Vector3f& eye, const Eigen::Vector3f& center, const Eigen::Vector3f& up)
{
    const Eigen::Vector3f f = (center - eye).normalized();
    const Eigen::Vector3f s = f.cross(up).normalized();
    const Eigen::Vector3f u = s.cross(f);

    Eigen::Matrix4f result = Eigen::Matrix4f::Identity();
    result(0, 0) = s.x();
    result(0, 1) = s.y();
    result(0, 2) = s.z();
    result(1, 0) = u.x();
    result(1, 1) = u.y();
    result(1, 2) = u.z();
    result(2, 0) = -f.x();
    result(2, 1) = -f.y();
    result(2, 2) = -f.z();
    result(0, 3) = -s.dot(eye);
    result(1, 3) = -u.dot(eye);
    result(2, 3) = f.dot(eye);
    return result;
}

Eigen::Projective3f ortho(float left, float right, float bottom, float top, float zNear, float zFar)
{
    Eigen::Matrix4f result = Eigen::Matrix4f::Identity();
    result(0, 0) = 2.0f / (right - left);
    result(1, 1) = 2.0f / (top - bottom);
    result(2, 2) = -2.0f / (zFar - zNear);
    result(0, 3) = -(right + left) / (right - left);
    result(1, 3) = -(top + bottom) / (top - bottom);
    result(2, 3) = -(zFar + zNear) / (zFar - zNear);
    return Eigen::Projective3f(result);
}

Eigen::Vector3f unproject_point(const Eigen::Vector3f& v,
    const Eigen::Matrix4f& view,
    const Eigen::Matrix4f& perspective,
    const Eigen::Vector4f& viewport)
{
    Eigen::Vector4f tmp = Eigen::Vector4f(v.x(), v.y(), v.z(), 1.0f);
    tmp.x() = (tmp.x() - viewport(0)) / viewport(2);
    tmp.y() = (tmp.y() - viewport(1)) / viewport(3);
    tmp = tmp * 2.0f - Eigen::Vector4f::Ones();

    Eigen::Vector4f obj = (perspective * view).inverse() * tmp;
    obj *= 1.0f / obj.w();

    return obj.head<3>();
}

float pi()
{
    return float(M_PI);
}

float two_pi()
{
    return 2.0f * pi();
}

Eigen::Vector3f vector_projection(const Eigen::Vector3f& u, const Eigen::Vector3f& v)
{
    return u.dot(v) / v.dot(v) * v;
}

float vector_angle(const Eigen::Vector3f& a, const Eigen::Vector3f& b)
{
    return std::acos(std::min(std::max(a.dot(b), -1.0f), 1.0f));
}

} // namespace ui
} // namespace lagrange
