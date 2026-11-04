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

#include <lagrange/io/api.h>

#include <lagrange/SurfaceMesh.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/io/types.h>

#include <iosfwd>

namespace lagrange::io {

///
/// @addtogroup module-io
/// @{
///

/**
 * Saves a mesh to a stream in MSH format.
 *
 * Only facets of size 2, 3, and 4 are supported, written as line, triangle, and quad elements.
 *
 * @param[in,out] output_stream  Output stream.
 * @param[in]     mesh           Input mesh.
 * @param[in]     options        Option settings.
 *
 * @tparam        Scalar         Mesh scalar type.
 * @tparam        Index          Mesh index type.
 */
template <typename Scalar, typename Index>
LA_IO_API void save_mesh_msh(
    std::ostream& output_stream,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options = {});

/**
 * @overload
 *
 * Saves a mesh to a file in MSH format.
 *
 * Only facets of size 2, 3, and 4 are supported, written as line, triangle, and quad elements.
 *
 * @param[in]  filename  Output filename.
 * @param[in]  mesh      Mesh to write.
 * @param[in]  options   Save options.
 *
 * @see        @ref save_mesh_msh
 *
 * @tparam     Scalar    Mesh scalar type.
 * @tparam     Index     Mesh index type.
 */
template <typename Scalar, typename Index>
LA_IO_API void save_mesh_msh(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options = {});

/// @}

} // namespace lagrange::io
