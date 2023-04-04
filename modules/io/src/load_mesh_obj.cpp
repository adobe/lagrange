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
#include <lagrange/io/load_mesh_obj.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/io/internal/load_obj.h>
#include <lagrange/utils/Error.h>

#include <istream>

namespace lagrange::io {

template <typename MeshType>
MeshType load_mesh_obj(const fs::path& filename, const LoadOptions& options)
{
    auto ret = internal::load_mesh_obj<MeshType>(filename, options);
    if (!ret.success) {
        throw Error(fmt::format("Failed to load mesh from file: '{}'", filename.string()));
    }
    return std::move(ret.mesh);
}

template <typename MeshType>
MeshType load_mesh_obj(std::istream& input_stream, const LoadOptions& options)
{
    auto ret = internal::load_mesh_obj<MeshType>(input_stream, options);
    if (!ret.success) {
        throw Error("Failed to load mesh from stream");
    }
    return std::move(ret.mesh);
}

#define LA_X_load_mesh_obj(_, S, I)           \
    template SurfaceMesh<S, I> load_mesh_obj( \
        const fs::path& filename,             \
        const LoadOptions& options);          \
    template SurfaceMesh<S, I> load_mesh_obj(std::istream&, const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh_obj, 0)

} // namespace lagrange::io
