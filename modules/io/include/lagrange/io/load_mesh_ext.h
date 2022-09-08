/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <fstream>
#include <limits>
#include <string>

#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/normalize_meshes.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>

#include <lagrange/fs/filesystem.h>
#include <tiny_obj_loader.h>

namespace lagrange {
namespace io {

struct MeshLoaderParams
{
    /// When loading a mesh with mixed facet sizes, this parameter controls whether the polygonal
    /// faces will be triangulated, or left as is and padded with INVALID. Note that if the mesh has
    /// a compile-time face size, and cannot accommodate the maximum face size of the input mesh,
    /// then it will be triangulated as well. If the input mesh has constant face size (e.g. a quad
    /// mesh), and the mesh type can accommodate such facet type, then faces will also not be
    /// triangulated, regardless of this parameter value.
    bool triangulate = false;

    /// Normalize each object to a unit box around the origin.
    bool normalize = false;

    bool load_normals = true;
    bool load_uvs = true;
    bool load_materials = true;

    /// Combines individual objects into a single mesh. Result contains a vector of size 1.
    bool as_one_mesh = false;
};

template <typename MeshType>
struct MeshLoaderResult
{
    bool success = true;
    std::vector<std::unique_ptr<MeshType>> meshes;
    std::vector<tinyobj::material_t> materials;
    std::vector<std::string> mesh_names;
};

inline int fix_index(int index, size_t n, size_t global_offset)
{
    // Shifted absolute index
    if (index > 0) return index - static_cast<int>(global_offset) - 1;
    // Relative index
    if (index < 0) return static_cast<int>(n) + index - static_cast<int>(global_offset);
    // 0 index is invalid in obj files
    return -1;
}

///
/// Loads a .obj mesh from a stream.
///
/// @param[in,out] input_stream     The stream to read data from.
/// @param[in]     params           Optional parameters for the loader.
/// @param[in]     material_reader  Optional pointer to the material reader class.
///
/// @tparam        MeshType         Mesh type.
///
/// @return        Result of the load.
///
template <typename MeshType>
auto load_mesh_ext(
    std::istream& input_stream,
    const MeshLoaderParams& params = {},
    tinyobj::MaterialReader* material_reader = nullptr) -> MeshLoaderResult<MeshType>
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;

    MeshLoaderResult<MeshType> result;

    if (!input_stream.good()) {
        logger().error("Invalid input stream given");
        result.success = false;
        return result;
    }

    /// State of the loading state machine
    /// Passed through tinyobj as void* user data (through function pointers)
    struct Loader
    {
        Loader(const MeshLoaderParams& _params, MeshLoaderResult<MeshType>& _result)
            : params(_params)
            , result(_result)
        {}

        const MeshLoaderParams& params;
        MeshLoaderResult<MeshType>& result;

        std::vector<tinyobj::real_t> vertices;
        std::vector<tinyobj::real_t> normals;
        std::vector<tinyobj::real_t> uvs;
        std::vector<tinyobj::index_t> indices;
        std::vector<int> material_ids;

        size_t vertex_offset = 0;
        size_t normal_offset = 0;
        size_t uv_offset = 0;

        std::string object_name = "";
        int current_material_id = 0;

        std::vector<unsigned char> face_sizes;
        int max_face_size = 0;
        bool is_face_size_constant = false;
        bool is_last_object = false;

