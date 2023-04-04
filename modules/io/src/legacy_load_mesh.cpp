/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/io/legacy/load_mesh.impl.h>

namespace lagrange::io {
LAGRANGE_LEGACY_INLINE
namespace legacy {

template std::unique_ptr<TriangleMesh3D> load_mesh_basic(const fs::path&);
template std::unique_ptr<TriangleMesh3Df> load_mesh_basic(const fs::path&);

template std::vector<std::unique_ptr<TriangleMesh3D>> load_obj_meshes(const fs::path&);
template std::vector<std::unique_ptr<TriangleMesh3Df>> load_obj_meshes(const fs::path&);

template std::unique_ptr<TriangleMesh3D> load_obj_mesh(const fs::path&);
template std::unique_ptr<TriangleMesh3Df> load_obj_mesh(const fs::path&);

template std::unique_ptr<TriangleMesh3D> load_mesh(const fs::path&);
template std::unique_ptr<TriangleMesh3Df> load_mesh(const fs::path&);
template std::unique_ptr<TriangleMesh2D> load_mesh(const fs::path&);
template std::unique_ptr<Mesh<Eigen::MatrixXf, Eigen::MatrixXi>> load_mesh(const fs::path&);

} // namespace legacy
} // namespace lagrange::io
