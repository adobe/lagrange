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

#include <fstream>
#include <numeric>
#include <string>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/writeOBJ.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/utils/la_assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/la_assert.h>

#include <lagrange/io/save_mesh_ply.h>

#include <lagrange/fs/filesystem.h>

namespace lagrange {
namespace io {

namespace internal {
template <typename MeshType>
void save_mesh_2D(const fs::path& filename, const MeshType& mesh)
{
    fs::ofstream fout(filename);
    const auto& vertices = mesh.get_vertices();
    for (auto i : range(mesh.get_num_vertices())) {
        fout << "v " << vertices(i, 0) << " " << vertices(i, 1) << std::endl;
    }

    const auto& facets = mesh.get_facets();
    const auto vertex_per_facet = mesh.get_vertex_per_facet();
    for (auto i : range(mesh.get_num_facets())) {
        fout << "f";
        for (auto j : range(vertex_per_facet)) {
            fout << " " << facets(i, j) + 1;
        }
        fout << std::endl;
    }
    fout.close();
}

template <typename MeshType>
void save_mesh_basic(const fs::path& filename, const MeshType& mesh)
{
    if (mesh.get_dim() == 2) {
        save_mesh_2D(filename, mesh);
    } else {
        const auto& vertices = mesh.get_vertices();
        const auto& facets = mesh.get_facets();
        igl::writeOBJ(filename.string(), vertices, facets);
    }
}

template <typename MeshType>
void save_mesh_with_uv(const fs::path& filename, const MeshType& mesh)
{
    LA_ASSERT(mesh.is_uv_initialized());
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();

    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> CN;
    Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> FN;

    const auto& TC = mesh.get_uv();
    const auto& FTC = mesh.get_uv_indices();

    if (mesh.has_corner_attribute("normal")) {
        CN = mesh.get_corner_attribute("normal");
        FN.resize(facets.rows(), 3);
        std::iota(FN.data(), FN.data() + FN.size(), 0);
    } else if (mesh.has_indexed_attribute("normal")) {
        auto attr = mesh.get_indexed_attribute("normal");
        CN = std::get<0>(attr);
        FN = std::get<1>(attr);
    }

    igl::writeOBJ(filename.string(), vertices, facets, CN, FN, TC, FTC);
}

template <typename MeshType>
void save_mesh_with_vertex_uv(const fs::path& filename, const MeshType& mesh)
{
    LA_ASSERT(mesh.has_vertex_attribute("uv"));

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();

    Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor> CN;
    Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor> FN;

    const auto& TC = mesh.get_vertex_attribute("uv");
    const auto& FTC = facets;

    igl::writeOBJ(filename.string(), vertices, facets, CN, FN, TC, FTC);
}

template <typename MeshType>
void save_mesh_with_corner_uv(const fs::path& filename, const MeshType& mesh)
{
    LA_ASSERT(mesh.has_corner_attribute("uv"));

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();
    LA_ASSERT(vertex_per_facet == 3);

    Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor> CN;
    Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor> FN;

    const auto& TC = mesh.get_corner_attribute("uv");
    LA_ASSERT(safe_cast<Index>(TC.rows()) == num_facets * vertex_per_facet);
    LA_ASSERT(TC.cols() == 2);
    Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor> FTC;
    FTC.resize(num_facets, 3);
    for (Index i = 0; i < num_facets; i++) {
        FTC(i, 0) = i * 3;
        FTC(i, 1) = i * 3 + 1;
        FTC(i, 2) = i * 3 + 2;
    }

    igl::writeOBJ(filename.string(), vertices, facets, CN, FN, TC, FTC);
}


template <typename MeshType>
void save_mesh_vtk(
    const fs::path& filename,
    const MeshType& mesh,
    const std::vector<std::string>& face_attrib_names,
    const std::vector<std::string>& vertex_attrib_names)
{
    using AttributeArray = typename MeshType::AttributeArray;

    auto write_connectivity = [&](std::ostream& fl) {
        LA_ASSERT(fl);
        // Not a hard requirement. But since this is just a debugging tool
        // let's just enforce it for now.
        LA_ASSERT(mesh.get_vertex_per_facet() == 3);

        /*
         * Write the vtk file.
         */

        // write the header
        fl << "# vtk DataFile Version 2.0\n";
        fl << "Lagrange output mesh\n";
        fl << "ASCII\n";
        fl << "DATASET UNSTRUCTURED_GRID\n";
        fl << "\n";

        // write the vertices
        fl << "POINTS " << mesh.get_num_vertices() << " float\n";
        for (auto vnidx : range(mesh.get_num_vertices())) {
            auto xyz = mesh.get_vertices().row(vnidx).eval();
            if (xyz.cols() == 3) {
                fl << xyz(0) << " " << xyz(1) << " " << xyz(2) << "\n";
            } else if (xyz.cols() == 2) {
                fl << xyz(0) << " " << xyz(1) << " " << 0 << "\n";
            } else {
                throw std::runtime_error("This dimension is not supported");
            }
        }
        fl << "\n";

        //
        // write the faces
        //

        // count their total number of vertices.
        fl << "CELLS " << mesh.get_num_facets() << " "
           << mesh.get_num_facets() * (mesh.get_vertex_per_facet() + 1) << "\n";
        for (auto fn : range(mesh.get_num_facets())) {
            fl << mesh.get_vertex_per_facet() << " ";
            for (auto voffset : range(mesh.get_vertex_per_facet())) { //
                fl << mesh.get_facets()(fn, voffset) << " ";
            }
            fl << "\n";
        }
        fl << "\n";

        // write the face types
        fl << "CELL_TYPES " << mesh.get_num_facets() << "\n";
        for (auto f : range(mesh.get_num_facets())) {
            (void)f;
            // fl << "7 \n"; // VTK POLYGON
            fl << "5 \n"; // VTK TRIANGLE
        }
        fl << "\n";
    }; // end of write connectivity

    auto write_vertex_data_header = [&](std::ostream& fl) {
        fl << "POINT_DATA " << mesh.get_num_vertices() << " \n";
    }; // end of write_vert_data_header

    auto write_cell_data_header = [&](std::ostream& fl) {
        fl << "CELL_DATA " << mesh.get_num_facets() << " \n";
    }; // end of write_cell_header

    auto write_data =
        [](std::ostream& fl, const std::string attrib_name, const AttributeArray& attrib) {
            fl << "SCALARS " << attrib_name << " float " << attrib.cols() << "\n";
            fl << "LOOKUP_TABLE default \n";
            for (auto row : range(attrib.rows())) {
                for (auto col : range(attrib.cols())) {
                    fl << attrib(row, col) << " ";
                } // end of col
                fl << "\n";
            } // end of row
            fl << "\n";
        }; // end of write_data()

    // Open the file
    fs::ofstream fl(filename, fs::fstream::out);
    fl.precision(12);
    fl.flags(fs::fstream::scientific);
    LA_ASSERT(fl.is_open());

    // write the connectivity
    write_connectivity(fl);

    // Writing the face attribs
    {
        // Check if the mesh has any of the requested attribs
        bool has_any_facet_attrib = false;
        for (const auto& attrib_name : face_attrib_names) {
            if (mesh.has_facet_attribute(attrib_name)) {
                has_any_facet_attrib = true;
                break;
            }
        }
        // Write the header if it does
        if (has_any_facet_attrib) {
            write_cell_data_header(fl);
        }
        // Now write the data
        for (const auto& attrib_name : face_attrib_names) {
            if (mesh.has_facet_attribute(attrib_name)) {
                write_data(fl, attrib_name, mesh.get_facet_attribute(attrib_name));
            }
        }

    } // end of writing face attribs

    // Writing the vertex attribs
    {
        // Check if the mesh has any of the requested attribs
        bool has_any_vertex_attrib = false;
        for (const auto& attrib_name : vertex_attrib_names) {
            if (mesh.has_vertex_attribute(attrib_name)) {
                has_any_vertex_attrib = true;
                break;
            }
        }

        // Write the header if it does
        if (has_any_vertex_attrib) {
            write_vertex_data_header(fl);
        }

        // Now write the data
        for (const auto& attrib_name : vertex_attrib_names) {
            if (mesh.has_vertex_attribute(attrib_name)) {
                write_data(fl, attrib_name, mesh.get_vertex_attribute(attrib_name));
            }
        }

    } // end of writing vertex attribs

} // end of write vtk

} // namespace internal


template <typename MeshType>
void save_mesh(const fs::path& filename, const MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    if (filename.extension() == ".obj") {
        if (mesh.is_uv_initialized()) {
            internal::save_mesh_with_uv(filename, mesh);
        } else if (mesh.has_vertex_attribute("uv")) {
            internal::save_mesh_with_vertex_uv(filename, mesh);
        } else if (mesh.has_corner_attribute("uv")) {
            internal::save_mesh_with_corner_uv(filename, mesh);
        } else {
            internal::save_mesh_basic(filename, mesh);
        }
    } else if (filename.extension() == ".vtk") {
        internal::save_mesh_vtk(
            filename,
            mesh,
            mesh.get_facet_attribute_names(),
            mesh.get_vertex_attribute_names());
    } else if (filename.extension() == ".ply") {
        save_mesh_ply(filename, mesh);
    } else {
        internal::save_mesh_basic(filename, mesh);
    }
}

} // namespace io
} // namespace lagrange