        void triangulate()
        {
            std::vector<tinyobj::index_t> new_indices;
            std::vector<unsigned char> new_face_sizes;
            std::vector<int> new_material_ids;
            size_t indices_i = 0;

            // Loop over faces
            for (auto face_index : range(face_sizes.size())) {
                const auto vertex_in_face_num = face_sizes[face_index];
                const auto new_triangle_num = (vertex_in_face_num - 3) + 1;

                // Triangle fan conversion
                for (auto i : range(new_triangle_num)) {
                    // Always use first vertex of origin polygon
                    tinyobj::index_t a;
                    a.vertex_index = indices[indices_i + 0].vertex_index;
                    a.normal_index = indices[indices_i + 0].normal_index;
                    a.texcoord_index = indices[indices_i + 0].texcoord_index;

                    // Next two vertices shift by one in every iteration
                    tinyobj::index_t b;
                    b.vertex_index = indices[indices_i + i + 1].vertex_index;
                    b.normal_index = indices[indices_i + i + 1].normal_index;
                    b.texcoord_index = indices[indices_i + i + 1].texcoord_index;
                    tinyobj::index_t c;
                    c.vertex_index = indices[indices_i + i + 2].vertex_index;
                    c.normal_index = indices[indices_i + i + 2].normal_index;
                    c.texcoord_index = indices[indices_i + i + 2].texcoord_index;

                    new_indices.push_back(a);
                    new_indices.push_back(b);
                    new_indices.push_back(c);
                    new_face_sizes.push_back(3);

                    if (face_index < material_ids.size()) {
                        new_material_ids.push_back(material_ids[face_index]);
                    }
                }

                indices_i += vertex_in_face_num;
            }

            // Replace old polygon indices with triangle fan indices
            indices = new_indices;
            face_sizes = new_face_sizes;
            material_ids = new_material_ids;
            max_face_size = 3;
            is_face_size_constant = true;
        }

    } mesh_loader(params, result);


    // Tinyobj callbacks (triggered when a line is parsed)
    // callbacks are function pointers with void * user_data, hence no capture.
    const auto vertex_cb = [](void* user_data,
                              tinyobj::real_t x,
                              tinyobj::real_t y,
                              tinyobj::real_t z,
                              tinyobj::real_t w) {
        Loader& loader = *(reinterpret_cast<Loader*>(user_data));
        loader.vertices.insert(loader.vertices.end(), {x, y, z});
        (void)(w); // Avoid unused parameter warning.
    };

    const auto normal_cb =
        [](void* user_data, tinyobj::real_t x, tinyobj::real_t y, tinyobj::real_t z) {
            Loader& loader = *(reinterpret_cast<Loader*>(user_data));
            if (loader.params.load_normals) {
                loader.normals.insert(loader.normals.end(), {x, y, z});
            }
        };

    const auto texcoord_cb =
        [](void* user_data, tinyobj::real_t x, tinyobj::real_t y, tinyobj::real_t z) {
            Loader& loader = *(reinterpret_cast<Loader*>(user_data));
            if (loader.params.load_uvs) {
                loader.uvs.insert(loader.uvs.end(), {x, y});
            }
            (void)(z); // Avoid unused parameter warning.
        };

    const auto usemtl_cb = [](void* user_data, const char* name, int material_id) {
        Loader& loader = *(reinterpret_cast<Loader*>(user_data));
        loader.current_material_id = material_id;
        (void)(name); // Avoid unused parameter warning.
    };

    const auto mtllib_cb =
        [](void* user_data, const tinyobj::material_t* materials, int num_materials) {
            Loader& loader = *(reinterpret_cast<Loader*>(user_data));
            loader.result.materials.insert(
                loader.result.materials.end(),
                materials,
                materials + num_materials);
        };

    const auto index_cb = [](void* user_data, tinyobj::index_t* indices, int num_indices) {
        Loader& loader = *(reinterpret_cast<Loader*>(user_data));
        auto num_vertices = loader.vertices.size() / 3;
        auto num_normals = loader.normals.size() / 3;
        auto num_uvs = loader.uvs.size() / 2;
        for (auto i = 0; i < num_indices; i++) {
            auto index = indices[i];
            index.vertex_index =
                fix_index(index.vertex_index, num_vertices, loader.vertex_offset);
            assert(index.vertex_index >= 0 && index.vertex_index < static_cast<int>(num_vertices));
            if (loader.params.load_normals && num_normals) {
                index.normal_index =
                    fix_index(index.normal_index, num_normals, loader.normal_offset);
                assert(index.normal_index >= 0 && index.normal_index < static_cast<int>(num_normals));
            } else {
                index.normal_index = -1;
            }
            if (loader.params.load_uvs && num_uvs) {
                index.texcoord_index =
                    fix_index(index.texcoord_index, num_uvs, loader.uv_offset);
                assert(index.texcoord_index >= 0 && index.texcoord_index < static_cast<int>(num_uvs));
            } else {
                index.texcoord_index = -1;
            }
            loader.indices.push_back(index);
        }

        loader.face_sizes.push_back(static_cast<unsigned char>(num_indices));
        if (loader.params.load_materials) {
            loader.material_ids.push_back(loader.current_material_id);
        }

        if (loader.max_face_size != 0 && num_indices != loader.max_face_size) {
            loader.is_face_size_constant = false;
        }
        loader.max_face_size = std::max(num_indices, loader.max_face_size);
    };

