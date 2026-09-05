/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include "bind_types.h"

#include <lagrange/io/load_mesh.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/io/types.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace lagrange::js::bind {
namespace {

template <typename T, typename Loader>
T load_from_buffer_impl(
    const emscripten::val& js_array,
    const std::string& error_tag,
    Loader loader)
{
    const auto length = js_array["length"].as<size_t>();
    std::vector<char> buf(length);
    emscripten::val memory_view = emscripten::val(
        emscripten::typed_memory_view(length, reinterpret_cast<uint8_t*>(buf.data())));
    memory_view.call<void>("set", js_array);

    std::stringstream ss;
    ss.write(buf.data(), static_cast<std::streamsize>(length));
    ss.seekg(0);
    try {
        return loader(ss);
    } catch (const std::exception& e) {
        std::ostringstream diag;
        diag << error_tag << e.what() << " [size=" << length << ", magic=";
        const size_t n = std::min(length, size_t(8));
        for (size_t i = 0; i < n; ++i) {
            if (i) diag << ' ';
            diag << std::hex << std::setw(2) << std::setfill('0')
                 << (static_cast<unsigned>(buf[i]) & 0xff);
        }
        diag << ']';
        emscripten::val::global("console").call<void>("error", diag.str());
        throw;
    }
}

emscripten::val to_uint8array(const std::ostringstream& ss)
{
    const auto& buf = ss.str();
    return emscripten::val::global("Uint8Array")
        .new_(
            emscripten::typed_memory_view(
                buf.size(),
                reinterpret_cast<const uint8_t*>(buf.data())));
}

void parse_load_opts(const emscripten::val& opts, io::LoadOptions& o)
{
    if (opts.isUndefined()) return;
    apply_opt(opts, "triangulate", o.triangulate);
    apply_opt(opts, "loadNormals", o.load_normals);
    apply_opt(opts, "loadTangents", o.load_tangents);
    apply_opt(opts, "loadUvs", o.load_uvs);
    apply_opt(opts, "loadWeights", o.load_weights);
    apply_opt(opts, "loadMaterials", o.load_materials);
    apply_opt(opts, "loadVertexColors", o.load_vertex_colors);
    apply_opt(opts, "loadObjectIds", o.load_object_ids);
    apply_opt(opts, "loadImages", o.load_images);
    apply_opt(opts, "loadLines", o.load_lines);
    apply_opt(opts, "stitchVertices", o.stitch_vertices);
    apply_opt(opts, "quiet", o.quiet);
}

// Overlays any fields present in `opts` onto `o`. Fields absent in `opts` keep their caller-set
// value (or the C++ struct default).
void parse_save_opts(const emscripten::val& opts, io::SaveOptions& o)
{
    if (opts.isUndefined()) return;

    auto enc = opts["encoding"];
    if (!enc.isUndefined()) {
        o.encoding =
            enc.as<std::string>() == "ascii" ? io::FileEncoding::Ascii : io::FileEncoding::Binary;
    }

    auto oa = opts["outputAttributes"];
    if (!oa.isUndefined()) {
        o.output_attributes = oa.as<std::string>() == "selectedOnly"
                                  ? io::SaveOptions::OutputAttributes::SelectedOnly
                                  : io::SaveOptions::OutputAttributes::All;
    }

    auto sel = opts["selectedAttributes"];
    if (!sel.isUndefined()) {
        unsigned len = sel["length"].as<unsigned>();
        o.selected_attributes.reserve(len);
        for (unsigned i = 0; i < len; ++i)
            o.selected_attributes.push_back(sel[i].as<AttributeId>());
    }

    auto acp = opts["attributeConversionPolicy"];
    if (!acp.isUndefined()) {
        o.attribute_conversion_policy =
            acp.as<std::string>() == "convertAsNeeded"
                ? io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded
                : io::SaveOptions::AttributeConversionPolicy::ExactMatchOnly;
    }
    apply_opt(opts, "embedImages", o.embed_images);
    apply_opt(opts, "exportMaterials", o.export_materials);
    apply_opt(opts, "quiet", o.quiet);
}

MeshType load_mesh_from_buffer(const emscripten::val& js_array, const emscripten::val& opts)
{
    io::LoadOptions load_opts;
    parse_load_opts(opts, load_opts);
    return load_from_buffer_impl<MeshType>(
        js_array,
        "load_mesh error: ",
        [&load_opts](std::istream& ss) { return io::load_mesh<MeshType>(ss, load_opts); });
}

emscripten::val
save_mesh_to_buffer(const MeshType& mesh, const std::string& format, const emscripten::val& opts)
{
    io::FileFormat file_format;
    io::SaveOptions save_opts;
    if (format == "obj") {
        file_format = io::FileFormat::Obj;
        save_opts.encoding = io::FileEncoding::Ascii;
    } else if (format == "ply") {
        file_format = io::FileFormat::Ply;
        save_opts.encoding = io::FileEncoding::Binary;
    } else if (format == "glb") {
        file_format = io::FileFormat::Gltf;
        save_opts.encoding = io::FileEncoding::Binary;
    } else if (format == "gltf") {
        file_format = io::FileFormat::Gltf;
        save_opts.encoding = io::FileEncoding::Ascii;
    } else if (format == "msh") {
        file_format = io::FileFormat::Msh;
        save_opts.encoding = io::FileEncoding::Binary;
    } else {
        throw std::invalid_argument("Unsupported mesh format: " + format);
    }
    parse_save_opts(opts, save_opts);
    std::ostringstream ss;
    io::save_mesh(ss, mesh, file_format, save_opts);
    return to_uint8array(ss);
}

SceneType load_scene_from_buffer(const emscripten::val& js_array, const emscripten::val& opts)
{
    io::LoadOptions load_opts;
    parse_load_opts(opts, load_opts);
    return load_from_buffer_impl<SceneType>(
        js_array,
        "load_scene error: ",
        [&load_opts](std::istream& ss) { return io::load_scene<SceneType>(ss, load_opts); });
}

emscripten::val
save_scene_to_buffer(const SceneType& scene, const std::string& format, const emscripten::val& opts)
{
    io::FileFormat file_format;
    io::SaveOptions save_opts;
    if (format == "glb") {
        file_format = io::FileFormat::Gltf;
        save_opts.encoding = io::FileEncoding::Binary;
    } else if (format == "gltf") {
        file_format = io::FileFormat::Gltf;
        save_opts.encoding = io::FileEncoding::Ascii;
    } else if (format == "obj") {
        file_format = io::FileFormat::Obj;
        save_opts.encoding = io::FileEncoding::Ascii;
    } else {
        throw std::invalid_argument("Unsupported scene format: " + format);
    }
    parse_save_opts(opts, save_opts);
    std::ostringstream ss;
    io::save_scene(ss, scene, file_format, save_opts);
    return to_uint8array(ss);
}

} // namespace
} // namespace lagrange::js::bind

EMSCRIPTEN_BINDINGS(lagrange_io)
{
    using namespace emscripten;
    using namespace lagrange::js::bind;

    function("loadMeshFromBuffer", &load_mesh_from_buffer);
    function("saveMeshToBuffer", &save_mesh_to_buffer);
    function("loadSceneFromBuffer", &load_scene_from_buffer);
    function("saveSceneToBuffer", &save_scene_to_buffer);
}
