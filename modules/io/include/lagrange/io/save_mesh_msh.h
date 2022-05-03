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
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/fs/filesystem.h>

#include <ostream>
#include <vector>

namespace lagrange::io {

/**
 * Config options for saving mesh in .msh format.
 */
struct MshSaverOptions
{
    bool binary = true; ///< Use binary encoding.
    std::vector<AttributeId> attr_ids; ///< Set of attributes to output.
};


/**
 * Saves a mesh to a stream in MSH format. If the mesh cannot be saved, an
 * exception is raised (e.g., invalid output stream, incorrect mesh dimension,
 * or facet size < 3).
 *
 * @param[in,out] output_stream  Output stream to write to.
 * @param[in]     mesh           Input mesh.
 * @param[in]     options        Option settings.
 *
 * @tparam        Scalar         Mesh scalar type.
 * @tparam        Index          Mesh index type.
 */
template <typename Scalar, typename Index>
void save_mesh_msh(
    std::ostream& output_stream,
    const SurfaceMesh<Scalar, Index>& mesh,
    const MshSaverOptions& options = {});

/**
 * @overload
 *
 * @param[in]    filename  Output filename.
 *
 * @see save_mesh_msh(std::ostream&, const SurfaceMesh<S, I>&, const MshOptions&) for more info.
 */
template <typename Scalar, typename Index>
void save_mesh_msh(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const MshSaverOptions& options = {})
{
    fs::ofstream fout(filename);
    save_mesh_msh(fout, mesh, options);
}

} // namespace lagrange::io
