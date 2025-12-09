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
#include <lagrange/compute_area.h>

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/internal/get_uv_attribute.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/SmallVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/quad_area.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/triangle_area.h>
#include <lagrange/utils/warning.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Geometry>

namespace lagrange {

namespace {

template <typename Scalar, typename Index>
class FacetPositionsView
{
public:
    FacetPositionsView(
        const SurfaceMesh<Scalar, Index>& mesh,
        const Index fid,
        const ConstRowMatrixView<Scalar>& vertex_positions)
        : m_vertex_positions(vertex_positions)
        , m_vertices_indices(mesh.get_facet_vertices(fid))
        , m_mesh_dim(mesh.get_dimension())
    {}

    span<const Scalar> operator()(const Index local_vertex_index) const
    {
        return {
            m_vertex_positions.data() + m_vertices_indices[local_vertex_index] * m_mesh_dim,
            static_cast<size_t>(m_mesh_dim)};
    }

private:
    const ConstRowMatrixView<Scalar>& m_vertex_positions;
    const span<const Index> m_vertices_indices;
    const Index m_mesh_dim;
};

template <typename Scalar, typename Index, int Dimension>
class FacetPositionsTransformed
{
private:
    using Position = Eigen::Vector<Scalar, Dimension>;

public:
    FacetPositionsTransformed(
        const SurfaceMesh<Scalar, Index>& mesh,
        const Index fid,
        const ConstRowMatrixView<Scalar>& vertex_positions,
        const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transformation)
        : m_transformed_positions(mesh.get_facet_size(fid))
    {
        FacetPositionsView<Scalar, Index> positions_view(mesh, fid, vertex_positions);
        for (const auto local_vertex_idx : range(m_transformed_positions.size())) {
            const auto vertex_position = positions_view(static_cast<Index>(local_vertex_idx));
            la_debug_assert(vertex_position.size() == Dimension);
            m_transformed_positions[local_vertex_idx] =
                transformation * Eigen::Map<const Position>(vertex_position.data());
        }
    }

    const span<const Scalar> operator()(const Index local_vertex_index)
    {
        const auto& position = m_transformed_positions[local_vertex_index];
        return {position.data(), static_cast<size_t>(position.size())};
    }

private:
    SmallVector<Position, 4> m_transformed_positions;
};

template <typename Scalar, typename Index, typename FacetPositions, typename... FacetPositionsArgs>
void compute_triangle_area(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    bool use_signed_area,
    FacetPositionsArgs... args)
{
    const auto num_facets = mesh.get_num_facets();
    const auto dim = mesh.get_dimension();

    auto& attr = mesh.template ref_attribute<Scalar>(id);
    la_debug_assert(static_cast<Index>(attr.get_num_elements()) == num_facets);
    la_debug_assert(static_cast<Index>(attr.get_num_channels()) == 1);
    auto attr_ref = matrix_ref(attr);

    if (dim == 3) {
        using S = span<const Scalar, 3>;
        tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
            FacetPositions p(mesh, fid, std::forward<FacetPositionsArgs>(args)...);
            attr_ref(fid) = triangle_area_3d<Scalar>(S(p(0)), S(p(1)), S(p(2)));
        });
    } else if (dim == 2) {
        using S = span<const Scalar, 2>;
        tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
            FacetPositions p(mesh, fid, std::forward<FacetPositionsArgs>(args)...);
            attr_ref(fid) = triangle_area_2d<Scalar>(S(p(0)), S(p(1)), S(p(2)));
        });
        if (!use_signed_area) attr_ref = attr_ref.array().abs();
    } else {
        // High dimensional triangle area can be computed from the edge lengths.
        // This is not implemented due to limited use cases.
        throw Error("High dimensional triangle area computation is not implemented!");
    }
}

