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
#include <lagrange/polyscope/register_mesh.h>

#include "register_attributes.h"

#include <lagrange/AttributeTypes.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

namespace lagrange::polyscope {

namespace {

template <typename Scalar, typename Index>
struct MeshFacetAdapter
{
    using SurfaceMeshType = SurfaceMesh<Scalar, Index>;
    const SurfaceMesh<Scalar, Index>& mesh;
};

// Convert mesh facet list of variable size to a standard format for Polyscope
template <class S, class I, class T, typename Scalar, typename Index>
std::tuple<std::vector<S>, std::vector<I>> standardizeNestedList(
    const MeshFacetAdapter<Scalar, Index>& data)
{
    auto& mesh = data.mesh;
    std::vector<S> entries;
    std::vector<I> start;
    entries.reserve(mesh.get_num_corners());
    start.reserve(mesh.get_num_facets() + 1);
    I last_end = 0;
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        auto facet = mesh.get_facet_vertices(f);
        start.push_back(mesh.get_facet_corner_begin(f));
        entries.insert(entries.end(), facet.begin(), facet.end());
        last_end = start.back() + static_cast<I>(facet.size());
    }
    start.push_back(last_end);
    return {entries, start};
}

} // namespace

template <typename Scalar, typename Index>
::polyscope::SurfaceMesh* register_mesh(
    std::string_view name,
    const SurfaceMesh<Scalar, Index>& mesh)
{
    const Index dim = mesh.get_dimension();
    la_runtime_assert(dim == 2 || dim == 3, "Only 2D and 3D meshes are supported.");

    // Register mesh connectivity with Polyscope
    auto ps_mesh = [&] {
        if (mesh.is_regular()) {
            // Simple case when facets can be viewed as a matrix
            if (dim == 2) {
                return ::polyscope::registerSurfaceMesh2D(
                    std::string(name),
                    vertex_view(mesh),
                    facet_view(mesh));
            } else {
                return ::polyscope::registerSurfaceMesh(
                    std::string(name),
                    vertex_view(mesh),
                    facet_view(mesh));
            }
        } else {
            // Hybrid meshes require going through a custom adapter
            if (dim == 2) {
                return ::polyscope::registerSurfaceMesh2D(
                    std::string(name),
                    vertex_view(mesh),
                    MeshFacetAdapter<Scalar, Index>{mesh});
            } else {
                return ::polyscope::registerSurfaceMesh(
                    std::string(name),
                    vertex_view(mesh),
                    MeshFacetAdapter<Scalar, Index>{mesh});
            }
        }
    }();

    register_attributes(ps_mesh, mesh);

    return ps_mesh;
}

template <typename ValueType>
::polyscope::SurfaceMeshQuantity* register_attribute(
    ::polyscope::SurfaceMesh& ps_mesh,
    std::string_view name,
    const lagrange::Attribute<ValueType>& attr)
{
    return register_attribute(&ps_mesh, name, attr);
}

#define LA_X_register_mesh(_, Scalar, Index)                         \
    template ::polyscope::SurfaceMesh* register_mesh<Scalar, Index>( \
        std::string_view name,                                       \
        const SurfaceMesh<Scalar, Index>& mesh);
LA_SURFACE_MESH_X(register_mesh, 0)

#define LA_X_register_attribute(_, ValueType)                                 \
    template ::polyscope::SurfaceMeshQuantity* register_attribute<ValueType>( \
        ::polyscope::SurfaceMesh & ps_mesh,                                   \
        std::string_view name,                                                \
        const lagrange::Attribute<ValueType>& attr);
LA_ATTRIBUTE_X(register_attribute, 0)

} // namespace lagrange::polyscope
