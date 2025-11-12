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
#pragma once

#include "Padding.h"

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/ExactPredicatesShewchuk.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/internal/get_uv_attribute.h>
#include <lagrange/map_attribute.h>
#include <lagrange/solver/DirectSolver.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/views.h>
#include <lagrange/weld_indexed_attribute.h>

// Include before any TextureSignalProcessing header to override their threadpool implementation.
#include "ThreadPool.h"
#define MULTI_THREADING_INCLUDED
using namespace lagrange::texproc::threadpool;

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Misha/RegularGrid.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Sparse>

#include <random>

namespace lagrange::texproc {

namespace {

using namespace MishaK;

// Using `Point` directly leads to ambiguity with Apple Accelerate types.
template <typename T, unsigned N>
using Vector = MishaK::Point<T, N>;

// The dimension of the embedding space
static const unsigned int Dim = 3;

// The dimension of the manifold
static const unsigned int K = 2;

// The linear solver
using Solver = lagrange::solver::SolverLDLT<Eigen::SparseMatrix<double>>;

} // namespace

enum class RequiresIndexedTexcoords { Yes, No };
enum class CheckFlippedUV { Yes, No };

namespace mesh_utils {

template <unsigned int NumChannels, typename ValueType>
void set_grid(
    image::experimental::View3D<ValueType> texture,
    RegularGrid<K, Vector<double, NumChannels>>& grid)
{
    unsigned int num_channels = static_cast<unsigned int>(texture.extent(2));
    if (num_channels != NumChannels) la_debug_assert("Number of channels don't match");

    // Copy the texture data into the texture grid
    grid.resize(texture.extent(0), texture.extent(1));
    for (unsigned int j = 0; j < grid.res(1); j++) {
        for (unsigned int i = 0; i < grid.res(0); i++) {
            for (unsigned int c = 0; c < NumChannels; c++) {
                grid(i, j)[c] = texture(i, j, c);
            }
        }
    }
}

template <unsigned int NumChannels, typename ValueType>
void set_raw_view(
    const RegularGrid<K, Vector<double, NumChannels>>& grid,
    image::experimental::View3D<ValueType> texture)
{
    // Copy the texture grid data back into the texture
    for (unsigned int j = 0; j < grid.res(1); j++) {
        for (unsigned int i = 0; i < grid.res(0); i++) {
            for (unsigned int c = 0; c < NumChannels; c++) {
                texture(i, j, c) = grid(i, j)[c];
            }
        }
    }
}

template <typename ValueType>
void set_raw_view(
    const RegularGrid<K, double>& grid,
    image::experimental::View3D<ValueType> texture)
{
    // Copy the texture grid data back into the texture
    for (unsigned int j = 0; j < grid.res(1); j++) {
        for (unsigned int i = 0; i < grid.res(0); i++) {
            texture(i, j, 0) = grid(i, j);
        }
    }
}

template <typename Scalar, typename Index>
void check_for_flipped_uv(const SurfaceMesh<Scalar, Index>& mesh, AttributeId id)
{
    auto uv_mesh =
        [&]() -> std::pair<ConstRowMatrixView<Scalar>, std::optional<ConstRowMatrixView<Index>>> {
        if (mesh.is_attribute_indexed(id)) {
            const auto& uv_attr = mesh.template get_indexed_attribute<Scalar>(id);
            auto uv_values = matrix_view(uv_attr.values());
            auto uv_indices = reshaped_view(uv_attr.indices(), 3);
            return {uv_values, uv_indices};
        } else {
            const auto& uv_attr = mesh.template get_attribute<Scalar>(id);
            la_runtime_assert(
                uv_attr.get_element_type() == AttributeElement::Vertex ||
                    uv_attr.get_element_type() == AttributeElement::Corner,
                "UV attribute must be per-vertex or per-corner.");
            auto uv_values = matrix_view(uv_attr);
            return {
                uv_values,
                uv_attr.get_element_type() == AttributeElement::Vertex
                    ? std::nullopt
                    : std::optional<ConstRowMatrixView<Index>>(facet_view(mesh))};
        }
    }();

    auto uv_index = [&](Index f, unsigned int k) {
        if (uv_mesh.second.has_value()) {
            return (*uv_mesh.second)(f, k);
        } else {
            return f * 3 + k;
        }
    };

    ExactPredicatesShewchuk predicates;
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        Eigen::RowVector2d p0 = uv_mesh.first.row(uv_index(f, 0)).template cast<double>();
        Eigen::RowVector2d p1 = uv_mesh.first.row(uv_index(f, 1)).template cast<double>();
        Eigen::RowVector2d p2 = uv_mesh.first.row(uv_index(f, 2)).template cast<double>();
        auto r = predicates.orient2D(p0.data(), p1.data(), p2.data());
        if (r <= 0) {
            throw Error(
                fmt::format(
                    "The input mesh has flipped UVs:\n  p0=({:.3g})\n  p1=({:.3g})\n  p2=("
                    "{:.3g})\n"
                    "Please fix the input mesh before proceeding.",
                    fmt::join(p0, ", "),
                    fmt::join(p1, ", "),
                    fmt::join(p2, ", ")));
        }
    }
}

//
// Jitters texel coordinates to avoid creating rank-deficient systems when a texture vertex falls
// exactly on a texel center.
//
// Consider the case when a (boundary) texture vertex falls at integer location (i,j). The code
// "activates" all texels supported on that vertex . Depending on how you handle open/closed
// intervals (and taking into account issues of rounding), in principle you could activate any of
// the 9 texels in [i-1,i+1]x[j-1,j+1]. But of these 9 only the center one is actually supported on
// the vertex. If it is also the case that all the adjacent texture vertices are on one side, this
// could lead to problems.
//
// For example, if the vertices are all to the right of i, then the texels {i-1}x[j-1,j+1] will not
// be supported anywhere on the chart and the associated entries in its mass-matrix row will all be
// zero. And, unless that DoF is removed, this causes the linear system to be rank deficient,
// resulting in issues for the numerical factorization.
//
// This problem is removed by slightly jittering texture coordinates to move them off the texture
// lattice edges, so that a given texture vertex can be assumed to always have four well-defined
// texels supporting it.
//
// @note       Another alternative is to use a small cutoff distance to avoid activating texels that
//             have almost no support when visiting a seam texture vertex.
//
template <typename Scalar>
void jitter_texture(
    span<Scalar> texcoords_buffer,
    unsigned int width,
    unsigned int height,
    double epsilon = 1e-4)
{
    if (std::abs(epsilon) < std::numeric_limits<double>().denorm_min()) {
        return;
    }

    Scalar jitter_scale = static_cast<Scalar>(epsilon / std::max<unsigned int>(width, height));
    std::mt19937 gen;
    std::uniform_real_distribution<Scalar> dist(-jitter_scale, jitter_scale);
    for (auto& x : texcoords_buffer) {
        x += dist(gen);
    }
}

template <typename Scalar, typename Index>
struct MeshWrapper
{
    MeshWrapper(const SurfaceMesh<Scalar, Index>& mesh_)
        : mesh(mesh_)
    {}

