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

template <typename MeshType, typename DerivedV, typename DerivedF>
void extract_attribute(
    const MeshType& mesh,
    const std::string& attr_name,
    Eigen::MatrixBase<DerivedV>& values,
    Eigen::MatrixBase<DerivedF>& indices)
{
    const auto& facets = mesh.get_facets();
    if (mesh.has_indexed_attribute(attr_name)) {
        auto attr = mesh.get_indexed_attribute(attr_name);
        values = std::get<0>(attr);
        indices = std::get<1>(attr);
    } else if (mesh.has_corner_attribute(attr_name)) {
        values = mesh.get_corner_attribute(attr_name);
        indices.derived().resize(facets.rows(), facets.cols());
        typename MeshType::Index count = 0;
        for (auto i : range(facets.rows())) {
            for (auto j : range(facets.cols())) {
                indices(i, j) = count;
                count++;
            }
        }
    } else if (mesh.has_vertex_attribute(attr_name)) {
        values = mesh.get_vertex_attribute(attr_name);
        indices = facets;
    }
}

template <typename MeshType>
void save_mesh_obj(const fs::path& filename, const MeshType& mesh)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();

    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> CN;
    Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> FN;
    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> TC;
    Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> FTC;

    extract_attribute(mesh, "uv", TC, FTC);
    extract_attribute(mesh, "normal", CN, FN);

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
        internal::save_mesh_obj(filename, mesh);
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
