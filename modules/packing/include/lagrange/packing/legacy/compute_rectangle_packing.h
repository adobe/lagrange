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
#pragma once

// Third party rectangle bin pack
// clang-format off
#include <lagrange/utils/warnoff.h>
#include <RectangleBinPack/GuillotineBinPack.h>
#include <RectangleBinPack/Rect.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/Logger.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/DisjointSets.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/timing.h>
#include <lagrange/utils/warning.h>

namespace lagrange {
namespace packing {
LAGRANGE_LEGACY_INLINE
namespace legacy {

namespace internal {

template <typename T>
LA_NOSANITIZE_SIGNED_INTEGER_OVERFLOW bool product_will_overflow(volatile T x, volatile T y)
{
    // Note that volatile is needed to avoid compiler optimization.
    if (x == 0) return false;
    return ((x * y) / x) != y;
}
} // namespace internal

class PackingFailure : public std::runtime_error
{
public:
    PackingFailure(const std::string& what)
        : std::runtime_error(what)
    {}
};

template <typename Scalar>
std::tuple<Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>, std::vector<bool>> pack_boxes(
    const Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>& bbox_mins,
    const Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>& bbox_maxs,
    bool allow_flip,
    int margin = 2)
{
#ifdef RECTANGLE_BIN_PACK_OSS
    using Int = int;
#else
    using Int = ::rbp::Int;
#endif

    la_runtime_assert(bbox_mins.rows() == bbox_maxs.rows());
    const auto num_boxes = bbox_mins.rows();
    if (num_boxes == 0) {
        return std::make_tuple(
            Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>(0, 2),
            std::vector<bool>());
    }

    const Scalar max_box_length = (bbox_maxs - bbox_mins).maxCoeff();
    constexpr Int MAX_AREA = std::numeric_limits<Int>::max();
    constexpr Int RESOLUTION = 1 << 12;
    Int max_canvas_size = RESOLUTION;
    Int min_canvas_size = RESOLUTION;
    const Scalar scale =
        max_box_length > safe_cast<Scalar>(1e-12) ? max_box_length / max_canvas_size : 1;
    la_runtime_assert(std::isfinite(scale));
    assert(internal::product_will_overflow<Int>(2, MAX_AREA));

    std::vector<::rbp::RectSize> boxes;
    boxes.reserve(num_boxes);
    for (auto i : range(num_boxes)) {
        boxes.emplace_back();
        auto& box = boxes.back();
        box.width = safe_cast<Int>(std::ceil((bbox_maxs(i, 0) - bbox_mins(i, 0)) / scale)) + margin;
        box.height =
            safe_cast<Int>(std::ceil((bbox_maxs(i, 1) - bbox_mins(i, 1)) / scale)) + margin;
        assert(!internal::product_will_overflow<Int>(box.width, box.height));
    }

    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> centers(num_boxes, 2);
    std::vector<bool> flipped(num_boxes);

    auto pack = [&](int L, bool trial) -> bool {
        assert(!internal::product_will_overflow<Int>(L, L));
#ifdef RECTANGLE_BIN_PACK_OSS
        if (!allow_flip) {
            logger().warn(
                "Disabling rotation is not supported with this version of RectangleBinPack!");
        }
        rbp::GuillotineBinPack packer(L, L);
#else
        rbp::GuillotineBinPack packer(L, L, allow_flip);
#endif
        for (auto i : range(num_boxes)) {
            auto rect = packer.Insert(
                boxes[i].width,
                boxes[i].height,
                false, // Perform empty space merging for defragmentation.
                rbp::GuillotineBinPack::FreeRectChoiceHeuristic::RectBestAreaFit,
                rbp::GuillotineBinPack::GuillotineSplitHeuristic::SplitMinimizeArea);
            if (rect.width == 0 || rect.height == 0) {
                return false;
            }
        }
        const auto& packed_rect = packer.GetUsedRectangles();
        logger().debug("num packed rectangles {}, expecting {}", packed_rect.size(), num_boxes);
        if (static_cast<decltype(num_boxes)>(packed_rect.size()) != num_boxes) return false;

        if (!trial) {
            for (auto i : range(num_boxes)) {
                const auto& r = packed_rect[i];
                flipped[i] = r.width != boxes[i].width;
                assert(allow_flip || !flipped[i]);
                centers(i, 0) = (r.x + r.width * 0.5f) * scale;
                centers(i, 1) = (r.y + r.height * 0.5f) * scale;
            }
        }
        return true;
    };

    // Find max_canvas_size large enough to fit all boxes.
    while (!pack(max_canvas_size, true)) {
        min_canvas_size = max_canvas_size;
        max_canvas_size *= 2;
        if (MAX_AREA / max_canvas_size <= max_canvas_size) {
            // Ops, run out of bound.
            throw PackingFailure("Cannot pack even with canvas at max area!");
        }
    }
    la_runtime_assert(max_canvas_size > 0);
    // Binary search for the smallest max_canvas_size that fits.
    while (min_canvas_size < max_canvas_size) {
        const auto l = (min_canvas_size + max_canvas_size) / 2;
        if (l == min_canvas_size) {
            break;
        }
        if (pack(l, true)) {
            max_canvas_size = l;
        } else {
            min_canvas_size = l;
        }
    }
    la_runtime_assert(max_canvas_size > 0);
    bool r = pack(max_canvas_size, false);
    la_runtime_assert(r);

    return std::make_tuple(centers, flipped);
}

/**
 * PackingOptions allows one to customize packing options.
 */
struct PackingOptions
{
#ifndef RECTANGLE_BIN_PACK_OSS
    bool allow_flip = true; ///< Whether to allow box to rotate by 90 degree when packing.
#endif
    bool normalize = true; ///< Should the output be normalized to fit into a unit box.
    int margin = 2; ///< Minimum allowed distance (kind of) between two boxes.
};

/**
 * Pack UV charts of a given mesh.
 *
 * @param[in,out] mesh     The mesh with UV initialized.
 * @param[in]     options  The packing options.
 *
 * @tparam        MeshType The Mesh type.
 *
 * The UV of `mesh` will be updated as a result.
 */
template <typename MeshType>
void compute_rectangle_packing(MeshType& mesh, const PackingOptions& options)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input is not a Mesh object");
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    VerboseTimer timer;
    timer.tick();