template <typename Scalar, typename Index, typename FacetPositions, typename... FacetPositionsArgs>
void compute_quad_area(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    bool use_signed_area,
    FacetPositionsArgs... args)
{
    const auto num_facets = mesh.get_num_facets();
    const auto dim = mesh.get_dimension();

    auto& attr = mesh.template ref_attribute<Scalar>(id);
    la_debug_assert(static_cast<Index>(attr.get_num_elements()) == num_facets);
    la_debug_assert(static_cast<Index>(attr.get_num_channels()) == 1);
    auto attr_ref = matrix_ref(attr);

    if (dim == 3) {
        using S = span<const Scalar, 3>;
        tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
            FacetPositions p(mesh, fid, std::forward<FacetPositionsArgs>(args)...);
            attr_ref(fid) = quad_area_3d<Scalar>(S(p(0)), S(p(1)), S(p(2)), S(p(3)));
        });
    } else if (dim == 2) {
        using S = span<const Scalar, 2>;
        tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
            FacetPositions p(mesh, fid, std::forward<FacetPositionsArgs>(args)...);
            attr_ref(fid) = quad_area_2d<Scalar>(S(p(0)), S(p(1)), S(p(2)), S(p(3)));
        });
        if (!use_signed_area) attr_ref = attr_ref.array().abs();
    } else {
        // This is not implemented due to limited use cases.
        throw Error("High dimensional quad area computation is not implemented!");
    }
}

template <typename Scalar, typename Index, typename FacetPositions, typename... FacetPositionsArgs>
void compute_polygon_area(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    bool use_signed_area,
    FacetPositionsArgs... args)
{
    const auto num_facets = mesh.get_num_facets();
    const auto dim = mesh.get_dimension();

    auto& attr = mesh.template ref_attribute<Scalar>(id);
    la_debug_assert(static_cast<Index>(attr.get_num_elements()) == num_facets);
    la_debug_assert(static_cast<Index>(attr.get_num_channels()) == 1);
    auto attr_ref = matrix_ref(attr);

    if (dim == 3) {
        using S = span<const Scalar, 3>;
        tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
            FacetPositions p(mesh, fid, std::forward<FacetPositionsArgs>(args)...);
            const auto n = static_cast<Index>(mesh.get_facet_size(fid));
            Scalar _center[3]{0, 0, 0};
            for (Index i = 0; i < n; i++) {
                const auto position = p(i);
                _center[0] += position[0];
                _center[1] += position[1];
                _center[2] += position[2];
            }
            _center[0] /= n;
            _center[1] /= n;
            _center[2] /= n;
            span<const Scalar> center(_center, 3);
            attr_ref(fid) = 0;
            for (Index i = 0; i < n; i++) {
                Index prev = (i + n - 1) % n;
                Index curr = i;
                attr_ref(fid) += triangle_area_3d<Scalar>(S(p(prev)), S(p(curr)), S(center));
            }
        });
    } else if (dim == 2) {
        using S = span<const Scalar, 2>;
        tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
            FacetPositions p(mesh, fid, std::forward<FacetPositionsArgs>(args)...);
            const auto n = static_cast<Index>(mesh.get_facet_size(fid));
            // 2D triangle area is signed, so no need of computing polygon center.
            Scalar _O[2]{0, 0};
            span<const Scalar> O(_O, 2);
            attr_ref(fid) = 0;
            for (Index i = 0; i < n; i++) {
                Index prev = (i + n - 1) % n;
                Index curr = i;
                attr_ref(fid) += triangle_area_2d<Scalar>(S(p(prev)), S(p(curr)), S(O));
            }
        });
        if (!use_signed_area) attr_ref = attr_ref.array().abs();
    } else {
        // This is not implemented due to limited use cases.
        throw Error("High dimensional area computation is not implemented!");
    }
}

