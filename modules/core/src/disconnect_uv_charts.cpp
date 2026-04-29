/*
 * Copyright 2026 Adobe. All rights reserved.
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
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_uv_charts.h>
#include <lagrange/disconnect_uv_charts.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/get_unique_attribute_name.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/hash.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/scope_guard.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace lagrange {

namespace {

template <typename UVScalar, typename Scalar, typename Index>
size_t disconnect_uv_charts_impl(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId uv_attr_id,
    span<const Index> chart_ids)
{
    auto& uv_attr = mesh.template ref_indexed_attribute<UVScalar>(uv_attr_id);
    auto old_values = matrix_view(uv_attr.values());
    auto uv_indices = vector_ref(uv_attr.indices());

    const Index num_old_values = static_cast<Index>(old_values.rows());
    const Index num_cols = static_cast<Index>(old_values.cols());

    // Remap (uv_index, chart_id) -> new_uv_index, rebuilding the values array.
    using Key = std::pair<Index, Index>;
    using KeyHash = OrderedPairHash<Key>;
    std::unordered_map<Key, Index, KeyHash> remap;
    std::vector<std::array<UVScalar, 2>> new_values;
    new_values.reserve(num_old_values);

    const Index num_facets = mesh.get_num_facets();

    for (Index f = 0; f < num_facets; ++f) {
        const Index chart = chart_ids[f];
        const auto c_begin = mesh.get_facet_corner_begin(f);
        const auto c_end = mesh.get_facet_corner_end(f);
        for (auto c = c_begin; c < c_end; ++c) {
            const Index uv_idx = static_cast<Index>(uv_indices[c]);
            auto [it, inserted] =
                remap.emplace(Key{uv_idx, chart}, static_cast<Index>(new_values.size()));
            if (inserted) {
                std::array<UVScalar, 2> v;
                for (Index col = 0; col < num_cols; ++col) {
                    v[col] = old_values(uv_idx, col);
                }
                new_values.push_back(v);
            }
            uv_indices[c] = it->second;
        }
    }

    size_t num_duplicated = new_values.size() > static_cast<size_t>(num_old_values)
                                ? new_values.size() - static_cast<size_t>(num_old_values)
                                : 0;

    // Rewrite UV values
    uv_attr.values().resize_elements(new_values.size());
    auto new_val_ref = matrix_ref(uv_attr.values());
    for (size_t i = 0; i < new_values.size(); ++i) {
        for (Index c = 0; c < num_cols; ++c) {
            new_val_ref(i, c) = new_values[i][c];
        }
    }

    if (num_duplicated > 0) {
        logger().info(
            "Disconnected UV charts: duplicated {} UV vertices ({} -> {}).",
            num_duplicated,
            num_old_values,
            new_values.size());
    }

    return num_duplicated;
}

} // namespace

template <typename Scalar, typename Index>
size_t disconnect_uv_charts(
    SurfaceMesh<Scalar, Index>& mesh,
    const DisconnectUVChartsOptions& options)
{
    // Resolve UV attribute name (indexed UV attributes only)
    UVMeshOptions uv_mesh_options;
    uv_mesh_options.uv_attribute_name = options.uv_attribute_name;
    uv_mesh_options.element_types = UVMeshOptions::ElementTypes::IndexedOrVertex;

    using OtherScalar = std::conditional_t<std::is_same_v<Scalar, float>, double, float>;

    AttributeId uv_attr_id;
    bool is_other_scalar = false;
    if (auto id = uv_attribute_id<Scalar, Index, Scalar>(mesh, uv_mesh_options)) {
        uv_attr_id = *id;
    } else if (auto id2 = uv_attribute_id<Scalar, Index, OtherScalar>(mesh, uv_mesh_options)) {
        uv_attr_id = *id2;
        is_other_scalar = true;
    } else {
        throw Error("disconnect_uv_charts: no suitable indexed UV attribute found.");
    }
    if (!mesh.is_attribute_indexed(uv_attr_id)) {
        throw Error(
            "disconnect_uv_charts: UV attribute must be indexed. "
            "Found a vertex UV attribute, but this function requires an indexed UV attribute.");
    }

    std::string_view uv_attr_name = mesh.get_attribute_name(uv_attr_id);

    // Get or compute chart ids
    std::string chart_attr_name;
    auto cleanup_guard = make_scope_guard([&]() noexcept {
        if (!chart_attr_name.empty() && mesh.has_attribute(chart_attr_name)) {
            mesh.delete_attribute(chart_attr_name);
        }
    });

    if (options.chart_id_attribute_name.empty()) {
        chart_attr_name = get_unique_attribute_name(mesh, "@_disconnect_uv_charts_tmp");
        UVChartOptions chart_options;
        chart_options.uv_attribute_name = uv_attr_name;
        chart_options.output_attribute_name = chart_attr_name;
        compute_uv_charts(mesh, chart_options);
    } else {
        cleanup_guard.dismiss();
        chart_attr_name = options.chart_id_attribute_name;
    }

    if (!mesh.has_attribute(chart_attr_name)) {
        throw Error("disconnect_uv_charts: chart ID attribute does not exist.");
    }
    auto chart_id_attr_id = mesh.get_attribute_id(chart_attr_name);
    if (mesh.get_attribute_base(chart_id_attr_id).get_element_type() != AttributeElement::Facet) {
        throw Error("disconnect_uv_charts: chart ID attribute must be a facet attribute.");
    }
    auto chart_ids = attribute_vector_view<Index>(mesh, chart_id_attr_id);
    if (static_cast<size_t>(chart_ids.size()) != mesh.get_num_facets()) {
        throw Error("disconnect_uv_charts: chart ID attribute must have one value per facet.");
    }
    span<const Index> chart_ids_span{chart_ids.data(), static_cast<size_t>(chart_ids.size())};

    // Dispatch based on UV scalar type
    size_t num_duplicated;
    if (!is_other_scalar) {
        num_duplicated = disconnect_uv_charts_impl<Scalar>(mesh, uv_attr_id, chart_ids_span);
    } else {
        num_duplicated = disconnect_uv_charts_impl<OtherScalar>(mesh, uv_attr_id, chart_ids_span);
    }

    return num_duplicated;
}

#define LA_X_disconnect_uv_charts(_, Scalar, Index)                  \
    template LA_CORE_API size_t disconnect_uv_charts<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                 \
        const DisconnectUVChartsOptions&);
LA_SURFACE_MESH_X(disconnect_uv_charts, 0)

} // namespace lagrange
