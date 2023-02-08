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

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/io/save_mesh_ply.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <happly.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::io {

namespace {

template <typename T, typename Derived>
std::vector<T> to_vector(const Eigen::DenseBase<Derived>& src)
{
    std::vector<T> dst(src.size());
    Eigen::VectorX<T>::Map(dst.data(), dst.size()) = src.template cast<T>();
    return dst;
}

} // namespace

template <typename Scalar, typename Index>
void save_mesh_ply(
    std::ofstream& output_stream,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    la_runtime_assert(mesh.get_dimension() == 3);

    // Create an empty object
    happly::PLYData ply;

    // Add mesh elements
    ply.addElement("vertex", mesh.get_num_vertices());
    ply.addElement("face", mesh.get_num_facets());
    if (mesh.has_edges()) {
        ply.addElement("edge", mesh.get_num_edges());
    }

    // Vertex positions
    {
        auto pos = vertex_view(mesh);
        auto vx = to_vector<Scalar>(pos.col(0));
        auto vy = to_vector<Scalar>(pos.col(1));
        auto vz = to_vector<Scalar>(pos.col(2));
        ply.getElement("vertex").addProperty<Scalar>("x", vx);
        ply.getElement("vertex").addProperty<Scalar>("y", vy);
        ply.getElement("vertex").addProperty<Scalar>("z", vz);
    }

    // TODO: Make this a utility function?
    std::unordered_set<AttributeId> allowed_ids;
    if (options.output_attributes == SaveOptions::OutputAttributes::SelectedOnly) {
        allowed_ids.insert(options.selected_attributes.begin(), options.selected_attributes.end());
    } else {
        la_debug_assert(options.output_attributes == SaveOptions::OutputAttributes::All);
        mesh.seq_foreach_attribute_id([&](auto id) { allowed_ids.insert(id); });
    }

    // Vertex normals
    if (auto id = internal::find_matching_attribute<Scalar>(
            mesh,
            allowed_ids,
            AttributeElement::Vertex,
            AttributeUsage::Normal,
            3);
        id != invalid_attribute_id()) {
        logger().debug("Writing vertex normal attribute '{}'", mesh.get_attribute_name(id));
        auto nrm = attribute_matrix_view<Scalar>(mesh, id);
        auto nx = to_vector<Scalar>(nrm.col(0));
        auto ny = to_vector<Scalar>(nrm.col(1));
        auto nz = to_vector<Scalar>(nrm.col(2));
        ply.getElement("vertex").addProperty<Scalar>("nx", nx);
        ply.getElement("vertex").addProperty<Scalar>("ny", ny);
        ply.getElement("vertex").addProperty<Scalar>("nz", nz);
    }

    // Vertex texcoords
    if (auto id = internal::find_matching_attribute<Scalar>(
            mesh,
            allowed_ids,
            AttributeElement::Vertex,
            AttributeUsage::UV,
            2);
        id != invalid_attribute_id()) {
        logger().debug("Writing vertex uv attribute '{}'", mesh.get_attribute_name(id));
        auto uv = attribute_matrix_view<Scalar>(mesh, id);
        logger().info("uv rows {} / num vtx {}", uv.rows(), mesh.get_num_vertices());
        auto s = to_vector<Scalar>(uv.col(0));
        auto t = to_vector<Scalar>(uv.col(1));
        ply.getElement("vertex").addProperty<Scalar>("u", s);
        ply.getElement("vertex").addProperty<Scalar>("v", t);
    }

    // Vertex colors
    if (auto id = internal::find_matching_attribute<Scalar>(
            mesh,
            allowed_ids,
            AttributeElement::Vertex,
            AttributeUsage::Color,
            3);
        id != invalid_attribute_id()) {
        logger().debug("Writing vertex color attribute '{}'", mesh.get_attribute_name(id));
        auto rgb = (attribute_matrix_view<Scalar>(mesh, id) * Scalar(255)).array().round().eval();
        auto r = to_vector<unsigned char>(rgb.col(0));
        auto g = to_vector<unsigned char>(rgb.col(1));
        auto b = to_vector<unsigned char>(rgb.col(2));
        ply.getElement("vertex").addProperty<unsigned char>("red", r);
        ply.getElement("vertex").addProperty<unsigned char>("green", g);
        ply.getElement("vertex").addProperty<unsigned char>("blue", b);
    }

    // Facet indices
    {
        std::vector<std::vector<uint32_t>> facet_indices(mesh.get_num_facets());
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            facet_indices[f].reserve(mesh.get_facet_size(f));
            for (Index v : mesh.get_facet_vertices(f)) {
                facet_indices[f].push_back(static_cast<uint32_t>(v));
            }
        }
        ply.getElement("face").addListProperty<uint32_t>("vertex_indices", facet_indices);
    }

    // Edge indices
    if (mesh.has_edges()) {
        std::vector<uint32_t> v1(mesh.get_num_edges());
        std::vector<uint32_t> v2(mesh.get_num_edges());
        for (Index e = 0; e < mesh.get_num_edges(); ++e) {
            auto verts = mesh.get_edge_vertices(e);
            v1[e] = uint32_t(verts[0]);
            v2[e] = uint32_t(verts[1]);
        }
        ply.getElement("edge").addProperty<uint32_t>("vertex1", v1);
        ply.getElement("edge").addProperty<uint32_t>("vertex2", v2);
    }

    // Write the object to file
    happly::DataFormat format;
    switch (options.encoding) {
    case FileEncoding::Binary: format = happly::DataFormat::Binary; break;
    case FileEncoding::Ascii: format = happly::DataFormat::ASCII; break;
    default: format = happly::DataFormat::Binary;
    }
    ply.write(output_stream, format);
}

template <typename Scalar, typename Index>
void save_mesh_ply(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    fs::ofstream fout(filename);
    save_mesh_ply(fout, mesh, options);
}

#define LA_X_save_mesh_ply(_, Scalar, Index)    \
    template void save_mesh_ply(                \
        const fs::path& filename,               \
        const SurfaceMesh<Scalar, Index>& mesh, \
        const SaveOptions& options);
LA_SURFACE_MESH_X(save_mesh_ply, 0)

} // namespace lagrange::io
