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

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/strings.h>

#include <lagrange/io/save_mesh_gltf.h>
#include <lagrange/io/save_mesh_msh.h>
#include <lagrange/io/save_mesh_obj.h>
#include <lagrange/io/save_mesh_ply.h>

namespace lagrange::io {

template <typename Scalar, typename Index>
void save_mesh(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    std::string ext = to_lower(filename.extension().string());
    if (ext == ".obj") {
        save_mesh_obj(filename, mesh, options);
    } else if (ext == ".ply") {
        save_mesh_ply(filename, mesh, options);
    } else if (ext == ".msh") {
        save_mesh_msh(filename, mesh, options);
    } else if (ext == ".gltf" || ext == ".glb") {
        save_mesh_gltf(filename, mesh, options);
    } else {
        la_runtime_assert(false, string_format("Unrecognized filetype: {}!", ext));
    }
}

#define LA_X_save_mesh(_, S, I)        \
    template void save_mesh(           \
        const fs::path& filename,      \
        const SurfaceMesh<S, I>& mesh, \
        const SaveOptions& options);
LA_SURFACE_MESH_X(save_mesh, 0);

} // namespace lagrange::io
