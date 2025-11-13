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

#include "mesh_utils.h"

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
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Dense>

namespace lagrange::texproc {

namespace {

static constexpr bool NodeAtCellCenter = true;

using MKIndex = unsigned int;

using TexelInfo = typename Texels<NodeAtCellCenter, MKIndex>::template TexelInfo<K>;

template <typename Scalar, typename Index, typename ValueType>
void position_dilation(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    const DilationOptions& options)
{
    unsigned int num_channels = static_cast<unsigned int>(texture.extent(2));
    la_runtime_assert(num_channels == 3, "Texture must have 3 channels");

    auto wrapper =
        mesh_utils::create_mesh_wrapper(mesh, RequiresIndexedTexcoords::No, CheckFlippedUV::No);

    unsigned int res[] = {
        static_cast<unsigned int>(texture.extent(0)),
        static_cast<unsigned int>(texture.extent(1))};

    Padding padding = create_padding(wrapper, res[0], res[1]);
    res[0] += padding.width();
    res[1] += padding.height();

    // The dilated active texels
    RegularGrid<K, TexelInfo> dilated_texel_info =
        Texels<NodeAtCellCenter, MKIndex>::template GetSupportedTexelInfo<Dim, false>(
            wrapper.num_simplices(),
            [&](size_t v) { return wrapper.vertex(v); },
            [&](size_t s) { return wrapper.facet_indices(s); },
            [&](size_t s) { return wrapper.vflipped_simplex_texcoords(s); },
            res,
            options.dilation_radius,
            false);

    // Sample the positions into a grid
    RegularGrid<K, Vector<float, Dim>> texture_positions =
        Texels<NodeAtCellCenter, MKIndex>::template GetTexelPositions<float, Dim>(
            wrapper.num_simplices(),
            [&](size_t s) { return wrapper.simplex_vertices(s); },
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
    using TexelData = Vector<double, static_cast<unsigned int>(-1)>;

    unsigned int num_channels = static_cast<unsigned int>(texture.extent(2));

    auto wrapper =
        mesh_utils::create_mesh_wrapper(mesh, RequiresIndexedTexcoords::No, CheckFlippedUV::No);

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
        padding = create_padding(wrapper, w, h);
        padding.pad(texture_grid);
    }

    // A functor returning the texture triangle in texture-space coordinates
    auto texture_space_simplex = [&](unsigned int si) {
        Simplex<double, K, K> s = wrapper.vflipped_simplex_texcoords(si);
        for (unsigned int k = 0; k <= K; k++) {
            for (unsigned int d = 0; d < K; d++) {
                s[k][d] *= texture_grid.res(d);
            }
        }
        return s;
    };

    // The active texels
    RegularGrid<K, TexelInfo> input_texel_info =
        Texels<NodeAtCellCenter, MKIndex>::template GetSupportedTexelInfo<Dim, false>(
            wrapper.num_simplices(),
            [&](size_t v) { return wrapper.vertex(v); },
            [&](size_t s) { return wrapper.facet_indices(s); },
            [&](size_t s) { return wrapper.vflipped_simplex_texcoords(s); },
            texture_grid.res(),
            0,
            false);

    // The dilated active texels
    RegularGrid<K, TexelInfo> dilated_texel_info =
        Texels<NodeAtCellCenter, MKIndex>::template GetSupportedTexelInfo<Dim, false>(
            wrapper.num_simplices(),
            [&](size_t v) { return wrapper.vertex(v); },
            [&](size_t s) { return wrapper.facet_indices(s); },
            [&](size_t s) { return wrapper.vflipped_simplex_texcoords(s); },
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
