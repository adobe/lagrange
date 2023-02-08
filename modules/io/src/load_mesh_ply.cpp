/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/io/load_mesh_ply.h>

#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/readPLY.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::io {

template <typename MeshType>
MeshType load_mesh_ply(std::istream& input_stream, const LoadOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using AttributeArray = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

    VertexArray V;
    FacetArray F;
    Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic> E;
    AttributeArray N;
    AttributeArray UV;

    AttributeArray VD;
    std::vector<std::string> VDheader;

    AttributeArray FD;
    std::vector<std::string> FDheader;

    AttributeArray ED;
    std::vector<std::string> EDheader;
    std::vector<std::string> comments;

    // TODO: libigl's wrapper around tinyply doesn't allow to load attributes of different tyeps
    // (e.g. char for colors + float for normals).
    bool success = igl::
        readPLY(input_stream, V, F, E, N, UV, VD, VDheader, FD, FDheader, ED, EDheader, comments);
    if (!success) throw std::runtime_error("Error reading ply!");

    MeshType mesh;
    mesh.add_vertices(V.rows());
    mesh.add_triangles(F.rows());
    vertex_ref(mesh) = V;
    facet_ref(mesh) = F;

    // TODO we should replace this whole file with happly, rather than implementing the stuff below.

    if (Scalar(UV.rows()) == mesh.get_num_vertices() && options.load_uvs) {
        // TODO load uvs
    }

    if (Scalar(N.rows()) == mesh.get_num_vertices() && options.load_normals) {
        // TODO load normals
    }

    if (Scalar(VD.rows()) == mesh.get_num_vertices() && options.load_vertex_colors) {
        // TODO load vertex colors
    }

    return mesh;
}

template <typename MeshType>
MeshType load_mesh_ply(const fs::path& filename, const LoadOptions& options)
{
    fs::ifstream fin(filename);
    return load_mesh_ply<MeshType>(fin, options);
}

#define LA_X_load_mesh_ply(_, S, I) \
    template SurfaceMesh<S, I> load_mesh_ply(const fs::path& filename, const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh_ply, 0)

} // namespace lagrange::io