    size_t num_simplices() const { return static_cast<size_t>(mesh.get_num_facets()); }
    size_t num_vertices() const { return static_cast<size_t>(mesh.get_num_vertices()); }
    size_t num_texcoords() const { return texcoords.size() / K; }

    Vector<double, Dim> vertex(size_t i) const
    {
        Vector<double, Dim> p;
        for (unsigned int d = 0; d < Dim; d++) {
            p[d] = static_cast<double>(vertices[i * Dim + d]);
        }
        return p;
    }

    Vector<double, K> texcoord(size_t i) const
    {
        Vector<double, K> q;
        for (unsigned int k = 0; k < K; k++) {
            q[k] = static_cast<double>(texcoords[i * K + k]);
        }
        return q;
    }

    Vector<double, K> vflipped_texcoord(size_t i) const
    {
        Vector<double, K> q;
        for (unsigned int k = 0; k < K; k++) {
            q[k] = static_cast<double>(texcoords[i * K + k]);
        }
        q[1] = 1.0 - q[1];
        return q;
    }

    int vertex_index(size_t f, unsigned int k) const
    {
        return static_cast<int>(vertex_indices[f * (K + 1) + k]);
    }

    int texture_index(size_t f, unsigned int k) const
    {
        switch (texture_element) {
        case AttributeElement::Indexed: return static_cast<int>(texture_indices[f * (K + 1) + k]);
        case AttributeElement::Vertex: return static_cast<int>(vertex_indices[f * (K + 1) + k]);
        case AttributeElement::Corner: return static_cast<int>(f * (K + 1) + k);
        default: la_debug_assert("Unsupported texture element type"); return 0;
        }
    }

    Simplex<double, K, K> simplex_texcoords(size_t f) const
    {
        Simplex<double, K, K> s;
        for (unsigned int k = 0; k <= K; k++) {
            s[k] = texcoord(texture_index(f, k));
        }
        return s;
    }

