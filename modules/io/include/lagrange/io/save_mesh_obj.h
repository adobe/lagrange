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

#include <ostream>
#include <vector>

namespace lagrange {
namespace io {

///
/// Saves a .obj mesh to a stream. If the mesh cannot be saved, an exception is raised (e.g.,
/// invalid output stream, incorrect mesh dimension, or facet size < 3).
///
/// @param[in,out] output_stream  Output stream to write to.
/// @param[in]     mesh           Mesh to write as an obj.
///
/// @tparam        S              Mesh scalar type.
/// @tparam        I              Mesh index type.
///
template <typename S, typename I>
void save_mesh_obj(std::ostream& output_stream, const SurfaceMesh<S, I>& mesh);

} // namespace io
} // namespace lagrange