    const auto object_cb = [](void* user_data, const char* name) {
        Loader& loader = *(reinterpret_cast<Loader*>(user_data));

        size_t num_coords;
        size_t max_face_size;
        const auto DIM = 3;
        const auto UV_DIM = 2;

        if (VertexArray::ColsAtCompileTime == Eigen::Dynamic) {
            num_coords = DIM;
        } else {
            num_coords = VertexArray::ColsAtCompileTime;
        }
        if (FacetArray::ColsAtCompileTime == Eigen::Dynamic) {
            max_face_size = size_t(loader.max_face_size);
            if (!loader.is_face_size_constant && loader.params.triangulate) {
                max_face_size = 3;
            }
        } else {
            max_face_size = size_t(FacetArray::ColsAtCompileTime);
        }

        if (loader.params.as_one_mesh && !loader.is_last_object) {
            return;
        }

        // First object begins
        if (loader.vertices.size() == 0) {
            loader.object_name = name;
            return;
        }

        // Triangulate if needed
        if (max_face_size < safe_cast<size_t>(loader.max_face_size)) {
            loader.triangulate();
        }

        auto num_faces = loader.face_sizes.size();
        VertexArray vertices(loader.vertices.size() / DIM, num_coords);
        FacetArray faces(num_faces, max_face_size);

        typename MeshType::UVArray uvs;
        typename MeshType::UVIndices uv_indices;
        typename MeshType::AttributeArray corner_normals;
        if (!loader.uvs.empty()) {
            uvs.resize(loader.uvs.size() / UV_DIM, UV_DIM);
            uv_indices.resize(num_faces, max_face_size);
        }
        if (!loader.normals.empty()) {
            corner_normals.resize(num_faces * max_face_size, num_coords);
        }

        // Copy vertices
        for (auto i : range(loader.vertices.size() / DIM)) {
            for (auto k : range(num_coords)) {
                vertices(i, k) = safe_cast<typename MeshType::Scalar>(loader.vertices[DIM * i + k]);
            }
        }

        // Copy uvs
        for (auto i : range(loader.uvs.size() / UV_DIM)) {
            for (auto k : range(UV_DIM)) {
                uvs(i, k) = safe_cast<typename MeshType::Scalar>(loader.uvs[UV_DIM * i + k]);
            }
        }

        // Copy indices
        size_t indices_i = 0;
        for (auto face_index : range(loader.face_sizes.size())) {
            const auto face_size = loader.face_sizes[face_index];

            for (auto vertex_in_face : range(face_size)) {
                const auto& index = loader.indices[indices_i];
                faces(face_index, vertex_in_face) =
                    safe_cast<typename MeshType::Index>(index.vertex_index);

                if (index.normal_index != -1) {
                    const auto row_index = face_index * max_face_size + vertex_in_face;
                    for (auto k : range(num_coords)) {
                        corner_normals(row_index, k) = safe_cast<typename MeshType::Scalar>(
                            loader.normals[DIM * index.normal_index + k]);
                    }
                }

                if (!loader.uvs.empty()) {
                    uv_indices(face_index, vertex_in_face) =
                        safe_cast<typename MeshType::Index>(index.texcoord_index);
                }

                indices_i++;
            }

            // Padding with invalid<Index> for mixed triangle/quad or arbitrary polygon meshes
            for (auto pad = size_t(face_size); pad < size_t(max_face_size); pad++) {
                faces(face_index, pad) = invalid<typename MeshType::Index>();
                if (!loader.uvs.empty()) {
                    uv_indices(face_index, pad) = invalid<typename MeshType::UVIndices::Scalar>();
                }
                if (!loader.normals.empty()) {
                    corner_normals.row(face_index * max_face_size + pad).setZero();
                }
            }
        }

        auto mesh = create_mesh(vertices, faces);

        if (!loader.uvs.empty()) {
            mesh->initialize_uv(uvs, uv_indices);

            // TODO: The loader should not do this mapping index -> corner
            map_indexed_attribute_to_corner_attribute(*mesh, "uv");
        }

        if (!loader.normals.empty()) {
            mesh->add_corner_attribute("normal");
            mesh->import_corner_attribute("normal", corner_normals);
        }

        if (loader.result.materials.size() > 0 && loader.material_ids.size() == num_faces) {
            // Because AttributeArray is not integral type, this converts to float type
            Eigen::Map<Eigen::VectorXi> map(loader.material_ids.data(), loader.material_ids.size());
            mesh->add_facet_attribute("material_id");
            mesh->set_facet_attribute("material_id", map.cast<typename MeshType::Scalar>());
        }

        loader.result.meshes.emplace_back(std::move(mesh));

        loader.result.mesh_names.push_back(loader.object_name);

        loader.vertex_offset += loader.vertices.size() / DIM;
        loader.normal_offset += loader.normals.size() / DIM;
        loader.uv_offset += loader.uvs.size() / UV_DIM;

        loader.vertices.clear();
        loader.uvs.clear();
        loader.normals.clear();
        loader.indices.clear();
        loader.face_sizes.clear();
        loader.material_ids.clear();
        loader.max_face_size = 0;
        loader.is_face_size_constant = true;
        loader.object_name = name;
    };

