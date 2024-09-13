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
#include <lagrange/io/load_scene.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_mesh_gltf.h>
#include <lagrange/io/save_mesh_msh.h>
#include <lagrange/io/save_mesh_obj.h>
#include <lagrange/io/save_mesh_ply.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/io/save_simple_scene.h>
#include <lagrange/scene/Scene.h>
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
    using SimpleSceneType = scene::SimpleScene<Scalar, Index, 3>;
    using SceneType = lagrange::scene::Scene<Scalar, Index>;

    nb::class_<io::LoadOptions>(m, "LoadOptions")
        .def(nb::init<>())
        .def_rw("triangulate", &io::LoadOptions::triangulate)
        .def_rw("load_normals", &io::LoadOptions::load_normals)
        .def_rw("load_tangents", &io::LoadOptions::load_tangents)
        .def_rw("load_uvs", &io::LoadOptions::load_uvs)
        .def_rw("load_weights", &io::LoadOptions::load_weights)
        .def_rw("load_materials", &io::LoadOptions::load_materials)
        .def_rw("load_vertex_colors", &io::LoadOptions::load_vertex_colors)
        .def_rw("load_object_ids", &io::LoadOptions::load_object_ids)
        .def_rw("search_path", &io::LoadOptions::search_path);
    //.def_rw("extension_converters", &io::LoadOptions::extension_converters);

    nb::enum_<io::FileEncoding>(m, "FileEncoding")
        .value("Binary", io::FileEncoding::Binary)
        .value("Ascii", io::FileEncoding::Ascii);
    nb::class_<io::SaveOptions> save_options(m, "SaveOptions");
    save_options.def(nb::init<>())
        .def_rw("encoding", &io::SaveOptions::encoding)
        .def_rw("output_attributes", &io::SaveOptions::output_attributes)
        .def_rw("selected_attributes", &io::SaveOptions::selected_attributes)
        .def_rw("attribute_conversion_policy", &io::SaveOptions::attribute_conversion_policy)
        .def_rw("embed_images", &io::SaveOptions::embed_images);
    //.def_rw("extension_converters", &io::SaveOptions::extension_converters);
    nb::enum_<io::SaveOptions::OutputAttributes>(save_options, "OutputAttributes")
        .value("All", io::SaveOptions::OutputAttributes::All)
        .value("SelectedOnly", io::SaveOptions::OutputAttributes::SelectedOnly);
    nb::enum_<io::SaveOptions::AttributeConversionPolicy>(save_options, "AttributeConversionPolicy")
        .value("ExactMatchOnly", io::SaveOptions::AttributeConversionPolicy::ExactMatchOnly)
        .value("ConvertAsNeeded", io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded);


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
:param binary: Whether to save the mesh in binary format if supported. Defaults to True. Only `msh`, `ply` and `glb` support binary format.
:param exact_match: Whether to save attributes in their exact form. Some mesh formats may not support all the attribute types. If set to False, attributes will be converted to the closest supported attribute type. Defaults to True.
:param selected_attributes: A list of attribute ids to save. If not specified, all attributes will be saved. Defaults to None.)");

    m.def(
        "load_mesh",
        [](const fs::path& filename,
           bool triangulate,
           bool load_normals,
           bool load_tangents,
           bool load_uvs,
           bool load_weights,
           bool load_materials,
           bool load_vertex_colors,
           bool load_object_ids,
           bool load_images,
           bool stitch_vertices,
           const fs::path& search_path) {
            io::LoadOptions opts;
            opts.triangulate = triangulate;
            opts.load_normals = load_normals;
            opts.load_tangents = load_tangents;
            opts.load_uvs = load_uvs;
            opts.load_weights = load_weights;
            opts.load_materials = load_materials;
            opts.load_vertex_colors = load_vertex_colors;
            opts.load_object_ids = load_object_ids;
            opts.load_images = load_images;
            opts.stitch_vertices = stitch_vertices;
            opts.search_path = search_path;
            return io::load_mesh<MeshType>(filename, opts);
        },
        "filename"_a,
        "triangulate"_a = io::LoadOptions().triangulate,
        "load_normals"_a = io::LoadOptions().load_normals,
        "load_tangents"_a = io::LoadOptions().load_tangents,
        "load_uvs"_a = io::LoadOptions().load_uvs,
        "load_weights"_a = io::LoadOptions().load_weights,
        "load_materials"_a = io::LoadOptions().load_materials,
        "load_vertex_colors"_a = io::LoadOptions().load_vertex_colors,
        "load_object_ids"_a = io::LoadOptions().load_object_ids,
        "load_images"_a = io::LoadOptions().load_images,
        "stitch_vertices"_a = io::LoadOptions().stitch_vertices,
        "search_path"_a = io::LoadOptions().search_path,
        R"(Load mesh from a file.

:param filename:           The input file name.
:param triangulate:        Whether to triangulate the mesh if it is not already triangulated. Defaults to False.
:param load_normals:       Whether to load vertex normals from mesh if available. Defaults to True.
:param load_tangents:      Whether to load tangents and bitangents from mesh if available. Defaults to True.
:param load_uvs:           Whether to load texture coordinates from mesh if available. Defaults to True.
:param load_weights:       Whether to load skinning weights attributes from mesh if available. Defaults to True.
:param load_materials:     Whether to load material ids from mesh if available. Defaults to True.
:param load_vertex_colors: Whether to load vertex colors from mesh if available. Defaults to True.
:param load_object_id:     Whether to load object ids from mesh if available. Defaults to True.
:param load_images:        Whether to load external images if available. Defaults to True.
:param stitch_vertices:    Whether to stitch vertices based on position. Defaults to False.
:param search_path:        Optional search path for external references (e.g. .mtl, .bin, etc.). Defaults to None.

:return SurfaceMesh: The mesh object extracted from the input string.)");

    m.def(
        "load_simple_scene",
        [](const fs::path& filename, bool triangulate, std::optional<fs::path> search_path) {
            io::LoadOptions opts;
            opts.triangulate = triangulate;
            if (search_path.has_value()) {
                opts.search_path = std::move(search_path.value());
            }
            return io::load_simple_scene<SimpleSceneType>(filename, opts);
        },
        "filename"_a,
        "triangulate"_a = false,
        "search_path"_a = nb::none(),
        R"(Load a simple scene from file.

:param filename:    The input file name.
:param triangulate: Whether to triangulate the scene if it is not already triangulated. Defaults to False.
:param search_path: Optional search path for external references (e.g. .mtl, .bin, etc.). Defaults to None.

:return SimpleScene: The scene object extracted from the input string.)");

    m.def(
        "save_simple_scene",
        [](const fs::path& filename, const SimpleSceneType& scene, bool binary) {
            io::SaveOptions opts;
            opts.encoding = binary ? io::FileEncoding::Binary : io::FileEncoding::Ascii;
            io::save_simple_scene(filename, scene, opts);
        },
        "filename"_a,
        "scene"_a,
        "binary"_a = true,
        R"(Save a simple scene to file.

:param filename: The output file name.
:param scene:    The input scene.
:param binary:   Whether to save the scene in binary format if supported. Defaults to True. Only `glb` supports binary format.)");

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
:param binary: Whether to save the mesh in binary format if supported. Defaults to True. Only `msh`, `ply` and `glb` support binary format.
:param exact_match: Whether to save attributes in their exact form. Some mesh formats may not support all the attribute types. If set to False, attributes will be converted to the closest supported attribute type. Defaults to True.
:param selected_attributes: A list of attribute ids to save. If not specified, all attributes will be saved. Defaults to None.

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
`gltf`, `glb`, `fbx` and `msh`. Format is automatically detected.

