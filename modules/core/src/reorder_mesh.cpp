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
#include <lagrange/reorder_mesh.h>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_centroid.h>
#include <lagrange/permute_facets.h>
#include <lagrange/permute_vertices.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/hash.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/tracy.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Geometry>

#include <algorithm>
#include <numeric>
#include <unordered_map>

namespace lagrange {

////////////////////////////////////////////////////////////////////////////////

namespace {

// Expands a 10-bit integer into 30 bits
// by inserting 2 zeros after each bit.
uint_fast32_t expand_bits(uint_fast32_t v)
{
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

// Calculates a 30-bit Morton code for the
// given 3D point located within the unit cube [0,1].
uint_fast32_t morton_code_3d(float x, float y, float z)
{
    x = std::min(std::max(x * 1024.0f, 0.0f), 1023.0f);
    y = std::min(std::max(y * 1024.0f, 0.0f), 1023.0f);
    z = std::min(std::max(z * 1024.0f, 0.0f), 1023.0f);
    uint_fast32_t xx = expand_bits((uint_fast32_t)x);
    uint_fast32_t yy = expand_bits((uint_fast32_t)y);
    uint_fast32_t zz = expand_bits((uint_fast32_t)z);
    return xx * 4 + yy * 2 + zz;
}

template <typename Derived>
std::vector<uint_fast32_t> morton_codes(const Eigen::MatrixBase<Derived>& vertices)
{
    const size_t num_vertices = (size_t)vertices.rows();
    const size_t num_coords = (size_t)vertices.cols();
    la_runtime_assert(num_coords == 2 || num_coords == 3);

    if (num_vertices == 0) return {};

    using Scalar = typename Derived::Scalar;
    Eigen::AlignedBox<Scalar, 3> bbox;
    for (auto p : vertices.rowwise()) {
        bbox.extend(p.transpose());
    }

    std::vector<uint_fast32_t> codes(num_vertices);
    tbb::parallel_for(size_t(0), num_vertices, [&](size_t i) {
        Eigen::RowVector3f p =
            ((vertices.row(i).transpose() - bbox.min()).array() / bbox.diagonal().array())
                .template cast<float>();
        codes[i] = morton_code_3d(p.x(), p.y(), p.z());
    });

    return codes;
}

///
/// In place conversion of Morton code to Hilbert code.
///
/// This is a parallel implementation of the algorithm described in
/// https://github.com/rawrunprotected/hilbert_curves
/// License: public domain
///
/// @param[in,out] codes  Spatial codes (input is Morton, output is Hilbert).
///
void morton_to_hilbert(std::vector<uint_fast32_t>& codes)
{
    constexpr uint32_t bits = 10;
    constexpr uint8_t m2h_table[] = {
        48, 33, 35, 26, 30, 79, 77, 44, 78, 68, 64, 50, 51, 25, 29, 63, 27, 87, 86, 74,
        72, 52, 53, 89, 83, 18, 16, 1,  5,  60, 62, 15, 0,  52, 53, 57, 59, 87, 86, 66,
        61, 95, 91, 81, 80, 2,  6,  76, 32, 2,  6,  12, 13, 95, 91, 17, 93, 41, 40, 36,
        38, 10, 11, 31, 14, 79, 77, 92, 88, 33, 35, 82, 70, 10, 11, 23, 21, 41, 40, 4,
        19, 25, 29, 47, 46, 68, 64, 34, 45, 60, 62, 71, 67, 18, 16, 49};

    tbb::parallel_for(size_t(0), codes.size(), [&](size_t k) {
        const uint32_t c = codes[k];
        uint32_t transform = 0;
        uint32_t out = 0;

        for (int32_t i = 3 * (bits - 1); i >= 0; i -= 3) {
            transform = m2h_table[transform | ((c >> i) & 7)];
            out = (out << 3) | (transform & 7);
            transform &= ~7;
        }
        codes[k] = out;
    });
}

///
/// Compute an ordering of a set of points based on Morton encoding of their coordinates.
///
/// @param[in]  vertices  Vertices to reorder.
/// @param[in]  method    Reordering method.
///
/// @return     Sorted indices for the new->old mapping.
///
template <typename Index, typename Derived>
std::vector<Index> spatial_ordering_points(
    const Eigen::MatrixBase<Derived>& vertices,
    ReorderingMethod method)
{
    std::vector<Index> indices(vertices.rows());
    std::iota(indices.begin(), indices.end(), 0);
    if (method == ReorderingMethod::Lexicographic) {
        tbb::parallel_sort(indices.begin(), indices.end(), [&](Index i, Index j) {
            return std::lexicographical_compare(
                vertices.row(i).data(),
                vertices.row(i).data() + vertices.cols(),
                vertices.row(j).data(),
                vertices.row(j).data() + vertices.cols());
        });
    } else {
        la_debug_assert(method == ReorderingMethod::Morton || method == ReorderingMethod::Hilbert);
        auto codes = morton_codes(vertices);
        if (method == ReorderingMethod::Hilbert) {
            morton_to_hilbert(codes);
        }
        tbb::parallel_sort(indices.begin(), indices.end(), [&](Index i, Index j) {
            return codes[i] < codes[j] || (codes[i] == codes[j] && i < j);
        });
    }
    return indices;
}

} // namespace

///
/// Reorder mesh vertices using Morton encoding.
///
/// @todo          Reorder mesh facets as well.
///
/// @param[in,out] mesh      Mesh to reorder.
///
/// @tparam        MeshType  Mesh type.
///
template <typename Scalar, typename Index>
void reorder_mesh(SurfaceMesh<Scalar, Index>& mesh, ReorderingMethod method)
{
    LAGRANGE_ZONE_SCOPED;
    if (mesh.has_edges()) {
        logger().warn(
            "Spatial sort will recompute edge data. Any per-edge attribute will be lost.");
    }

    if (method == ReorderingMethod::None) {
        return;
    }
    logger().debug("Mesh reordering...");

    // 1st: Permute vertices
    permute_vertices<Scalar, Index>(
        mesh,
        spatial_ordering_points<Index>(vertex_view(mesh), method));

    // 2nd: Permute facets
    auto indices = [&] {
        auto mesh_copy = mesh; // use temporary to discard intermediate facet centroids
        auto facet_centroid_ids = compute_facet_centroid(mesh_copy);
        auto facet_centroids = attribute_matrix_view<Scalar>(mesh_copy, facet_centroid_ids);
        return spatial_ordering_points<Index>(facet_centroids, method);
    }();
    permute_facets<Scalar, Index>(mesh, indices);

    logger().debug("Mesh reordering done.");
}

#define LA_X_reorder_mesh(_, Scalar, Index) \
    template LA_CORE_API void reorder_mesh<Scalar, Index>(SurfaceMesh<Scalar, Index>&, ReorderingMethod);
LA_SURFACE_MESH_X(reorder_mesh, 0)

} // namespace lagrange
