#pragma once

#include <lagrange/io/load_mesh.h>

#include <lagrange/io/load_mesh_ext.h>
#include <lagrange/io/load_mesh_ply.h>
#include <lagrange/fs/file_utils.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/combine_mesh_list.h>
#include <lagrange/create_mesh.h>

#include <lagrange/utils/warnoff.h>
#include <igl/read_triangle_mesh.h>
#include <tiny_obj_loader.h>
#include <lagrange/utils/warnon.h>

namespace lagrange {
namespace io {

template <typename MeshType>
std::unique_ptr<MeshType> load_mesh_basic(const fs::path& filename)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    typename MeshType::VertexArray vertices;
    typename MeshType::FacetArray facets;

    igl::read_triangle_mesh(filename.string(), vertices, facets);

    return create_mesh(vertices, facets);
}
extern template std::unique_ptr<TriangleMesh3D> load_mesh_basic(const fs::path&);
extern template std::unique_ptr<TriangleMesh3Df> load_mesh_basic(const fs::path&);

template <typename MeshType>
std::vector<std::unique_ptr<MeshType>> load_obj_meshes(const fs::path& filename)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    if (!std::is_same<tinyobj::real_t, double>::value) {
        logger().warn("Tinyobjloader compiled to use float, loss of precision may occur.");
    }

    return load_mesh_ext<MeshType>(filename).meshes;
}
extern template std::vector<std::unique_ptr<TriangleMesh3D>> load_obj_meshes(const fs::path&);
extern template std::vector<std::unique_ptr<TriangleMesh3Df>> load_obj_meshes(const fs::path&);

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
        return combine_mesh_list(res);
    }
}
extern template std::unique_ptr<TriangleMesh3D> load_obj_mesh(const fs::path&);
extern template std::unique_ptr<TriangleMesh3Df> load_obj_mesh(const fs::path&);

template <typename MeshType>
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
extern template std::unique_ptr<TriangleMesh3D> load_mesh(const fs::path&);
extern template std::unique_ptr<TriangleMesh3Df> load_mesh(const fs::path&);


} // namespace io
} // namespace lagrange
