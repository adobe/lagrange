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
#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_components.h>
#include <lagrange/separate_by_components.h>
#include <lagrange/separate_by_facet_groups.h>

namespace lagrange {

template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> separate_by_components(
    const SurfaceMesh<Scalar, Index>& mesh,
    const SeparateByComponentsOptions& options)
{
    SurfaceMesh<Scalar, Index> mesh_copy = mesh; // This copy should be cheap.

    ComponentOptions component_options;
    component_options.connectivity_type = options.connectivity_type;
    size_t num_components = compute_components(mesh_copy, component_options);

    auto component_indices =
        mesh_copy.template get_attribute<Index>(component_options.output_attribute_name).get_all();
    return separate_by_facet_groups(
        mesh,
        num_components,
        component_indices,
        SeparateByFacetGroupsOptions(options));
}

#define LA_X_separate_by_components(_, Scalar, Index)                                    \
    template LA_CORE_API std::vector<SurfaceMesh<Scalar, Index>> separate_by_components( \
        const SurfaceMesh<Scalar, Index>&,                                               \
        const SeparateByComponentsOptions&);

LA_SURFACE_MESH_X(separate_by_components, 0)

} // namespace lagrange