    auto uv_mesh = mesh.get_uv_mesh();
    auto uvs = uv_mesh->get_vertices(); // Copied intentionally.
    const auto& facets = uv_mesh->get_facets();
    const auto num_vertices = uv_mesh->get_num_vertices();
    const auto num_facets = uv_mesh->get_num_facets();
    const auto vertex_per_facet = uv_mesh->get_vertex_per_facet();

    DisjointSets<Index> components(num_vertices);
    for (auto i : range(num_facets)) {
        for (auto j : range(vertex_per_facet)) {
            components.merge(facets(i, j), facets(i, (j + 1) % vertex_per_facet));
        }
    }
    std::vector<Index> per_vertex_comp_ids;
    auto num_comps = components.extract_disjoint_set_indices(per_vertex_comp_ids);

    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_mins(num_comps, 2);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_maxs(num_comps, 2);
    bbox_mins.setConstant(std::numeric_limits<Scalar>::max());
    bbox_maxs.setConstant(std::numeric_limits<Scalar>::lowest());
    for (auto i : range(num_vertices)) {
        const auto comp_id = per_vertex_comp_ids[i];
        if (comp_id == static_cast<Index>(num_comps)) continue;
        bbox_mins(comp_id, 0) = std::min(bbox_mins(comp_id, 0), uvs(i, 0));
        bbox_maxs(comp_id, 0) = std::max(bbox_maxs(comp_id, 0), uvs(i, 0));
        bbox_mins(comp_id, 1) = std::min(bbox_mins(comp_id, 1), uvs(i, 1));
        bbox_maxs(comp_id, 1) = std::max(bbox_maxs(comp_id, 1), uvs(i, 1));
    }

    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> centers(num_comps, 2);
    std::vector<bool> flipped(num_comps);
#ifdef RECTANGLE_BIN_PACK_OSS
    bool allow_flip = true;
#else
    bool allow_flip = options.allow_flip;
#endif
    std::tie(centers, flipped) = pack_boxes(bbox_mins, bbox_maxs, allow_flip, options.margin);