:param data:        A binary string representing the mesh data in a supported format.
:param triangulate: Whether to triangulate the mesh if it is not already triangulated. Defaults to False.

:return SurfaceMesh: The mesh object extracted from the input string.)");

    m.def(
        "load_scene",
        [](const fs::path& filename, const io::LoadOptions& options) {
            return io::load_scene<SceneType>(filename, options);
        },
        "filename"_a,
        "options"_a = io::LoadOptions(),
        R"(Load a scene.

:param filename:    The input file name.
:param options:     Load scene options. Check the class for more details.

:return Scene: The loaded scene object.)");

    m.def(
        "string_to_scene",
        [](nb::bytes data, bool triangulate) {
            std::stringstream ss;
            ss.write(data.c_str(), data.size());
            io::LoadOptions opts;
            opts.triangulate = triangulate;
            return io::load_scene<SceneType>(ss, opts);
        },
        "data"_a,
        "triangulate"_a = false,
        R"(Convert a binary string to a scene.

The binary string should use one of the supported formats (i.e. `gltf`, `glb` and `fbx`).

:param data:        A binary string representing the scene data in a supported format.
:param triangulate: Whether to triangulate the scene if it is not already triangulated. Defaults to False.

:return Scene: The scene object extracted from the input string.)");

    m.def(
        "save_scene",
        [](const fs::path& filename,
           const scene::Scene<Scalar, Index>& scene,
           const io::SaveOptions& options) { io::save_scene(filename, scene, options); },
        "filename"_a,
        "scene"_a,
        "options"_a = io::SaveOptions(),
        R"(Save a scene.

:param filename:    The output file name.
:param scene:       The scene to save.
:param options:     Save options. Check the class for more details.)");

    m.def(
        "scene_to_string",
        [](const scene::Scene<Scalar, Index>& scene,
           std::string_view format,
           bool binary,
           bool exact_match,
           bool embed_images,
           std::optional<std::vector<AttributeId>> selected_attributes) {
            lagrange::io::SaveOptions opts;
            opts.encoding = binary ? io::FileEncoding::Binary : io::FileEncoding::Ascii;
            opts.attribute_conversion_policy =
                exact_match ? io::SaveOptions::AttributeConversionPolicy::ExactMatchOnly
                            : io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
            opts.embed_images = embed_images;
            if (selected_attributes.has_value()) {
                opts.selected_attributes = std::move(selected_attributes.value());
                opts.output_attributes = io::SaveOptions::OutputAttributes::SelectedOnly;
            } else {
                opts.output_attributes = io::SaveOptions::OutputAttributes::All;
            }

            std::stringstream ss;
            if (format == "gltf") {
                opts.encoding = io::FileEncoding::Ascii;
                io::save_scene(ss, scene, lagrange::io::FileFormat::Gltf, opts);
            } else if (format == "glb") {
                opts.encoding = io::FileEncoding::Binary;
                io::save_scene(ss, scene, lagrange::io::FileFormat::Gltf, opts);
            } else {
                throw std::invalid_argument(fmt::format("Unsupported format: {}", format));
            }

            // TODO: switch to ss.view() when C++20 is available.
            std::string data = ss.str();
            return nb::bytes(data.data(), data.size());
        },
        "scene"_a,
        "format"_a,
        "binary"_a = true,
        "exact_match"_a = true,
        "embed_images"_a = false,
        "selected_attributes"_a = nb::none(),
        R"(Convert a scene to a binary string based on specified format.

:param scene:    The input scene.
:param format:   Format to use. Supported formats are "gltf" and "glb".
:param binary:   Whether to save the scene in binary format if supported. Defaults to True. Only `glb` supports binary format.
:param exact_match: Whether to save attributes in their exact form. Some mesh formats may not support all the attribute types. If set to False, attributes will be converted to the closest supported attribute type. Defaults to True.
:param selected_attributes: A list of attribute ids to save. If not specified, all attributes will be saved. Defaults to None.

:return str: The string representing the input scene.)");
}

} // namespace lagrange::python
