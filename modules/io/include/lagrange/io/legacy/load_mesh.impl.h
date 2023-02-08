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
#pragma once

#include <lagrange/io/load_mesh.h>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/combine_mesh_list.h>
#include <lagrange/create_mesh.h>
#include <lagrange/fs/file_utils.h>
#include <lagrange/io/legacy/load_mesh_ext.h>
#include <lagrange/io/legacy/load_mesh_ply.h>
#include <lagrange/legacy/inline.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/read_triangle_mesh.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <tiny_obj_loader.h>

namespace lagrange::io {
LAGRANGE_LEGACY_INLINE
namespace legacy {

template <typename MeshType>
std::unique_ptr<MeshType> load_mesh_basic(const fs::path& filename)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    typename MeshType::VertexArray vertices;
    typename MeshType::FacetArray facets;

    igl::read_triangle_mesh(filename.string(), vertices, facets);

    return create_mesh(vertices, facets);
}
extern template std::unique_ptr<lagrange::TriangleMesh3D> load_mesh_basic(const fs::path&);
extern template std::unique_ptr<lagrange::TriangleMesh3Df> load_mesh_basic(const fs::path&);

template <typename MeshType>
std::vector<std::unique_ptr<MeshType>> load_obj_meshes(const fs::path& filename)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    if (!std::is_same<tinyobj::real_t, double>::value) {
        logger().warn("Tinyobjloader compiled to use float, loss of precision may occur.");
    }

    return load_mesh_ext<MeshType>(filename).meshes;
}
extern template std::vector<std::unique_ptr<lagrange::TriangleMesh3D>> load_obj_meshes(const fs::path&);
extern template std::vector<std::unique_ptr<lagrange::TriangleMesh3Df>> load_obj_meshes(const fs::path&);

template <typename MeshType>
std::unique_ptr<MeshType> load_obj_mesh(const fs::path& filename)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    auto res = load_obj_meshes<MeshType>(filename);
    if (res.empty()) {
        return nullptr;
    } else if (res.size() == 1) {
        return std::move(res[0]);
    } else {
        logger().debug("Combining {} meshes into one.", res.size());
        return combine_mesh_list(res, true);
    }
}
extern template std::unique_ptr<lagrange::TriangleMesh3D> load_obj_mesh(const fs::path&);
extern template std::unique_ptr<lagrange::TriangleMesh3Df> load_obj_mesh(const fs::path&);

template <
    typename MeshType,
    std::enable_if_t<lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* /*= nullptr*/>
std::unique_ptr<MeshType> load_mesh(const fs::path& filename)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    if (filename.extension() == ".obj") {
        return load_obj_mesh<MeshType>(filename);
    } else if (filename.extension() == ".ply") {
        return load_mesh_ply<MeshType>(filename);
    } else {
        return load_mesh_basic<MeshType>(filename);
    }
}
extern template std::unique_ptr<lagrange::TriangleMesh3D> load_mesh(const fs::path&);
extern template std::unique_ptr<lagrange::TriangleMesh3Df> load_mesh(const fs::path&);


} // namespace legacy
} // namespace lagrange::io
