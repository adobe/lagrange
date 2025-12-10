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
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_uv_charts.h>
#include <lagrange/map_attribute.h>
#include <lagrange/packing/api.h>
#include <lagrange/packing/repack_uv_charts.h>
#include <lagrange/utils/assert.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

#include "pack_boxes.h"

#include <limits>

namespace lagrange::packing {

template <typename Scalar, typename Index>
void repack_uv_charts(SurfaceMesh<Scalar, Index>& mesh, const RepackOptions& options)
{
    UVMeshOptions uv_options;
    uv_options.uv_attribute_name = options.uv_attribute_name;
    auto uv_mesh = uv_mesh_ref(mesh, uv_options);

    AttributeId chart_attr_id = invalid_attribute_id();
    if (options.chart_attribute_name.empty()) {
        // Compute chart id attribute name.
        UVChartOptions chart_options;
        chart_options.uv_attribute_name = options.uv_attribute_name;
        chart_options.output_attribute_name = "@patch_id";
        chart_options.connectivity_type = ConnectivityType::Vertex;
        compute_uv_charts(mesh, chart_options);
        chart_attr_id = mesh.get_attribute_id(chart_options.output_attribute_name);
    } else {
        la_runtime_assert(
            mesh.has_attribute(options.chart_attribute_name),
            "Chart id attribute not found.");
        chart_attr_id = mesh.get_attribute_id(options.chart_attribute_name);
    }
    // Map chart id attribute to vertex element.
    auto chart_ids = attribute_vector_view<Index>(mesh, chart_attr_id);
    Index num_charts = chart_ids.maxCoeff() + 1;

    uv_mesh.template create_attribute<Index>(
        "chart_id",
        AttributeElement::Facet,
        AttributeUsage::Scalar,
        1,
        {chart_ids.data(), static_cast<size_t>(chart_ids.size())});
    map_attribute_in_place(uv_mesh, "chart_id", AttributeElement::Vertex);
    auto vertex_chart_ids = attribute_vector_view<Index>(uv_mesh, "chart_id");

    auto uv_values = vertex_ref(uv_mesh);
    la_runtime_assert(uv_values.array().isFinite().all());

    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_mins(num_charts, 2);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> bbox_maxs(num_charts, 2);
    bbox_mins.setConstant(std::numeric_limits<Scalar>::max());
    bbox_maxs.setConstant(std::numeric_limits<Scalar>::lowest());

    Index num_uvs = uv_mesh.get_num_vertices();
    for (Index uv_id = 0; uv_id < num_uvs; uv_id++) {
        Index chart_id = vertex_chart_ids[uv_id];
        bbox_mins(chart_id, 0) = std::min(bbox_mins(chart_id, 0), uv_values(uv_id, 0));
        bbox_mins(chart_id, 1) = std::min(bbox_mins(chart_id, 1), uv_values(uv_id, 1));
        bbox_maxs(chart_id, 0) = std::max(bbox_maxs(chart_id, 0), uv_values(uv_id, 0));
        bbox_maxs(chart_id, 1) = std::max(bbox_maxs(chart_id, 1), uv_values(uv_id, 1));
    }

    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> centers(num_charts, 2);
    std::vector<bool> rotated(num_charts);
    Scalar canvas_size;
#ifdef RECTANGLE_BIN_PACK_OSS
    bool allow_rotation = true;
#else
    bool allow_rotation = options.allow_rotation;
#endif
    std::tie(centers, rotated, canvas_size) =
        pack_boxes(bbox_mins, bbox_maxs, allow_rotation, options.margin);

    Eigen::Matrix<Scalar, 2, 2> rot90;
    rot90 << 0, -1, 1, 0;

    for (Index uv_id = 0; uv_id < num_uvs; uv_id++) {
        Index chart_id = vertex_chart_ids[uv_id];
        const Eigen::Matrix<Scalar, 1, 2> comp_center =
            (bbox_mins.row(chart_id) + bbox_maxs.row(chart_id)) * 0.5;
        if (!rotated[chart_id]) {
            uv_values.row(uv_id) = (uv_values.row(uv_id) - comp_center) + centers.row(chart_id);
        } else {
            uv_values.row(uv_id) =
                (uv_values.row(uv_id) - comp_center) * rot90 + centers.row(chart_id);
        }
    }

    const auto all_bbox_min = uv_values.colwise().minCoeff().eval();
    uv_values = (uv_values.rowwise() - all_bbox_min) / canvas_size;
}

#define LA_X_repack_uv_charts(_, Scalar, Index)    \
    template LA_PACKING_API void repack_uv_charts( \
        SurfaceMesh<Scalar, Index>&,               \
        const RepackOptions&);
LA_SURFACE_MESH_X(repack_uv_charts, 0)

} // namespace lagrange::packing
