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

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/load_mesh_gltf.h>
#include <lagrange/io/load_mesh_msh.h>
#include <lagrange/io/load_mesh_obj.h>
#include <lagrange/io/load_mesh_ply.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_mesh_gltf.h>
#include <lagrange/io/save_mesh_msh.h>
#include <lagrange/io/save_mesh_obj.h>
#include <lagrange/io/save_mesh_ply.h>
#include <lagrange/io/save_simple_scene.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/utils/Error.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <sstream>
#include <string_view>

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

void populate_io_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;
    using SceneType = scene::SimpleScene<Scalar, Index, 3>;

    m.def(
        "save_mesh",
        [](const fs::path& filename,
           const MeshType& mesh,
           bool binary,
           bool exact_match,
           std::optional<std::vector<AttributeId>> selected_attributes) {
            io::SaveOptions opts;
            opts.encoding = binary ? io::FileEncoding::Binary : io::FileEncoding::Ascii;
            opts.attribute_conversion_policy =
                exact_match ? io::SaveOptions::AttributeConversionPolicy::ExactMatchOnly
                            : io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
            if (selected_attributes.has_value()) {
                opts.selected_attributes = std::move(selected_attributes.value());
                opts.output_attributes = io::SaveOptions::OutputAttributes::SelectedOnly;
            } else {
                opts.output_attributes = io::SaveOptions::OutputAttributes::All;
            }

            io::save_mesh(filename, mesh, opts);
        },
        "filename"_a,
        "mesh"_a,
        "binary"_a = true,
        "exact_match"_a = true,
        "selected_attributes"_a = nb::none(),
        R"(Save mesh to file.

Filename extension determines the file format. Supported formats are: `obj`, `ply`, `msh`, `glb` and `gltf`.

:param filename: The output file name.
:param mesh: The input mesh.
:param binary: Whether to save the mesh in binary format if supported. Defaults to True.
               Only `msh`, `ply` and `glb` support binary format.
:param exact_match:
               Whether to save attributes in their exact form. Some mesh formats may not support all
               the attribute types. If set to False, attributes will be converted to the closest
               supported attribute type. Defaults to True.
:param selected_attributes:
               A list of attribute ids to save. If not specified, all attributes will be saved.
               Defaults to None.)");

    m.def(
        "load_mesh",
        [](const fs::path& filename, bool triangulate) {
            io::LoadOptions opts;
            opts.triangulate = triangulate;
            return io::load_mesh<MeshType>(filename, opts);
        },
        "filename"_a,
        "triangulate"_a = false,
        R"(Load mesh from a file.

:param filename:    The input file name.
:param triangulate: Whether to triangulate the mesh if it is not already triangulated.
                    Defaults to False.

:return SurfaceMesh: The mesh object extracted from the input string.)");

    m.def("load_simple_scene", &io::load_simple_scene<SceneType>, "filename"_a, "optinos"_a);

    m.def(
        "save_simple_scene",
        &io::save_simple_scene<Scalar, Index, 3>,
        "filename"_a,
        "scene"_a,
        "options"_a);

    m.def(
        "mesh_to_string",
        [](const MeshType& mesh,
           std::string_view format,
           bool binary,
           bool exact_match,
           std::optional<std::vector<AttributeId>> selected_attributes) {
            std::stringstream ss;

            io::SaveOptions opts;
            opts.encoding = binary ? io::FileEncoding::Binary : io::FileEncoding::Ascii;
            opts.attribute_conversion_policy =
                exact_match ? io::SaveOptions::AttributeConversionPolicy::ExactMatchOnly
                            : io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
            if (selected_attributes.has_value()) {
                opts.selected_attributes = std::move(selected_attributes.value());
                opts.output_attributes = io::SaveOptions::OutputAttributes::SelectedOnly;
            } else {
                opts.output_attributes = io::SaveOptions::OutputAttributes::All;
            }

            if (format == "obj") {
                io::save_mesh_obj(ss, mesh, opts);
            } else if (format == "ply") {
                io::save_mesh_ply(ss, mesh, opts);
            } else if (format == "msh") {
                io::save_mesh_msh(ss, mesh, opts);
            } else if (format == "gltf") {
                opts.encoding = io::FileEncoding::Ascii;
                io::save_mesh_gltf(ss, mesh, opts);
            } else if (format == "glb") {
                opts.encoding = io::FileEncoding::Binary;
                io::save_mesh_gltf(ss, mesh, opts);
            } else {
                throw std::invalid_argument(fmt::format("Unsupported format: {}", format));
            }
            // TODO: switch to ss.view() when C++20 is available.
            std::string data = ss.str();
            return nb::bytes(data.data(), data.size());
        },
        "mesh"_a,
        "format"_a = "ply",
        "binary"_a = true,
        "exact_match"_a = true,
        "selected_attributes"_a = nb::none(),
        R"(Convert a mesh to a binary string based on specified format.

:param mesh: The input mesh.
:param format: Format to use. Supported formats are "obj", "ply", "gltf" and "msh".
:param binary: Whether to save the mesh in binary format if supported. Defaults to True.
               Only `msh`, `ply` and `glb` support binary format.
:param exact_match:
               Whether to save attributes in their exact form. Some mesh formats may not support all
               the attribute types. If set to False, attributes will be converted to the closest
               supported attribute type. Defaults to True.
:param selected_attributes:
               A list of attribute ids to save. If not specified, all attributes will be saved.
               Defaults to None.

:return str: The string representing the input mesh.)");

    m.def(
        "string_to_mesh",
        [](nb::bytes data, bool triangulate) {
            std::stringstream ss;
            ss.write(data.c_str(), data.size());
            io::LoadOptions opts;
            opts.triangulate = triangulate;
            return io::load_mesh<MeshType>(ss, opts);
        },
        "data"_a,
        "triangulate"_a = false,
        R"(Convert a binary string to a mesh.

The binary string should use one of the supported formats. Supported formats include `obj`, `ply`,
`gltf`, `glb` and `msh`. Format is automatically detected.

:param data:        A binary string representing the mesh data in a supported format.
:param triangulate: Whether to triangulate the mesh if it is not already triangulated.
                    Defaults to False.

:return SurfaceMesh: The mesh object extracted from the input string.)");
}

} // namespace lagrange::python
