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
#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/Logger.h>
#include <lagrange/mesh_cleanup/rescale_uv_charts.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>
#include <lagrange/compute_uv_charts.h>


#include <Eigen/Core>
#include <Eigen/Dense>

namespace lagrange {

namespace {

template <typename Scalar, typename Index>
AttributeId get_uv_attr_id(SurfaceMesh<Scalar, Index>& mesh, std::string_view uv_attribute_name)
{
    if (uv_attribute_name.empty()) {
        auto id = internal::find_matching_attribute<Scalar>(
            mesh,
            "",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2);
        la_runtime_assert(id != invalid_attribute_id(), "No UV attribute found.");
        return id;
    } else {
        return mesh.get_attribute_id(uv_attribute_name);
    }
}

template <typename Scalar, typename Index>
AttributeId get_patch_attr_id(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view chart_id_attribute_name)
{
    if (chart_id_attribute_name.empty()) {
        UVChartOptions options;
        options.output_attribute_name = "@chart_id";
        options.connectivity_type = UVChartOptions::ConnectivityType::Vertex;
        logger().info(
            "Patch id extracted from UV charts as attribute \"{}\"",
            options.output_attribute_name);
        compute_uv_charts(mesh, options);
        return mesh.get_attribute_id(options.output_attribute_name);
    } else {
        return mesh.get_attribute_id(chart_id_attribute_name);
    }
}

} // namespace

template <typename Scalar, typename Index>
void rescale_uv_charts(SurfaceMesh<Scalar, Index>& mesh, const RescaleUVOptions& options)
{
    const auto num_facets = mesh.get_num_facets();
    if (num_facets == 0) return;

    auto uv_attr_id = get_uv_attr_id(mesh, options.uv_attribute_name);
    auto patch_attr_id = get_patch_attr_id(mesh, options.chart_id_attribute_name);

    la_runtime_assert(mesh.is_attribute_indexed(uv_attr_id), "UV attribute must be indexed.");

    auto chart_ids = vector_view(mesh.template get_attribute<Index>(patch_attr_id));
    const Index num_patches = chart_ids.maxCoeff() + 1;

    auto vertices = vertex_view(mesh);
    auto facets = facet_view(mesh);

    auto& uv_attr = mesh.template ref_indexed_attribute<Scalar>(uv_attr_id);
    auto uv_values = matrix_ref(uv_attr.values());
    auto uv_indices = reshaped_view(uv_attr.indices(), 3);

    std::vector<Scalar> u_scales(num_patches, 0);
    std::vector<Scalar> v_scales(num_patches, 0);
    std::vector<Scalar> chart_areas(num_patches, 0);
    std::vector<Index> facet_counts(num_patches, 0);
    for (auto i : range(num_facets)) {
        const Index id = static_cast<Index>(chart_ids(i));

        Eigen::Matrix<Scalar, 3, 3> V, U, J;
        V.col(0) = vertices.row(facets(i, 0)).transpose();
        V.col(1) = vertices.row(facets(i, 1)).transpose();
        V.col(2) = vertices.row(facets(i, 2)).transpose();

        U.col(0) << uv_values(uv_indices(i, 0), 0), uv_values(uv_indices(i, 0), 1), 1;
        U.col(1) << uv_values(uv_indices(i, 1), 0), uv_values(uv_indices(i, 1), 1), 1;
        U.col(2) << uv_values(uv_indices(i, 2), 0), uv_values(uv_indices(i, 2), 1), 1;

        Scalar uv_area = U.determinant();
        Scalar surface_area = (V.col(1) - V.col(0)).cross(V.col(2) - V.col(0)).norm();
        chart_areas[id] += uv_area > 0 ? surface_area : -surface_area;
        if (std::abs(uv_area) < static_cast<Scalar>(options.uv_area_threshold)) {
            // UV triangle is degenerate. Skipping it to avoid numerical issue
            // from creating outlier in uv scaling.
            continue;
        }

        J = V * U.inverse();
        if (!J.array().isFinite().all()) {
            continue;
        }

        u_scales[id] += J.col(0).norm();
        v_scales[id] += J.col(1).norm();
        facet_counts[id]++;
    }

    std::vector<bool> visited(uv_values.rows(), false);
    for (auto i : range(num_facets)) {
        const Index id = static_cast<Index>(chart_ids(i));
        const Index v0 = uv_indices(i, 0);
        const Index v1 = uv_indices(i, 1);
        const Index v2 = uv_indices(i, 2);
        const Index c = facet_counts[id];
        const Scalar u_scale = c == 0 ? 1 : u_scales[id] / c;
        const Scalar v_scale = c == 0 ? 1 : v_scales[id] / c;
        const Scalar u_sign = chart_areas[id] < 0 ? -1 : 1;

        if (!visited[v0]) {
            uv_values(v0, 0) *= u_scale * u_sign;
            uv_values(v0, 1) *= v_scale;
            visited[v0] = true;
        }
        if (!visited[v1]) {
            uv_values(v1, 0) *= u_scale * u_sign;
            uv_values(v1, 1) *= v_scale;
            visited[v1] = true;
        }
        if (!visited[v2]) {
            uv_values(v2, 0) *= u_scale * u_sign;
            uv_values(v2, 1) *= v_scale;
            visited[v2] = true;
        }
    }
}

#define LA_X_rescale_uv_charts(_, Scalar, Index)    \
    template void rescale_uv_charts<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                \
        const RescaleUVOptions&);
LA_SURFACE_MESH_X(rescale_uv_charts, 0)

} // namespace lagrange
