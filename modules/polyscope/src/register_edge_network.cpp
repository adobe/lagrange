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
#include <lagrange/polyscope/register_edge_network.h>

#include "register_attributes.h"

#include <lagrange/AttributeTypes.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

namespace lagrange::polyscope {

template <typename Scalar, typename Index>
::polyscope::CurveNetwork* register_edge_network(
    std::string_view name,
    const SurfaceMesh<Scalar, Index>& mesh)
{
    const Index dim = mesh.get_dimension();
    la_runtime_assert(dim == 2 || dim == 3, "Only 2D and 3D edge networks are supported.");
    la_runtime_assert(
        mesh.is_regular() && mesh.get_vertex_per_facet() == 2,
        "Edge network can only be registered for meshes with size-2 facets");
    auto ps_network = [&] {
        if (dim == 2) {
            return ::polyscope::registerCurveNetwork2D(
                std::string(name),
                vertex_view(mesh),
                facet_view(mesh));
        } else {
            return ::polyscope::registerCurveNetwork(
                std::string(name),
                vertex_view(mesh),
                facet_view(mesh));
        }
    }();
    register_attributes(ps_network, mesh);
    return ps_network;
}

template <typename ValueType>
::polyscope::CurveNetworkQuantity* register_attribute(
    ::polyscope::CurveNetwork& ps_curve_network,
    std::string_view name,
    const lagrange::Attribute<ValueType>& attr)
{
    return register_attribute(&ps_curve_network, name, attr);
}

#define LA_X_register_edge_network(_, Scalar, Index)                          \
    template ::polyscope::CurveNetwork* register_edge_network<Scalar, Index>( \
        std::string_view name,                                                \
        const SurfaceMesh<Scalar, Index>& mesh);
LA_SURFACE_MESH_X(register_edge_network, 0)

#define LA_X_register_attribute(_, ValueType)                                  \
    template ::polyscope::CurveNetworkQuantity* register_attribute<ValueType>( \
        ::polyscope::CurveNetwork & ps_curve_network,                          \
        std::string_view name,                                                 \
        const lagrange::Attribute<ValueType>& attr);
LA_ATTRIBUTE_X(register_attribute, 0)

} // namespace lagrange::polyscope