    Eigen::Matrix<Scalar, 2, 2> rot90;
    rot90 << 0, -1, 1, 0;
    for (auto i : range(num_vertices)) {
        const auto comp_id = per_vertex_comp_ids[i];

        if (comp_id == static_cast<Index>(num_comps)) {
            // Poor isolated vertices.
            uvs.row(i).setZero();
            continue;
        }

        const Eigen::Matrix<Scalar, 1, 2> comp_center =
            (bbox_mins.row(comp_id) + bbox_maxs.row(comp_id)) * 0.5;
        if (!flipped[comp_id]) {
            uvs.row(i) = (uvs.row(i) - comp_center) + centers.row(comp_id);
        } else {
            uvs.row(i) = (uvs.row(i) - comp_center) * rot90 + centers.row(comp_id);
        }
    }

    const auto all_bbox_min = uvs.colwise().minCoeff().eval();
    const auto all_bbox_max = uvs.colwise().maxCoeff().eval();
    const Scalar s = options.normalize ? (all_bbox_max - all_bbox_min).maxCoeff() : 1;

    uvs = (uvs.rowwise() - all_bbox_min) / s;
    mesh.initialize_uv(uvs, facets);
    timer.tock("Packing uv");
}

template <typename MeshType>
void compute_rectangle_packing(MeshType& mesh)
{
    const PackingOptions opt;
    return compute_rectangle_packing(mesh, opt);
}

/**
 * Pack a list of 2D meshes.
 *
 * @param[in,out] meshes_2D The list of 2D mesh pointers.
 * @param[in]     options   The packing options.
 *
 * @tparam        MeshTypePtr  The Mesh pointer type.
 *
 * The vertex coordinates of `meshes_2D` will be updated as a result.
 */
template <typename MeshTypePtr>
void compute_rectangle_packing(std::vector<MeshTypePtr>& meshes_2d, const PackingOptions& options)
{
    using MeshType = typename std::pointer_traits<MeshTypePtr>::element_type;
    using Scalar = typename MeshType::Scalar;
    using VertexArray = typename MeshType::VertexArray;
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input is not a Mesh object");

    const auto num_meshes = meshes_2d.size();
    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_mins(num_meshes, 2);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_maxs(num_meshes, 2);
    bbox_mins.setConstant(std::numeric_limits<Scalar>::max());
    bbox_maxs.setConstant(std::numeric_limits<Scalar>::lowest());

    for (auto i : range(num_meshes)) {
        const auto& vertices = meshes_2d[i]->get_vertices();
        bbox_mins.row(i) = vertices.colwise().minCoeff();
        bbox_maxs.row(i) = vertices.colwise().maxCoeff();
    }

    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> centers(num_meshes, 2);
    std::vector<bool> flipped(num_meshes);
#ifdef RECTANGLE_BIN_PACK_OSS
    bool allow_flip = false;
#else
    bool allow_flip = options.allow_flip;
#endif
    std::tie(centers, flipped) = pack_boxes(bbox_mins, bbox_maxs, allow_flip, options.margin);

    Eigen::Matrix<Scalar, 2, 2> rot90;
    rot90 << 0, -1, 1, 0;

    Eigen::Matrix<Scalar, 1, 2> all_bbox_min;
    Eigen::Matrix<Scalar, 1, 2> all_bbox_max;
    all_bbox_min.setConstant(std::numeric_limits<Scalar>::max());
    all_bbox_max.setConstant(std::numeric_limits<Scalar>::lowest());

    for (auto i : range(num_meshes)) {
        const auto comp_center = (bbox_mins.row(i) + bbox_maxs.row(i)) * 0.5;
        VertexArray vertices;
        meshes_2d[i]->export_vertices(vertices);

        if (!flipped[i]) {
            vertices.rowwise() += centers.row(i) - comp_center;
        } else {
            vertices =
                ((vertices.rowwise() - comp_center).transpose() * rot90).transpose().rowwise() +
                centers.row(i);
        }
        all_bbox_min = all_bbox_min.array().min(vertices.colwise().minCoeff().array());
        all_bbox_max = all_bbox_max.array().max(vertices.colwise().maxCoeff().array());
        meshes_2d[i]->import_vertices(vertices);
    }

    const Scalar s = options.normalize ? (all_bbox_max - all_bbox_min).maxCoeff() : 1;

    for (auto i : range(num_meshes)) {
        VertexArray vertices;
        meshes_2d[i]->export_vertices(vertices);
        vertices = vertices / s;
        meshes_2d[i]->import_vertices(vertices);
    }
}


} // namespace legacy
} // namespace packing
} // namespace lagrange