template <typename Scalar, typename Index, typename FacetPositions, typename... FacetPositionsArgs>
AttributeId compute_facet_area_impl(
    SurfaceMesh<Scalar, Index>& mesh,
    FacetAreaOptions options,
    FacetPositionsArgs... args)
{
    AttributeId id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Facet,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);

    const auto& vertex_positions = vertex_view(mesh);
    if (mesh.is_triangle_mesh()) {
        compute_triangle_area<Scalar, Index, FacetPositions>(
            mesh,
            id,
            options.use_signed_area,
            vertex_positions,
            std::forward<FacetPositionsArgs>(args)...);
    } else if (mesh.is_quad_mesh()) {
        compute_quad_area<Scalar, Index, FacetPositions>(
            mesh,
            id,
            options.use_signed_area,
            vertex_positions,
            std::forward<FacetPositionsArgs>(args)...);
    } else {
        compute_polygon_area<Scalar, Index, FacetPositions>(
            mesh,
            id,
            options.use_signed_area,
            vertex_positions,
            std::forward<FacetPositionsArgs>(args)...);
    }
    return id;
}


template <typename Scalar, typename Index, typename FacetPositions, typename... FacetPositionsArgs>
Scalar compute_mesh_area_impl(
    SurfaceMesh<Scalar, Index>& mesh,
    MeshAreaOptions options,
    FacetPositionsArgs... args)
{
    AttributeId area_id = invalid_attribute_id();
    if (!mesh.has_attribute(options.input_attribute_name)) {
        const FacetAreaOptions fa_options = {options.input_attribute_name, options.use_signed_area};
        area_id = compute_facet_area_impl<Scalar, Index, FacetPositions>(
            mesh,
            fa_options,
            std::forward<FacetPositionsArgs>(args)...);
    } else {
        area_id = mesh.get_attribute_id(options.input_attribute_name);
    }
    la_debug_assert(area_id != invalid_attribute_id());

    const auto& attr = mesh.template get_attribute<Scalar>(area_id);
    auto attr_ref = matrix_view(attr);

    const Scalar mesh_area = tbb::parallel_reduce(
        tbb::blocked_range<Index>(static_cast<Index>(0), static_cast<Index>(mesh.get_num_facets())),
        static_cast<Scalar>(0),
        [&](const tbb::blocked_range<Index>& tbb_range, Scalar area_sum) {
            for (auto fid = tbb_range.begin(); fid != tbb_range.end(); ++fid) {
                area_sum += attr_ref(fid);
            }
            return area_sum;
        },
        std::plus<Scalar>());
    return mesh_area;
}

} // namespace

template <typename Scalar, typename Index>
AttributeId compute_facet_area(SurfaceMesh<Scalar, Index>& mesh, FacetAreaOptions options)
{
    return compute_facet_area_impl<Scalar, Index, FacetPositionsView<Scalar, Index>>(mesh, options);
}

template <typename Scalar, typename Index, int Dimension>
AttributeId compute_facet_area(
    SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transformation,
    FacetAreaOptions options)
{
    return compute_facet_area_impl<
        Scalar,
        Index,
        FacetPositionsTransformed<Scalar, Index, Dimension>>(mesh, options, transformation);
}

template <typename Scalar, typename Index>
AttributeId compute_facet_vector_area(
    SurfaceMesh<Scalar, Index>& mesh,
    FacetVectorAreaOptions options)
{
    const auto dim = mesh.get_dimension();
    la_runtime_assert(dim == 3, "Facet vector area only supports 3D meshes.");
    const auto num_facets = mesh.get_num_facets();

    AttributeId id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Facet,
        AttributeUsage::Vector,
        dim,
        internal::ResetToDefault::No);
    auto vector_area = attribute_matrix_ref<Scalar>(mesh, id);
    vector_area.setZero();

    auto vertices = vertex_view(mesh);

    // Implementation is based on equation 6 from [1].
    //
    // [1]: De Goes, Fernando, Andrew Butts, and Mathieu Desbrun. "Discrete differential operators
    // on polygonal meshes." ACM Transactions on Graphics (TOG) 39.4 (2020): 110-1.
    tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
        auto f_size = mesh.get_facet_size(fid);
        auto f = mesh.get_facet_vertices(fid);
        for (Index lv = 0; lv < f_size; lv++) {
            Index lv_next = (lv + 1) % f_size;
            LA_IGNORE_ARRAY_BOUNDS_BEGIN
            vector_area.row(fid) += vertices.row(f[lv]).template head<3>().cross(
                vertices.row(f[lv_next]).template head<3>());
            LA_IGNORE_ARRAY_BOUNDS_END
        }
    });
    vector_area /= 2;

    return id;
}

