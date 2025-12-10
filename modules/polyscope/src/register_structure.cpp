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
#include <lagrange/polyscope/register_structure.h>

#include "register_attributes.h"

#include <lagrange/AttributeTypes.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/polyscope/register_edge_network.h>
#include <lagrange/polyscope/register_mesh.h>
#include <lagrange/polyscope/register_point_cloud.h>
#include <lagrange/utils/Error.h>
#include <lagrange/views.h>

namespace lagrange::polyscope {

template <typename Scalar, typename Index>
::polyscope::Structure* register_structure(
    std::string_view name,
    const SurfaceMesh<Scalar, Index>& mesh)
{
    if (mesh.get_num_facets() == 0) {
        // Register point cloud
        logger().debug("Registering point cloud '{}'", name);
        return register_point_cloud(name, mesh);
    } else if (mesh.is_regular() && mesh.get_vertex_per_facet() == 2) {
        // Register edge network
        logger().debug("Registering edge network '{}'", name);
        return register_edge_network(name, mesh);
    } else {
        // Register surface mesh
        logger().debug("Registering surface mesh '{}'", name);
        return register_mesh(name, mesh);
    }
}

template <typename ValueType>
::polyscope::Quantity* register_attribute(
    ::polyscope::Structure& ps_struct,
    std::string_view name,
    const lagrange::Attribute<ValueType>& attr)
{
    if (auto ps_point_cloud = dynamic_cast<::polyscope::PointCloud*>(&ps_struct)) {
        return register_attribute(ps_point_cloud, name, attr);
    } else if (auto ps_edge_network = dynamic_cast<::polyscope::CurveNetwork*>(&ps_struct)) {
        return register_attribute(ps_edge_network, name, attr);
    } else if (auto ps_mesh = dynamic_cast<::polyscope::SurfaceMesh*>(&ps_struct)) {
        return register_attribute(ps_mesh, name, attr);
    } else {
        throw Error("Unsupported Polyscope structure type");
    }
}

#define LA_X_register_structure(_, Scalar, Index)                       \
    template ::polyscope::Structure* register_structure<Scalar, Index>( \
        std::string_view name,                                          \
        const SurfaceMesh<Scalar, Index>& mesh);
LA_SURFACE_MESH_X(register_structure, 0)

#define LA_X_register_attribute(_, ValueType)                      \
    template ::polyscope::Quantity* register_attribute<ValueType>( \
        ::polyscope::Structure & ps_structure,                     \
        std::string_view name,                                     \
        const lagrange::Attribute<ValueType>& attr);
LA_ATTRIBUTE_X(register_attribute, 0)

} // namespace lagrange::polyscope