    tinyobj::callback_t callback;
    callback.vertex_cb = vertex_cb;
    callback.index_cb = index_cb;
    callback.object_cb = object_cb;

    if (params.load_normals) callback.normal_cb = normal_cb;

    if (params.load_uvs) callback.texcoord_cb = texcoord_cb;

    if (params.load_materials) {
        callback.mtllib_cb = mtllib_cb;
        callback.usemtl_cb = usemtl_cb;
    }

    std::string warning_message;
    std::string error_message;
    result.success = tinyobj::LoadObjWithCallback(
        input_stream,
        callback,
        &mesh_loader,
        params.load_materials ? material_reader : nullptr,
        &warning_message,
        &error_message);

    mesh_loader.is_last_object = true;
    object_cb(&mesh_loader, "");

    if (!error_message.empty()) {
        logger().error("Load mesh warning:\n{}", error_message);
    }
    if (!warning_message.empty()) {
        logger().warn("Load mesh error:\n{}", warning_message);
    }

    if (params.normalize) {
        normalize_meshes(mesh_loader.result.meshes);
    }

    return result;
}

/// Load .obj from disk
template <typename MeshType>
MeshLoaderResult<MeshType> load_mesh_ext(
    const lagrange::fs::path& filename,
    const MeshLoaderParams& params = {},
    tinyobj::MaterialReader* material_reader = nullptr)
{
    lagrange::fs::ifstream stream(filename);
    if (!stream.good()) {
        MeshLoaderResult<MeshType> result;
        logger().error("Cannot open file: \"{}\"", filename.string());
        result.success = false;
        return result;
    }
    tinyobj::MaterialFileReader default_reader(filename.parent_path().string());
    if (params.load_materials && !material_reader) {
        material_reader = &default_reader;
    }
    return load_mesh_ext<MeshType>(stream, params, material_reader);
}

} // namespace io
} // namespace lagrange
