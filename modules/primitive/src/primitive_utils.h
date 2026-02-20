/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/Logger.h>
#include <lagrange/primitive/SemanticLabel.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/utils/assert.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

#include <algorithm>
#include <numeric>
#include <vector>

#include <Eigen/Core>

namespace lagrange::primitive {

///
/// Generate a ring of points in 2D space centered at the origin.
///
/// @param[in]  radius        The radius of the ring.
/// @param[in]  num_segments  The number of segments to generate on the ring.
/// @param[in]  angle_offset  An optional angle offset in radians to control the starting point of
///                           the ring.
///
/// @returns  A vector of points representing the ring, where each point is represented by a
/// pair of coordinates (x, y). The first point is repeated at the end to close the ring.
///
template <typename Scalar>
std::vector<Scalar> generate_ring(Scalar radius, size_t num_segments, Scalar angle_offset = 0)
{
    std::vector<Scalar> ring(num_segments * 2 + 2);
    for (size_t i = 0; i < num_segments; ++i) {
        Scalar angle =
            static_cast<Scalar>(i) * (2 * lagrange::internal::pi / num_segments) + angle_offset;
        ring[i * 2] = radius * std::cos(angle);
        ring[i * 2 + 1] = radius * std::sin(angle);
    }

    // Close the ring by repeating the first point
    ring[num_segments * 2] = ring[0];
    ring[num_segments * 2 + 1] = ring[1];

    return ring;
}

///
/// Add a semantic label attribute to a mesh.
///
/// @tparam Scalar  The scalar type used for vertex coordinates.
/// @tparam Index   The index type used for mesh connectivity.
///
/// @param[in,out] mesh   The mesh to add the semantic label to.
/// @param[in] name       The name of the attribute to create.
/// @param[in] label      The semantic label value to assign to all facets.
///
template <typename Scalar, typename Index>
void add_semantic_label(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    const SemanticLabel label)
{
    mesh.template create_attribute<uint8_t>(
        name,
        AttributeElement::Facet,
        1,
        AttributeUsage::Scalar);
    auto values = attribute_vector_ref<uint8_t>(mesh, name);
    values.setConstant(static_cast<uint8_t>(label));
}

///
/// Normalize UV coordinates of a mesh to fit within a specified range.
///
/// @tparam Scalar  The scalar type used for vertex coordinates.
/// @tparam Index   The index type used for mesh connectivity.
///
/// @param[in,out] mesh   The mesh whose UV coordinates will be normalized.
/// @param[in] min_uv     The minimum UV coordinate values after normalization.
/// @param[in] max_uv     The maximum UV coordinate values after normalization.
///
template <typename Scalar, typename Index>
void normalize_uv(
    SurfaceMesh<Scalar, Index>& mesh,
    Eigen::Matrix<Scalar, 1, 2> min_uv,
    Eigen::Matrix<Scalar, 1, 2> max_uv)
{
    auto uv_mesh = uv_mesh_ref(mesh);
    auto uvs = vertex_ref(uv_mesh);

    Eigen::Matrix<Scalar, 1, 2> bbox_min = uvs.colwise().minCoeff();
    Eigen::Matrix<Scalar, 1, 2> bbox_max = uvs.colwise().maxCoeff();

    uvs =
        (((uvs.rowwise() - bbox_min).array().rowwise() / (bbox_max - bbox_min).array()).rowwise() *
         (max_uv - min_uv).array())
            .rowwise() +
        min_uv.array();
}

///
/// Move the mesh to the specified center.
///
/// @note This function assumes the mesh is currently centered at the origin.
///
/// @param[in,out] mesh   The input mesh to be modified in place.
/// @param[in]     center The new center for the mesh.
///
template <typename Scalar, typename Index, typename ValueType>
void center_mesh(SurfaceMesh<Scalar, Index>& mesh, std::array<ValueType, 3> center)
{
    Eigen::Transform<Scalar, 3, Eigen::Affine> transform;
    transform.setIdentity();
    transform.translate(
        Eigen::Matrix<Scalar, 3, 1>(
            static_cast<Scalar>(center[0]),
            static_cast<Scalar>(center[1]),
            static_cast<Scalar>(center[2])));
    transform_mesh(mesh, transform);
}

///
/// Create a mesh from a boundary loop defined by a span of coordinates.
///
/// @tparam Scalar  The scalar type used for vertex coordinates.
/// @tparam Index   The index type used for mesh connectivity.
///
/// @param[in] boundary  A span of 2D coordinates representing the boundary loop.
/// @param[in] uv_attribute_name  The name of the UV attribute to create.
/// @param[in] normal_attribute_name  The name of the normal attribute to create.
/// @param[in] flipped  Whether the facet orientation should be flipped.
///
/// @return A SurfaceMesh representing the boundary loop as a single polygon with UV and normal
/// attributes.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> boundary_to_mesh(
    span<Scalar> boundary,
    std::string_view uv_attribute_name,
    std::string_view normal_attribute_name,
    bool flipped = false)
{
    la_debug_assert(boundary.size() % 2 == 0);

    Eigen::Map<Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>> boundary_map(
        boundary.data(),
        boundary.size() / 2,
        2);

    Index num_vertices = static_cast<Index>(boundary.size() / 2);
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(num_vertices);
    auto vertices = vertex_ref(mesh);
    vertices.setZero();
    vertices.template leftCols<2>() = boundary_map;

    std::vector<Index> indices(num_vertices);
    std::iota(indices.begin(), indices.end(), 0);
    if (flipped) {
        std::reverse(indices.begin(), indices.end());
    }
    mesh.add_polygon({indices.data(), indices.size()});

    // Generate UV coordinates
    auto uv_attr_id = mesh.template create_attribute<Scalar>(
        uv_attribute_name,
        AttributeElement::Indexed,
        2,
        AttributeUsage::UV);
    auto& uv_attr = mesh.template ref_indexed_attribute<Scalar>(uv_attr_id);
    auto& uv_values = uv_attr.values();
    auto& uv_indices = uv_attr.indices();
    uv_values.resize_elements(num_vertices);
    auto uv_indices_ref = uv_indices.ref_all();

    matrix_ref(uv_values) = vertices.template leftCols<2>();
    if (flipped) {
        matrix_ref(uv_values).col(0) *= -1; // Flip UVs if the mesh is flipped
    }
    std::copy(indices.begin(), indices.end(), uv_indices_ref.begin());

    // Generate normals
    auto normal_attr_id = mesh.template create_attribute<Scalar>(
        normal_attribute_name,
        AttributeElement::Indexed,
        3,
        AttributeUsage::Normal);
    auto& normal_attr = mesh.template ref_indexed_attribute<Scalar>(normal_attr_id);
    auto& normal_values = normal_attr.values();
    auto& normal_indices = normal_attr.indices();
    normal_values.resize_elements(1);

    matrix_ref(normal_values).row(0) << 0, 0, (flipped ? -1 : 1);
    vector_ref(normal_indices).setZero(); // All facets share the same normal

    return mesh;
}

} // namespace lagrange::primitive
