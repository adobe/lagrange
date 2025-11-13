/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/constants.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/views.h>
#include <lagrange/winding/FastWindingNumber.h>

#include <UT_SolidAngle.h>

#include <vector>

namespace lagrange {

namespace winding {

struct FastWindingNumber::Impl
{
public:
    template <typename DerivedV, typename DerivedF>
    void initialize(
        const Eigen::MatrixBase<DerivedV>& vertices,
        const Eigen::MatrixBase<DerivedF>& facets)
    {
        la_runtime_assert(vertices.cols() == 3);
        la_runtime_assert(facets.cols() == 3);

        // TODO: Avoid copy if possible. For now we just copy stuff around.
        m_vertices.resize(vertices.rows());
        for (Eigen::Index v = 0; v < vertices.rows(); ++v) {
            m_vertices[v][0] = static_cast<float>(vertices(v, 0));
            m_vertices[v][1] = static_cast<float>(vertices(v, 1));
            m_vertices[v][2] = static_cast<float>(vertices(v, 2));
        }

        m_triangles.resize(facets.rows());
        for (Eigen::Index f = 0; f < facets.rows(); ++f) {
            m_triangles[f][0] = static_cast<int>(facets(f, 0));
            m_triangles[f][1] = static_cast<int>(facets(f, 1));
            m_triangles[f][2] = static_cast<int>(facets(f, 2));
        }

        const int num_vertices = static_cast<int>(m_vertices.size());
        const int num_triangles = static_cast<int>(m_triangles.size());
        const int* const triangles_ptr = reinterpret_cast<const int*>(m_triangles.data());
        m_engine.init(num_triangles, triangles_ptr, num_vertices, m_vertices.data());
    }

    bool is_inside(const std::array<float, 3>& pos) const
    {
        Vector q;
        q[0] = pos[0];
        q[1] = pos[1];
        q[2] = pos[2];
        return m_engine.computeSolidAngle(q) / (4.f * lagrange::internal::pi) > 0.5f;
    }

    float solid_angle(const std::array<float, 3>& pos) const
    {
        Vector q;
        q[0] = pos[0];
        q[1] = pos[1];
        q[2] = pos[2];
        return m_engine.computeSolidAngle(q);
    }

protected:
    using Vector = HDK_Sample::UT_Vector3T<float>;
    using Engine = HDK_Sample::UT_SolidAngle<float, float>;

    std::vector<Vector> m_vertices;
    std::vector<std::array<int, 3>> m_triangles;
    Engine m_engine;
};

template <typename Scalar, typename Index>
FastWindingNumber::FastWindingNumber(const SurfaceMesh<Scalar, Index>& mesh)
    : m_impl(make_value_ptr<Impl>())
{
    la_runtime_assert(
        mesh.get_dimension() == 3,
        "Fast winding number engine only supports 3D meshes");
    la_runtime_assert(
        mesh.is_triangle_mesh(),
        "Fast winding number engine only supports triangle meshes");
    m_impl->initialize(vertex_view(mesh), facet_view(mesh));
}

FastWindingNumber::FastWindingNumber() = default;
FastWindingNumber::~FastWindingNumber() = default;
FastWindingNumber::FastWindingNumber(FastWindingNumber&& other) noexcept = default;
FastWindingNumber& FastWindingNumber::operator=(FastWindingNumber&& other) noexcept = default;

bool FastWindingNumber::is_inside(const std::array<float, 3>& pos) const
{
    return m_impl->is_inside(pos);
}

float FastWindingNumber::solid_angle(const std::array<float, 3>& pos) const
{
    return m_impl->solid_angle(pos);
}

// Iterate over mesh (scalar, index) types
#define LA_X_fast_winding_number(_, Scalar, Index) \
    template FastWindingNumber::FastWindingNumber(const SurfaceMesh<Scalar, Index>& mesh);
LA_SURFACE_MESH_X(fast_winding_number, 0)

} // namespace winding
} // namespace lagrange
