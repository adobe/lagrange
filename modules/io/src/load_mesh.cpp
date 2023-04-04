/*
 * Copyright 2021 Adobe. All rights reserved.
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
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/utils/strings.h>

#include <lagrange/io/load_mesh_gltf.h>
#include <lagrange/io/load_mesh_msh.h>
#include <lagrange/io/load_mesh_obj.h>
#include <lagrange/io/load_mesh_ply.h>
#ifdef LAGRANGE_WITH_ASSIMP
    #include <lagrange/io/load_mesh_assimp.h>
#endif

namespace lagrange::io {

template <
    typename MeshType,
    std::enable_if_t<!lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* /* = nullptr*/>
MeshType load_mesh(const fs::path& filename, const LoadOptions& options)
{
    std::string ext = to_lower(filename.extension().string());
    if (ext == ".obj") {
        return load_mesh_obj<MeshType>(filename, options);
    } else if (ext == ".ply") {
        return load_mesh_ply<MeshType>(filename, options);
    } else if (ext == ".msh") {
        return load_mesh_msh<MeshType>(filename, options);
    } else if (ext == ".gltf" || ext == ".glb") {
        return load_mesh_gltf<MeshType>(filename, options);
    } else {
#ifdef LAGRANGE_WITH_ASSIMP
        return load_mesh_assimp<MeshType>(filename, options);
#else
        throw std::runtime_error(
            "Unsupported format. You may want to compile with LAGRANGE_WITH_ASSIMP=ON");
#endif
    }
}

#define LA_X_load_mesh(_, S, I)                                       \
    template SurfaceMesh<S, I> load_mesh<SurfaceMesh<S, I>, nullptr>( \
        const fs::path& filename,                                     \
        const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh, 0);

} // namespace lagrange::io