template <typename Scalar, typename Index>
Scalar compute_mesh_area(const SurfaceMesh<Scalar, Index>& mesh, MeshAreaOptions options)
{
    SurfaceMesh<Scalar, Index> shallow_copy = mesh;
    return compute_mesh_area_impl<Scalar, Index, FacetPositionsView<Scalar, Index>>(
        shallow_copy,
        options);
}

template <typename Scalar, typename Index, int Dimension>
Scalar compute_mesh_area(
    const SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transformation,
    MeshAreaOptions options)
{
    SurfaceMesh<Scalar, Index> shallow_copy = mesh;
    return compute_mesh_area_impl<
        Scalar,
        Index,
        FacetPositionsTransformed<Scalar, Index, Dimension>>(shallow_copy, options, transformation);
}

template <typename Scalar, typename Index>
Scalar compute_uv_area(const SurfaceMesh<Scalar, Index>& mesh, MeshAreaOptions options)
{
#define LA_X_uv_area_dispatch(_, UVScalar)                                                \
    {                                                                                     \
        auto id = internal::get_uv_id<Scalar, Index, UVScalar>(mesh);                     \
        if (id != invalid_attribute_id()) {                                               \
            return static_cast<Scalar>(                                                   \
                compute_mesh_area(uv_mesh_view<Scalar, Index, UVScalar>(mesh), options)); \
        }                                                                                 \
    }
    LA_SURFACE_MESH_SCALAR_X(uv_area_dispatch, 0)
#undef LA_X_uv_area_dispatch
    throw Error("No suitable UV attribute found!");
}

#define LA_X_compute_facet_area(_, Scalar, Index)                              \
    template LA_CORE_API AttributeId compute_facet_area<Scalar, Index>(        \
        SurfaceMesh<Scalar, Index>&,                                           \
        FacetAreaOptions);                                                     \
    template LA_CORE_API AttributeId compute_facet_vector_area<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                           \
        FacetVectorAreaOptions);                                               \
    template LA_CORE_API Scalar compute_mesh_area<Scalar, Index>(              \
        const SurfaceMesh<Scalar, Index>&,                                     \
        MeshAreaOptions);                                                      \
    template LA_CORE_API Scalar compute_uv_area<Scalar, Index>(                \
        const SurfaceMesh<Scalar, Index>&,                                     \
        MeshAreaOptions);

#define LA_X_compute_facet_area_dim(_, Scalar, Index, Dimension)                   \
    template LA_CORE_API AttributeId compute_facet_area<Scalar, Index, Dimension>( \
        SurfaceMesh<Scalar, Index>&,                                               \
        const Eigen::Transform<Scalar, Dimension, Eigen::Affine>&,                 \
        FacetAreaOptions);                                                         \
    template LA_CORE_API Scalar compute_mesh_area<Scalar, Index, Dimension>(       \
        const SurfaceMesh<Scalar, Index>&,                                         \
        const Eigen::Transform<Scalar, Dimension, Eigen::Affine>&,                 \
        MeshAreaOptions);

#define LA_X_dimension(Data, Scalar, Index)             \
    LA_X_compute_facet_area_dim(Data, Scalar, Index, 2) \
        LA_X_compute_facet_area_dim(Data, Scalar, Index, 3)

LA_SURFACE_MESH_X(compute_facet_area, 0)
LA_SURFACE_MESH_X(dimension, 0)

} // namespace lagrange
