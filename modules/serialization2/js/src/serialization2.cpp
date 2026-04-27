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

#include <lagrange/serialization/serialize_mesh.h>
#include <lagrange/serialization/serialize_scene.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <vector>

namespace {

using namespace lagrange;
using namespace lagrange::js::bind;
using val = emscripten::val;

} // namespace

EMSCRIPTEN_BINDINGS(lagrange_serialization)
{
    using namespace emscripten;

    function(
        "serializeMesh",
        +[](const MeshType& mesh, val opts) -> val {
            serialization::SerializeOptions o;
            apply_opt(opts, "compress", o.compress);
            apply_opt(opts, "compressionLevel", o.compression_level);
            apply_opt(opts, "numThreads", o.num_threads);

            auto buf = serialization::serialize_mesh(mesh, o);

            return val::global("Uint8Array").new_(typed_memory_view(buf.size(), buf.data()));
        });

    function(
        "deserializeMesh",
        +[](const val& data, val opts) -> MeshType {
            const auto length = data["length"].as<size_t>();
            std::vector<uint8_t> buf(length);
            val memory_view = val(typed_memory_view(length, buf.data()));
            memory_view.call<void>("set", data);

            serialization::DeserializeOptions o;
            apply_opt(opts, "allowSceneConversion", o.allow_scene_conversion);
            apply_opt(opts, "allowTypeCast", o.allow_type_cast);
            apply_opt(opts, "quiet", o.quiet);

            return serialization::deserialize_mesh<MeshType>(
                span<const uint8_t>(buf.data(), buf.size()),
                o);
        });

    function(
        "serializeScene",
        +[](const SceneType& scene, val opts) -> val {
            serialization::SerializeOptions o;
            apply_opt(opts, "compress", o.compress);
            apply_opt(opts, "compressionLevel", o.compression_level);
            apply_opt(opts, "numThreads", o.num_threads);

            auto buf = serialization::serialize_scene(scene, o);

            return val::global("Uint8Array").new_(typed_memory_view(buf.size(), buf.data()));
        });

    function(
        "deserializeScene",
        +[](const val& data, val opts) -> SceneType {
            const auto length = data["length"].as<size_t>();
            std::vector<uint8_t> buf(length);
            val memory_view = val(typed_memory_view(length, buf.data()));
            memory_view.call<void>("set", data);

            serialization::DeserializeOptions o;
            apply_opt(opts, "allowSceneConversion", o.allow_scene_conversion);
            apply_opt(opts, "allowTypeCast", o.allow_type_cast);
            apply_opt(opts, "quiet", o.quiet);

            return serialization::deserialize_scene<SceneType>(
                span<const uint8_t>(buf.data(), buf.size()),
                o);
        });
}
