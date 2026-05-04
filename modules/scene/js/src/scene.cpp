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

#include <lagrange/scene/scene_convert.h>

#include <emscripten/bind.h>

namespace lagrange::js::bind {
namespace {

SceneType mesh_to_scene(MeshType mesh)
{
    return scene::mesh_to_scene<Scalar, Index>(std::move(mesh));
}

MeshType scene_to_mesh(const SceneType& scene)
{
    return scene::scene_to_mesh<Scalar, Index>(scene);
}

} // namespace
} // namespace lagrange::js::bind

EMSCRIPTEN_BINDINGS(lagrange_scene)
{
    using namespace emscripten;
    using namespace lagrange::js::bind;

    class_<SceneType>("Scene")
        .function("getNumMeshes", optional_override([](const SceneType& s) -> size_t {
                      return s.meshes.size();
                  }))
        .function("getNumNodes", optional_override([](const SceneType& s) -> size_t {
                      return s.nodes.size();
                  }));

    function("meshToScene", &mesh_to_scene);
    function("sceneToMesh", &scene_to_mesh);
}
