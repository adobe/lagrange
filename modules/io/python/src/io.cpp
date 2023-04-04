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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_simple_scene.h>
#include <lagrange/scene/SimpleScene.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <string_view>

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

std::string to_string(io::FileEncoding encoding)
{
    switch (encoding) {
    case io::FileEncoding::Binary: return "Binary";
    case io::FileEncoding::Ascii: return "Ascii";
    default: throw Error("Unsupported encoding!");
    }
}

std::string to_string(io::SaveOptions::OutputAttributes output_attr)
{
    switch (output_attr) {
    case io::SaveOptions::OutputAttributes::All: return "All";
    case io::SaveOptions::OutputAttributes::SelectedOnly: return "SelectedOnly";
    default: throw Error("Unsupported output attributes enum!");
    }
}

std::string to_string(io::SaveOptions::AttributeConversionPolicy policy)
{
    switch (policy) {
    case io::SaveOptions::AttributeConversionPolicy::ExactMatchOnly: return "ExactMatchOnly";
    case io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded: return "ConvertAsNeeded";
    default: throw Error("Unsupported attribute conversion policy!");
    }
}

void populate_io_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;
    using SceneType = scene::SimpleScene<Scalar, Index, 3>;

    nb::enum_<io::FileEncoding>(m, "FileEncoding")
        .value("Binary", io::FileEncoding::Binary)
        .value("Ascii", io::FileEncoding::Ascii);

    nb::enum_<io::SaveOptions::OutputAttributes>(m, "OutputAttributes")
        .value("All", io::SaveOptions::OutputAttributes::All)
        .value("SelectedOnly", io::SaveOptions::OutputAttributes::SelectedOnly);

    nb::enum_<io::SaveOptions::AttributeConversionPolicy>(m, "AttributeConversionPolicy")
        .value("ExactMatchOnly", io::SaveOptions::AttributeConversionPolicy::ExactMatchOnly)
        .value("ConvertAsNeeded", io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded);

    nb::class_<io::SaveOptions>(m, "SaveOptions")
        .def(nb::init<>())
        .def(
            "__repr__",
            [](const io::SaveOptions& self) {
                return fmt::format(
                    R"(<lagrange.io.SaveOptions object at {}>:
encoding: {}
output_attributes: {}
selected_attributes: [{}]
attribute_conversion_policy: {})",
                    fmt::ptr(&self),
                    to_string(self.encoding),
                    to_string(self.output_attributes),
                    fmt::join(self.selected_attributes, ","),
                    to_string(self.attribute_conversion_policy));
            })
        .def_rw("encoding", &io::SaveOptions::encoding)
        .def_rw("output_attributes", &io::SaveOptions::output_attributes)
        .def_rw("selected_attributes", &io::SaveOptions::selected_attributes)
        .def_rw("attribute_conversion_policy", &io::SaveOptions::attribute_conversion_policy);

    nb::class_<io::LoadOptions>(m, "LoadOptions")
        .def(nb::init<>())
        .def(
            "__repr__",
            [](const io::LoadOptions& self) {
                return fmt::format(
                    R"(<lagrange.io.LoadOptions object at {}>:
triangulate: {}
load_normals: {}
load_tangents: {}
load_uvs: {}
load_materials: {}
load_vertex_colors: {}
load_object_id: {}
search_path: {}
)",
                    fmt::ptr(&self),
                    self.triangulate,
                    self.load_normals,
                    self.load_tangents,
                    self.load_uvs,
                    self.load_materials,
                    self.load_vertex_colors,
                    self.load_object_id,
                    self.search_path.string());
            })
        .def_rw("triangulate", &io::LoadOptions::triangulate)
        .def_rw("load_normals", &io::LoadOptions::load_normals)
        .def_rw("load_tangents", &io::LoadOptions::load_tangents)
        .def_rw("load_uvs", &io::LoadOptions::load_uvs)
        .def_rw("load_materials", &io::LoadOptions::load_materials)
        .def_rw("load_vertex_colors", &io::LoadOptions::load_vertex_colors)
        .def_rw("load_object_id", &io::LoadOptions::load_object_id)
        .def_rw("search_path", &io::LoadOptions::search_path);

    m.def("load_mesh", &io::load_mesh<MeshType>, "filename"_a, "options"_a = io::LoadOptions());
    m.def(
        "save_mesh",
        &io::save_mesh<Scalar, Index>,
        "filename"_a,
        "mesh"_a,
        "options"_a = io::SaveOptions());
    m.def("load_simple_scene", &io::load_simple_scene<SceneType>, "filename"_a, "optinos"_a);
    m.def(
        "save_simple_scene",
        &io::save_simple_scene<Scalar, Index, 3>,
        "filename"_a,
        "scene"_a,
        "options"_a);
}

} // namespace lagrange::python