    Simplex<double, K, K> vflipped_simplex_texcoords(size_t f) const
    {
        Simplex<double, K, K> s;
        for (unsigned int k = 0; k <= K; k++) {
            s[k] = vflipped_texcoord(texture_index(f, k));
        }
        return s;
    }

    Simplex<double, Dim, K> simplex_vertices(size_t f) const
    {
        Simplex<double, Dim, K> s;
        for (unsigned int k = 0; k <= K; k++) {
            s[k] = vertex(vertex_index(f, k));
        }
        return s;
    }

    SimplexIndex<K> facet_indices(size_t f) const
    {
        SimplexIndex<K> simplex;
        for (unsigned int k = 0; k <= K; ++k) {
            simplex[k] = static_cast<int>(vertex_indices[f * (K + 1) + k]);
        }
        return simplex;
    }

    SurfaceMesh<Scalar, Index> mesh;
    span<const Scalar> vertices;
    span<Scalar> texcoords;
    span<const Index> vertex_indices;
    span<const Index> texture_indices;
    AttributeElement texture_element = AttributeElement::Value;
};

template <typename Scalar, typename Index>
MeshWrapper<Scalar, Index> create_mesh_wrapper(
    const SurfaceMesh<Scalar, Index>& mesh_in,
    RequiresIndexedTexcoords requires_indexed_texcoords,
    CheckFlippedUV check_flipped_uv)
{
    MeshWrapper wrapper(mesh_in);
    SurfaceMesh<Scalar, Index>& _mesh = wrapper.mesh;

    triangulate_polygonal_facets(_mesh);

    // Get the texcoord id (and set the texcoords if they weren't already)
    AttributeId texcoord_id;

    // If the mesh comes with UVs
    if (auto res = find_matching_attribute(_mesh, AttributeUsage::UV)) {
        texcoord_id = res.value();
    } else {
        la_runtime_assert(false, "Requires uv coordinates.");
    }
    // Make sure the UV coordinate type is the same as that of the vertices
    if (!_mesh.template is_attribute_type<Scalar>(texcoord_id)) {
        logger().warn(
            "Input uv coordinates do not have the same scalar type as the input points. Casting "
            "attribute.");
        texcoord_id = cast_attribute_in_place<Scalar>(_mesh, texcoord_id);
    }

    // Make sure the UV coordinates are indexed
    if (requires_indexed_texcoords == RequiresIndexedTexcoords::Yes &&
        _mesh.get_attribute_base(texcoord_id).get_element_type() != AttributeElement::Indexed) {
        logger().warn("UV coordinates are not indexed. Welding.");
        texcoord_id = map_attribute_in_place(_mesh, texcoord_id, AttributeElement::Indexed);
        weld_indexed_attribute(_mesh, texcoord_id);
    }

    // Make sure that the number of corners is equal to (K+1) time sthe number of simplices
    la_runtime_assert(
        _mesh.get_num_corners() == _mesh.get_num_facets() * (K + 1),
        "Numer of corners doesn't match the number of simplices");

    if (check_flipped_uv == CheckFlippedUV::Yes) {
        check_for_flipped_uv(_mesh, texcoord_id);
    }

    wrapper.vertices = _mesh.get_vertex_to_position().get_all();
    wrapper.vertex_indices = _mesh.get_corner_to_vertex().get_all();
    if (_mesh.is_attribute_indexed(texcoord_id)) {
        auto& uv_attr = _mesh.template ref_indexed_attribute<Scalar>(texcoord_id);
        wrapper.texcoords = uv_attr.values().ref_all();
        wrapper.texture_indices = uv_attr.indices().get_all();
        wrapper.texture_element = AttributeElement::Indexed;
    } else {
        auto& uv_attr = _mesh.template ref_attribute<Scalar>(texcoord_id);
        wrapper.texcoords = uv_attr.ref_all();
        wrapper.texture_indices = {};
        wrapper.texture_element = uv_attr.get_element_type();
    }

    return wrapper;
}

// Pad input texture to ensure that texture coordinates fall within the rectangle defined by the
// _centers_ of the corner texels.
template <typename Scalar, typename Index>
Padding create_padding(MeshWrapper<Scalar, Index>& wrapper, unsigned int width, unsigned int height)
{
    static_assert(sizeof(std::array<Scalar, 2>) == 2 * sizeof(Scalar));
    span<std::array<Scalar, 2>> texcoords(
        reinterpret_cast<std::array<Scalar, 2>*>(wrapper.texcoords.data()),
        wrapper.num_texcoords());
    Padding padding;
    padding = Padding::init<Scalar>(width, height, texcoords);
    padding.pad(width, height, texcoords);
    return padding;
}

} // namespace mesh_utils

} // namespace lagrange::texproc
