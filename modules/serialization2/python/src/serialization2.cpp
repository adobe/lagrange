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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/python/binding.h>
#include <lagrange/scene/Scene.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/serialization/serialize_mesh.h>
#include <lagrange/serialization/serialize_scene.h>
#include <lagrange/serialization/serialize_simple_scene.h>
#include <lagrange/serialization/types.h>
#include <lagrange/utils/span.h>

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

void populate_serialization2_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;
    using SimpleSceneType = scene::SimpleScene<Scalar, Index, 3>;
    using SceneType = scene::Scene<Scalar, Index>;

    // Format version constants
    m.def(
        "mesh_format_version",
        &serialization::mesh_format_version,
        "Return the current mesh serialization format version.");
    m.def(
        "simple_scene_format_version",
        &serialization::simple_scene_format_version,
        "Return the current simple scene serialization format version.");
    m.def(
        "scene_format_version",
        &serialization::scene_format_version,
        "Return the current scene serialization format version.");

    // -----------------------------------------------------------------------
    // Mesh serialization
    // -----------------------------------------------------------------------

    m.def(
        "serialize_mesh",
        [](const MeshType& mesh, bool compress, int compression_level, unsigned num_threads) {
            serialization::SerializeOptions opts;
            opts.compress = compress;
            opts.compression_level = compression_level;
            opts.num_threads = num_threads;
            auto buf = serialization::serialize_mesh(mesh, opts);
            return nb::bytes(reinterpret_cast<const char*>(buf.data()), buf.size());
        },
        "mesh"_a,
        "compress"_a = serialization::SerializeOptions().compress,
        "compression_level"_a = serialization::SerializeOptions().compression_level,
        "num_threads"_a = serialization::SerializeOptions().num_threads,
        R"(Serialize a mesh to a bytes buffer.

:param mesh:              The mesh to serialize.
:param compress:          Enable zstd compression. Defaults to True.
:param compression_level: Zstd compression level (1-22). Defaults to 3.
:param num_threads:       Number of compression threads. 0 = automatic, 1 = single-threaded. Defaults to 0.

:return bytes: A bytes object containing the serialized mesh.)");

    m.def(
        "deserialize_mesh",
        [](nb::bytes data, bool allow_scene_conversion, bool allow_type_cast, bool quiet) {
            serialization::DeserializeOptions opts;
            opts.allow_scene_conversion = allow_scene_conversion;
            opts.allow_type_cast = allow_type_cast;
            opts.quiet = quiet;
            span<const uint8_t> buf(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
            return serialization::deserialize_mesh<MeshType>(buf, opts);
        },
        "data"_a,
        "allow_scene_conversion"_a = serialization::DeserializeOptions().allow_scene_conversion,
        "allow_type_cast"_a = serialization::DeserializeOptions().allow_type_cast,
        "quiet"_a = serialization::DeserializeOptions().quiet,
        R"(Deserialize a mesh from a bytes buffer.

Auto-detects compression. If the buffer contains a SimpleScene or Scene, it can be converted
when allow_scene_conversion is enabled. Type casting can be enabled via allow_type_cast.

:param data:                   A bytes object containing the serialized data.
:param allow_scene_conversion: Allow converting between meshes and scenes. Defaults to False.
:param allow_type_cast:        Allow casting scalar and index types. Defaults to False.
:param quiet:                  Suppress warnings. Defaults to False.

:return SurfaceMesh: The deserialized mesh.)");

    m.def(
        "save_mesh",
        [](const fs::path& filename,
           const MeshType& mesh,
           bool compress,
           int compression_level,
           unsigned num_threads) {
            serialization::SerializeOptions opts;
            opts.compress = compress;
            opts.compression_level = compression_level;
            opts.num_threads = num_threads;
            serialization::save_mesh(filename, mesh, opts);
        },
        "filename"_a,
        "mesh"_a,
        "compress"_a = serialization::SerializeOptions().compress,
        "compression_level"_a = serialization::SerializeOptions().compression_level,
        "num_threads"_a = serialization::SerializeOptions().num_threads,
        R"(Save a mesh to a binary file.

:param filename:          Output file path.
:param mesh:              The mesh to save.
:param compress:          Enable zstd compression. Defaults to True.
:param compression_level: Zstd compression level (1-22). Defaults to 3.
:param num_threads:       Number of compression threads. 0 = automatic, 1 = single-threaded. Defaults to 0.)");

    m.def(
        "load_mesh",
        [](const fs::path& filename,
           bool allow_scene_conversion,
           bool allow_type_cast,
           bool quiet) {
            serialization::DeserializeOptions opts;
            opts.allow_scene_conversion = allow_scene_conversion;
            opts.allow_type_cast = allow_type_cast;
            opts.quiet = quiet;
            return serialization::load_mesh<MeshType>(filename, opts);
        },
        "filename"_a,
        "allow_scene_conversion"_a = serialization::DeserializeOptions().allow_scene_conversion,
        "allow_type_cast"_a = serialization::DeserializeOptions().allow_type_cast,
        "quiet"_a = serialization::DeserializeOptions().quiet,
        R"(Load a mesh from a binary file.

Auto-detects compression. If the file contains a SimpleScene or Scene, it can be converted
when allow_scene_conversion is enabled. Type casting can be enabled via allow_type_cast.

:param filename:               Input file path.
:param allow_scene_conversion: Allow converting between meshes and scenes. Defaults to False.
:param allow_type_cast:        Allow casting scalar and index types. Defaults to False.
:param quiet:                  Suppress warnings. Defaults to False.

:return SurfaceMesh: The loaded mesh.)");

    // -----------------------------------------------------------------------
    // SimpleScene serialization
    // -----------------------------------------------------------------------

    m.def(
        "serialize_simple_scene",
        [](const SimpleSceneType& scene,
           bool compress,
           int compression_level,
           unsigned num_threads) {
            serialization::SerializeOptions opts;
            opts.compress = compress;
            opts.compression_level = compression_level;
            opts.num_threads = num_threads;
            auto buf = serialization::serialize_simple_scene(scene, opts);
            return nb::bytes(reinterpret_cast<const char*>(buf.data()), buf.size());
        },
        "scene"_a,
        "compress"_a = serialization::SerializeOptions().compress,
        "compression_level"_a = serialization::SerializeOptions().compression_level,
        "num_threads"_a = serialization::SerializeOptions().num_threads,
        R"(Serialize a simple scene to a bytes buffer.

:param scene:             The simple scene to serialize.
:param compress:          Enable zstd compression. Defaults to True.
:param compression_level: Zstd compression level (1-22). Defaults to 3.
:param num_threads:       Number of compression threads. 0 = automatic, 1 = single-threaded. Defaults to 0.

:return bytes: A bytes object containing the serialized simple scene.)");

    m.def(
        "deserialize_simple_scene",
        [](nb::bytes data, bool allow_scene_conversion, bool allow_type_cast, bool quiet) {
            serialization::DeserializeOptions opts;
            opts.allow_scene_conversion = allow_scene_conversion;
            opts.allow_type_cast = allow_type_cast;
            opts.quiet = quiet;
            span<const uint8_t> buf(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
            return serialization::deserialize_simple_scene<SimpleSceneType>(buf, opts);
        },
        "data"_a,
        "allow_scene_conversion"_a = serialization::DeserializeOptions().allow_scene_conversion,
        "allow_type_cast"_a = serialization::DeserializeOptions().allow_type_cast,
        "quiet"_a = serialization::DeserializeOptions().quiet,
        R"(Deserialize a simple scene from a bytes buffer.

Auto-detects compression. If the buffer contains a SurfaceMesh or Scene, it can be converted
when allow_scene_conversion is enabled. Type casting can be enabled via allow_type_cast.

:param data:                   A bytes object containing the serialized data.
:param allow_scene_conversion: Allow converting between meshes and scenes. Defaults to False.
:param allow_type_cast:        Allow casting scalar and index types. Defaults to False.
:param quiet:                  Suppress warnings. Defaults to False.

:return SimpleScene: The deserialized simple scene.)");

    m.def(
        "save_simple_scene",
        [](const fs::path& filename,
           const SimpleSceneType& scene,
           bool compress,
           int compression_level,
           unsigned num_threads) {
            serialization::SerializeOptions opts;
            opts.compress = compress;
            opts.compression_level = compression_level;
            opts.num_threads = num_threads;
            serialization::save_simple_scene(filename, scene, opts);
        },
        "filename"_a,
        "scene"_a,
        "compress"_a = serialization::SerializeOptions().compress,
        "compression_level"_a = serialization::SerializeOptions().compression_level,
        "num_threads"_a = serialization::SerializeOptions().num_threads,
        R"(Save a simple scene to a binary file.

:param filename:          Output file path.
:param scene:             The simple scene to save.
:param compress:          Enable zstd compression. Defaults to True.
:param compression_level: Zstd compression level (1-22). Defaults to 3.
:param num_threads:       Number of compression threads. 0 = automatic, 1 = single-threaded. Defaults to 0.)");

    m.def(
        "load_simple_scene",
        [](const fs::path& filename,
           bool allow_scene_conversion,
           bool allow_type_cast,
           bool quiet) {
            serialization::DeserializeOptions opts;
            opts.allow_scene_conversion = allow_scene_conversion;
            opts.allow_type_cast = allow_type_cast;
            opts.quiet = quiet;
            return serialization::load_simple_scene<SimpleSceneType>(filename, opts);
        },
        "filename"_a,
        "allow_scene_conversion"_a = serialization::DeserializeOptions().allow_scene_conversion,
        "allow_type_cast"_a = serialization::DeserializeOptions().allow_type_cast,
        "quiet"_a = serialization::DeserializeOptions().quiet,
        R"(Load a simple scene from a binary file.

Auto-detects compression. If the file contains a SurfaceMesh or Scene, it can be converted
when allow_scene_conversion is enabled. Type casting can be enabled via allow_type_cast.

:param filename:               Input file path.
:param allow_scene_conversion: Allow converting between meshes and scenes. Defaults to False.
:param allow_type_cast:        Allow casting scalar and index types. Defaults to False.
:param quiet:                  Suppress warnings. Defaults to False.

:return SimpleScene: The loaded simple scene.)");

    // -----------------------------------------------------------------------
    // Scene serialization
    // -----------------------------------------------------------------------

    m.def(
        "serialize_scene",
        [](const SceneType& scene, bool compress, int compression_level, unsigned num_threads) {
            serialization::SerializeOptions opts;
            opts.compress = compress;
            opts.compression_level = compression_level;
            opts.num_threads = num_threads;
            auto buf = serialization::serialize_scene(scene, opts);
            return nb::bytes(reinterpret_cast<const char*>(buf.data()), buf.size());
        },
        "scene"_a,
        "compress"_a = serialization::SerializeOptions().compress,
        "compression_level"_a = serialization::SerializeOptions().compression_level,
        "num_threads"_a = serialization::SerializeOptions().num_threads,
        R"(Serialize a scene to a bytes buffer.

:param scene:             The scene to serialize.
:param compress:          Enable zstd compression. Defaults to True.
:param compression_level: Zstd compression level (1-22). Defaults to 3.
:param num_threads:       Number of compression threads. 0 = automatic, 1 = single-threaded. Defaults to 0.

:return bytes: A bytes object containing the serialized scene.)");

    m.def(
        "deserialize_scene",
        [](nb::bytes data, bool allow_scene_conversion, bool allow_type_cast, bool quiet) {
            serialization::DeserializeOptions opts;
            opts.allow_scene_conversion = allow_scene_conversion;
            opts.allow_type_cast = allow_type_cast;
            opts.quiet = quiet;
            span<const uint8_t> buf(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
            return serialization::deserialize_scene<SceneType>(buf, opts);
        },
        "data"_a,
        "allow_scene_conversion"_a = serialization::DeserializeOptions().allow_scene_conversion,
        "allow_type_cast"_a = serialization::DeserializeOptions().allow_type_cast,
        "quiet"_a = serialization::DeserializeOptions().quiet,
        R"(Deserialize a scene from a bytes buffer.

Auto-detects compression. If the buffer contains a SurfaceMesh or SimpleScene, it can be converted
when allow_scene_conversion is enabled. Type casting can be enabled via allow_type_cast.

:param data:                   A bytes object containing the serialized data.
:param allow_scene_conversion: Allow converting between meshes and scenes. Defaults to False.
:param allow_type_cast:        Allow casting scalar and index types. Defaults to False.
:param quiet:                  Suppress warnings. Defaults to False.

:return Scene: The deserialized scene.)");

    m.def(
        "save_scene",
        [](const fs::path& filename,
           const SceneType& scene,
           bool compress,
           int compression_level,
           unsigned num_threads) {
            serialization::SerializeOptions opts;
            opts.compress = compress;
            opts.compression_level = compression_level;
            opts.num_threads = num_threads;
            serialization::save_scene(filename, scene, opts);
        },
        "filename"_a,
        "scene"_a,
        "compress"_a = serialization::SerializeOptions().compress,
        "compression_level"_a = serialization::SerializeOptions().compression_level,
        "num_threads"_a = serialization::SerializeOptions().num_threads,
        R"(Save a scene to a binary file.

:param filename:          Output file path.
:param scene:             The scene to save.
:param compress:          Enable zstd compression. Defaults to True.
:param compression_level: Zstd compression level (1-22). Defaults to 3.
:param num_threads:       Number of compression threads. 0 = automatic, 1 = single-threaded. Defaults to 0.)");

    m.def(
        "load_scene",
        [](const fs::path& filename,
           bool allow_scene_conversion,
           bool allow_type_cast,
           bool quiet) {
            serialization::DeserializeOptions opts;
            opts.allow_scene_conversion = allow_scene_conversion;
            opts.allow_type_cast = allow_type_cast;
            opts.quiet = quiet;
            return serialization::load_scene<SceneType>(filename, opts);
        },
        "filename"_a,
        "allow_scene_conversion"_a = serialization::DeserializeOptions().allow_scene_conversion,
        "allow_type_cast"_a = serialization::DeserializeOptions().allow_type_cast,
        "quiet"_a = serialization::DeserializeOptions().quiet,
        R"(Load a scene from a binary file.

Auto-detects compression. If the file contains a SurfaceMesh or SimpleScene, it can be converted
when allow_scene_conversion is enabled. Type casting can be enabled via allow_type_cast.

:param filename:               Input file path.
:param allow_scene_conversion: Allow converting between meshes and scenes. Defaults to False.
:param allow_type_cast:        Allow casting scalar and index types. Defaults to False.
:param quiet:                  Suppress warnings. Defaults to False.

:return Scene: The loaded scene.)");
}

} // namespace lagrange::python
