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

#include <lagrange/texproc/geodesic_dilation.h>

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/map_attribute.h>
#include <lagrange/triangulate_polygonal_facets.h>

// Include before any TextureSignalProcessing header to override their threadpool implementation.
#include "ThreadPool.h"
#define MULTI_THREADING_INCLUDED
using namespace lagrange::texproc::threadpool;

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Misha/Texels.h>
#include <Misha/RegularGrid.h>
#include <Src/Padding.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Dense>

namespace lagrange::texproc {

namespace {

using namespace MishaK;
using namespace MishaK::TSP;

// Using `Point` directly leads to ambiguity with Apple Accelerate types.
template <typename T, unsigned N>
using Vector = MishaK::Point<T, N>;

// The dimension of the manifold.
static const unsigned int K = 2;

// The dimension of the space into which the manifold is embedded.
static const unsigned int Dim = 3;

static constexpr bool NodeAtCellCenter = true;

using MKIndex = unsigned int;

using TexelInfo = typename Texels<NodeAtCellCenter, MKIndex>::template TexelInfo<K>;

using Real = double;

template <typename Scalar, typename Index>
void get_mesh(
    const SurfaceMesh<Scalar, Index>& t_mesh,
    std::vector<SimplexIndex<K>>& simplices,
    std::vector<Vector<Real, Dim>>& vertices,
    std::vector<Vector<Real, K>>& texcoords,
    AttributeId texcoord_id)
{
    simplices.resize(t_mesh.get_num_facets());
    vertices.resize(t_mesh.get_num_vertices());
    texcoords.resize(t_mesh.get_num_corners());

    auto vertex_indices = t_mesh.get_corner_to_vertex().get_all();
    for (unsigned int i = 0; i < t_mesh.get_num_facets(); i++) {
        for (unsigned int k = 0; k <= K; k++) {
            simplices[i][k] = static_cast<int>(vertex_indices[i * (K + 1) + k]);
        }
    }

    // Retrieve input vertex buffer
    auto& input_coords = t_mesh.get_vertex_to_position();
    la_runtime_assert(
        input_coords.get_num_elements() == t_mesh.get_num_vertices(),
        "Number of vertices should match number of vertices");

    // Retrieve input texture-coordinate buffer
    auto& input_texcoords = t_mesh.template get_attribute<Scalar>(texcoord_id);
    la_runtime_assert(
        input_texcoords.get_num_channels() == 2,
        "Input texture coordinates should only have 2 channels");
    la_runtime_assert(
        input_texcoords.get_num_elements() == t_mesh.get_num_corners(),
        "Number of texture coordinates should match number of corners");

    auto _input_coords = input_coords.get_all();
    auto _input_texcoords = input_texcoords.get_all();

    for (unsigned int i = 0; i < t_mesh.get_num_vertices(); i++) {
        for (unsigned int d = 0; d < Dim; d++) {
            vertices[i][d] = static_cast<Real>(_input_coords[i * Dim + d]);
        }
    }
    for (unsigned int i = 0; i < t_mesh.get_num_corners(); i++) {
        for (unsigned int k = 0; k < K; k++) {
            texcoords[i][k] = static_cast<Real>(_input_texcoords[i * K + k]);
        }
        texcoords[i][1] = 1. - texcoords[i][1];
    }
}

template <typename Scalar, typename Index, typename ValueType>
void position_dilation(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    const DilationOptions& options)
{
    la_runtime_assert(texture.extent(2) == 3, "Texture must have 3 channels");

    SurfaceMesh<Scalar, Index> _mesh = mesh;
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

    // Make sure the UV coordinates are associated with the corners
    if (_mesh.get_attribute_base(texcoord_id).get_element_type() != AttributeElement::Corner) {
        logger().debug("UV coordinates are not associated with the corners. Mapping to corners.");
        texcoord_id = map_attribute(_mesh, texcoord_id, "new_texture", AttributeElement::Corner);
    }

    unsigned int num_channels = static_cast<unsigned int>(texture.extent(2));

    std::vector<SimplexIndex<K>> simplices;
    std::vector<Vector<Real, Dim>> vertices;
    std::vector<Vector<Real, K>> texcoords;

    get_mesh(_mesh, simplices, vertices, texcoords, texcoord_id);

    unsigned int res[] = {
        static_cast<unsigned int>(texture.extent(0)),
        static_cast<unsigned int>(texture.extent(1))};

    Padding padding = Padding::Init(res[0], res[1], texcoords);
    padding.pad(res[0], res[1], texcoords);
    res[0] += padding.width();
    res[1] += padding.height();

    // A functor return the texture triangle in normalized coordinates
    auto normalized_simplex = [&](size_t sIdx) {
        Simplex<double, K, K> simplex;
        for (unsigned int k = 0; k <= K; k++) {
            simplex[k] = texcoords[sIdx * (K + 1) + k];
        }
        return simplex;
    };

    // A functor returning the embedded simplex
    auto simplex_embedding_functor = [&](size_t sIdx) {
        Simplex<double, Dim, K> s;
        for (unsigned int i = 0; i < 3; i++) {
            s[i] = vertices[simplices[sIdx][i]];
        }
        return s;
    };

    // The dilated active texels
    RegularGrid<K, TexelInfo> dilated_texel_info =
        Texels<NodeAtCellCenter, MKIndex>::template GetSupportedTexelInfo<Dim, false>(
            simplices.size(),
            [&](size_t v) { return vertices[v]; },
            [&](size_t s) { return simplices[s]; },
            normalized_simplex,
            res,
            options.dilation_radius,
            false);

    // Sample the positions into a grid
    RegularGrid<K, Vector<float, Dim>> texture_positions =
        Texels<NodeAtCellCenter, MKIndex>::template GetTexelPositions<float, Dim>(
            simplices.size(),
            simplex_embedding_functor,
            dilated_texel_info);

    // Undo padding
    padding.unpad(dilated_texel_info);
    padding.unpad(texture_positions);

    // Set the dilated texel values
    // ThreadPool::ParallelFor(0, dilated_texel_info.size(), [&](unsigned int, size_t i) {
    for (unsigned int j = 0; j < texture_positions.res(1); j++) {
        for (unsigned int i = 0; i < texture_positions.res(0); i++) {
            if (dilated_texel_info(i, j).sIdx != ~0u) {
                for (unsigned int c = 0; c < num_channels; c++) {
                    texture(i, j, c) = texture_positions(i, j)[c];
                }
            }
        }
    }
    // });
}

template <typename Scalar, typename Index, typename ValueType>
void texture_dilation(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    const DilationOptions& options)
{
    SurfaceMesh<Scalar, Index> _mesh = mesh;
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

    // Make sure the UV coordinates are associated with the corners
    if (_mesh.get_attribute_base(texcoord_id).get_element_type() != AttributeElement::Corner) {
        logger().debug("UV coordinates are not associated with the corners. Mapping to corners.");
        texcoord_id = map_attribute(_mesh, texcoord_id, "new_texture", AttributeElement::Corner);
    }

    using TexelData = Vector<double, static_cast<unsigned int>(-1)>;

    unsigned int num_channels = static_cast<unsigned int>(texture.extent(2));

    std::vector<SimplexIndex<K>> simplices;
    std::vector<Vector<Real, Dim>> vertices;
    std::vector<Vector<Real, K>> texcoords;

    get_mesh(_mesh, simplices, vertices, texcoords, texcoord_id);

    // Copy the texture data into the texture grid
    RegularGrid<K, TexelData> texture_grid;
    texture_grid.resize(texture.extent(0), texture.extent(1));
    for (unsigned int j = 0; j < texture_grid.res(1); j++) {
        for (unsigned int i = 0; i < texture_grid.res(0); i++) {
            texture_grid(i, j) = TexelData(num_channels);
            for (unsigned int c = 0; c < num_channels; c++) {
                texture_grid(i, j)[c] = texture(i, j, c);
            }
        }
    }

    Padding padding;
    {
        unsigned int w = static_cast<unsigned int>(texture.extent(0));
        unsigned int h = static_cast<unsigned int>(texture.extent(1));
        padding = Padding::Init(w, h, texcoords);
        padding.pad(static_cast<int>(w), static_cast<int>(h), texcoords);
        padding.pad(texture_grid);
    }

    // A functor return the texture triangle in normalized coordinates
    auto normalized_simplex = [&](size_t sIdx) {
        Simplex<double, K, K> simplex;
        for (unsigned int k = 0; k <= K; k++) {
            simplex[k] = texcoords[sIdx * (K + 1) + k];
        }
        return simplex;
    };

    // A functor returning the texture triangle in texture-space coordinates
    auto texture_space_simplex = [&](unsigned int si) {
        Simplex<double, K, K> s = normalized_simplex(si);
        for (unsigned int k = 0; k <= K; k++)
            for (unsigned int d = 0; d < K; d++) s[k][d] *= texture_grid.res(d);
        return s;
    };

    // The active texels
    RegularGrid<K, TexelInfo> input_texel_info =
        Texels<NodeAtCellCenter, MKIndex>::template GetSupportedTexelInfo<Dim, false>(
            simplices.size(),
            [&](size_t v) { return vertices[v]; },
            [&](size_t s) { return simplices[s]; },
            normalized_simplex,
            texture_grid.res(),
            0,
            false);

    // The dilated active texels
    RegularGrid<K, TexelInfo> dilated_texel_info =
        Texels<NodeAtCellCenter, MKIndex>::template GetSupportedTexelInfo<Dim, false>(
            simplices.size(),
            [&](size_t v) { return vertices[v]; },
            [&](size_t s) { return simplices[s]; },
            normalized_simplex,
            texture_grid.res(),
            options.dilation_radius,
            false);


    // A functor returning the texture space coordinate associated to a texel
    auto sample_position = [&](TexelInfo ti) { return texture_space_simplex(ti.sIdx)(ti.bc); };

    // A functor returning the texture value at a texel
    auto sample_value = [&](TexelInfo ti) { return texture_grid(sample_position(ti)); };

    // Set the dilated texel values
    // ThreadPool::ParallelFor(0, dilated_texel_info.size(), [&](unsigned int, size_t i) {
    for (size_t i = 0; i < dilated_texel_info.size(); i++) {
        if (dilated_texel_info[i].sIdx != ~0u && input_texel_info[i].sIdx == ~0u) {
            texture_grid[i] = sample_value(dilated_texel_info[i]);
        }
    }
    // });

    // Undo padding
    padding.unpad(texture_grid);

    // Copy the texture grid data back into the texture
    for (unsigned int j = 0; j < texture_grid.res(1); j++) {
        for (unsigned int i = 0; i < texture_grid.res(0); i++) {
            for (unsigned int c = 0; c < num_channels; c++) {
                texture(i, j, c) = texture_grid(i, j)[c];
            }
        }
    }
}

} // namespace

template <typename Scalar, typename Index, typename ValueType>
void geodesic_dilation(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    const DilationOptions& options)
{
    if (options.output_position_map) {
        position_dilation(mesh, texture, options);
    } else {
        texture_dilation(mesh, texture, options);
    }
}

#define LA_X_geodesic_dilation(ValueType, Scalar, Index) \
    template void geodesic_dilation(                     \
        const SurfaceMesh<Scalar, Index>& mesh,          \
        image::experimental::View3D<ValueType> texture,  \
        const DilationOptions& options);
#define LA_X_geodesic_dilation_aux(_, ValueType) LA_SURFACE_MESH_X(geodesic_dilation, ValueType)
LA_ATTRIBUTE_X(geodesic_dilation_aux, 0)

} // namespace lagrange::texproc
