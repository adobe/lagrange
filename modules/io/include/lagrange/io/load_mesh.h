/*
 * Copyright 2017 Adobe. All rights reserved.
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

/*
* This file does not contain function definition because those require other expensive headers
* and we want to minimize the size of this header, as it's used in almost all tests.
*
* If you have any issues with a load_mesh function not being defined for your specific
* mesh type, you can #include <lagrange/io/load_mesh.impl.h> instead.
*/

#include <lagrange/io/load_mesh_obj.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/common.h>

#include <memory>
#include <vector>

namespace lagrange {
namespace io {

template <typename MeshType>
std::unique_ptr<MeshType> load_mesh_basic(const fs::path& filename);

template <typename MeshType>
std::vector<std::unique_ptr<MeshType>> load_obj_meshes(const fs::path& filename);

template <typename MeshType>
std::unique_ptr<MeshType> load_obj_mesh(const fs::path& filename);

template <typename MeshType>
std::unique_ptr<MeshType> load_mesh(const fs::path& filename);

} // namespace io
} // namespace lagrange
