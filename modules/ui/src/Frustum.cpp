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
#include <lagrange/ui/Frustum.h>

namespace lagrange {
namespace ui {

Frustum Frustum::transformed(const Eigen::Affine3f& T) const
{
    Frustum f = *this;
    for (auto& p : f.planes) {
        p = p.transform(T);
    }

    for (auto& v : f.vertices) {
        v = T * v.homogeneous();
    }
    return f;
}

bool Frustum::intersects(const Eigen::AlignedBox3f& bb, bool& fully_inside) const
{
    const auto& f = *this;
    int planes_in = 0;
    for (size_t i = 0; i < f.planes.size(); ++i) {
        int out = 0;
        for (auto k = 0; k < 8; k++) {
            Eigen::Vector3f corner = bb.corner(Eigen::AlignedBox3f::CornerType(k));
            out +=
                ((f.planes[i].coeffs().dot(Eigen::Vector4f(corner.x(), corner.y(), corner.z(), 1.0f)) < 0.0f)
                        ? 1
                        : 0);
        }
        if (out == 8) return false;
        if (out == 0) planes_in++;
    }
    if (planes_in == 6)
        fully_inside = true;
    else
        fully_inside = false;

    return true;
}
template <typename VA, typename VB>
bool test_axis(const VA& va, const VB& vb, const Eigen::Vector3f& n)
{
    const float eps = 1e-8f;
    float min1 = std::numeric_limits<float>::max();
    float min2 = min1;
    float max1 = std::numeric_limits<float>::lowest();
    float max2 = max1;

    for (auto a : va) {
        const float d = a.dot(n);
        min1 = std::min(min1, d);
        max1 = std::max(max1, d);
    }
    for (auto b : vb) {
        const float d = b.dot(n);
        min2 = std::min(min2, d);
        max2 = std::max(max2, d);
    }

    if (min1 > max2 + eps || max1 < min2 - eps) {
        return false;
    }
    return true;
}


bool Frustum::intersects(
    const Eigen::Vector3f& a, const Eigen::Vector3f& b, const Eigen::Vector3f& c) const
{
    const std::array<Eigen::Vector3f, 3> t = {a, b, c};

    const std::array<Eigen::Vector3f, 3> t_edges = {t[0] - t[1], t[0] - t[2], t[1] - t[2]};

    const auto test = [&](const Eigen::Vector3f& n) { return test_axis(t, vertices, n); };

    const std::array<Eigen::Vector3f, 6> f_edges = {
        get_edge(FRUSTUM_NEAR_LEFT_BOTTOM, FRUSTUM_FAR_LEFT_BOTTOM),
        get_edge(FRUSTUM_NEAR_RIGHT_BOTTOM, FRUSTUM_FAR_RIGHT_BOTTOM),
        get_edge(FRUSTUM_NEAR_LEFT_TOP, FRUSTUM_FAR_LEFT_TOP),
        get_edge(FRUSTUM_NEAR_RIGHT_TOP, FRUSTUM_FAR_RIGHT_TOP),
        get_edge(FRUSTUM_FAR_RIGHT_TOP, FRUSTUM_FAR_LEFT_TOP), // horizontal
        get_edge(FRUSTUM_FAR_RIGHT_TOP, FRUSTUM_FAR_RIGHT_BOTTOM), // vertical
    };

    const Eigen::Vector3f n = t_edges[0].cross(t_edges[1]);
    bool face_test = test(n) && test(planes[FRUSTUM_LEFT].normal()) &&
                     test(planes[FRUSTUM_BOTTOM].normal()) && test(planes[FRUSTUM_RIGHT].normal()) &&
                     test(planes[FRUSTUM_TOP].normal()) && test(planes[FRUSTUM_NEAR].normal()) &&
                     test(planes[FRUSTUM_FAR].normal());
    if (!face_test) return false;

    for (auto t_e : t_edges) {
        for (auto f : f_edges) {
            if (!test(t_e.cross(f))) return false;
        }
    }
    return true;
}

bool Frustum::intersects(const Eigen::Vector3f& a, const Eigen::Vector3f& b) const
{
    const std::array<Eigen::Vector3f, 2> t = {a, b};

    const std::array<Eigen::Vector3f, 1> t_edges = {t[1] - t[0]};

    const auto test = [&](const Eigen::Vector3f& n) { return test_axis(t, vertices, n); };

    const std::array<Eigen::Vector3f, 6> f_edges = {
        get_edge(FRUSTUM_NEAR_LEFT_BOTTOM, FRUSTUM_FAR_LEFT_BOTTOM),
        get_edge(FRUSTUM_NEAR_RIGHT_BOTTOM, FRUSTUM_FAR_RIGHT_BOTTOM),
        get_edge(FRUSTUM_NEAR_LEFT_TOP, FRUSTUM_FAR_LEFT_TOP),
        get_edge(FRUSTUM_NEAR_RIGHT_TOP, FRUSTUM_FAR_RIGHT_TOP),
        get_edge(FRUSTUM_FAR_RIGHT_TOP, FRUSTUM_FAR_LEFT_TOP), // horizontal
        get_edge(FRUSTUM_FAR_RIGHT_TOP, FRUSTUM_FAR_RIGHT_BOTTOM), // vertical
    };

    bool face_test = test(planes[FRUSTUM_LEFT].normal()) && test(planes[FRUSTUM_BOTTOM].normal()) &&
                     test(planes[FRUSTUM_RIGHT].normal()) && test(planes[FRUSTUM_TOP].normal()) &&
                     test(planes[FRUSTUM_NEAR].normal()) && test(planes[FRUSTUM_FAR].normal());
    if (!face_test) return false;

    for (auto t_e : t_edges) {
        for (auto f : f_edges) {
            if (!test(t_e.cross(f))) return false;
        }
    }
    return true;
}

bool Frustum::is_backfacing(
    const Eigen::Vector3f& a, const Eigen::Vector3f& b, const Eigen::Vector3f& c) const
{
    const Eigen::Vector3f n = (a - b).cross(a - c);
    const Eigen::Vector3f e = get_edge(FRUSTUM_NEAR_RIGHT_TOP, FRUSTUM_FAR_RIGHT_TOP);
    return n.dot(e) < 0.0f;
}

Eigen::Vector3f Frustum::get_edge(FrustumVertices a, FrustumVertices b) const
{
    return vertices[a] - vertices[b];
}

Eigen::Vector3f Frustum::get_normalized_edge(FrustumVertices a, FrustumVertices b) const
{
    return get_edge(a, b).normalized();
}


bool Frustum::contains(const Eigen::Vector3f& a) const
{
    const Eigen::Vector4f pt = {a.x(), a.y(), a.z(), 1.0f};

    for (const auto & plane : planes) {
        if (plane.coeffs().dot(pt) < 0.0f) return false;
    }
    return true;
}

} // namespace ui
} // namespace lagrange
