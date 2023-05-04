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

#ifdef LAGRANGE_WITH_ASSIMP

    // this .cpp provides implementations for functions defined in those headers:
    #include <lagrange/io/internal/load_assimp.h>
    #include <lagrange/io/load_mesh_assimp.h>
    #include <lagrange/io/load_simple_scene_assimp.h>

    #include <lagrange/Attribute.h>
    #include <lagrange/Logger.h>
    #include <lagrange/SurfaceMeshTypes.h>
    #include <lagrange/attribute_names.h>
    #include <lagrange/combine_meshes.h>
    #include <lagrange/internal/skinning.h>
    #include <lagrange/scene/SimpleSceneTypes.h>
    #include <lagrange/utils/assert.h>

    #include <assimp/material.h>
    #include <assimp/Importer.hpp>

    #include <Eigen/Core>

    #include <istream>

// =====================================
// internal/load_assimp.h
// =====================================
namespace lagrange::io {
namespace internal {

std::unique_ptr<aiScene> load_assimp(const fs::path& filename, unsigned int flags)
{
    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    const aiScene* scene = importer.ReadFile(filename.string(), flags);

    if (!scene) {
        throw std::runtime_error(importer.GetErrorString());
    }

    return std::unique_ptr<aiScene>(importer.GetOrphanedScene());
}

std::unique_ptr<aiScene> load_assimp(std::istream& input_stream, unsigned int flags)
{
    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    // TODO: Switch to input_stream.view() when C++20 is available.
    std::istreambuf_iterator<char> data_itr(input_stream), end_of_stream;
    std::string data(data_itr, end_of_stream);
    const aiScene* scene = importer.ReadFileFromMemory(data.data(), data.size(), flags);

    if (!scene) {
        throw std::runtime_error(importer.GetErrorString());
    }

    return std::unique_ptr<aiScene>(importer.GetOrphanedScene());
}


template <typename MeshType>
MeshType convert_mesh_assimp_to_lagrange(const aiMesh& aimesh, const LoadOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    constexpr unsigned int dim = 3;
    constexpr unsigned int color_dim = 4;
    constexpr unsigned int uv_dim = 2;

    MeshType lmesh;

    lmesh.add_vertices(aimesh.mNumVertices, [&](Index v, span<Scalar> p) -> void {
        for (unsigned int i = 0; i < dim; ++i) {
            p[i] = Scalar(aimesh.mVertices[v][i]);
        }
    });

    lmesh.add_hybrid(
        aimesh.mNumFaces,
        [&aimesh](Index f) -> Index { return aimesh.mFaces[f].mNumIndices; },
        [&aimesh](Index f, span<Index> t) -> void {
            const aiFace& face = aimesh.mFaces[f];
            la_debug_assert(t.size() == face.mNumIndices);
            for (unsigned int i = 0; i < face.mNumIndices; ++i) {
                t[i] = Index(face.mIndices[i]);
            }
        });

    const unsigned int num_uv_channels = aimesh.GetNumUVChannels();
    if (options.load_uvs && num_uv_channels > 0) {
        for (unsigned int uv_set = 0; uv_set < num_uv_channels; ++uv_set) {
            std::string name;
            if (aimesh.HasTextureCoordsName(uv_set)) {
                name = std::string(aimesh.GetTextureCoordsName(uv_set)->C_Str());
            } else if (num_uv_channels > 1) {
                name = std::string(AttributeName::texcoord) + "_" + std::to_string(uv_set);
            } else {
                name = AttributeName::texcoord;
            }

            auto id = lmesh.template create_attribute<Scalar>(
                name,
                AttributeElement::Vertex,
                AttributeUsage::UV,
                uv_dim);
            auto uv_attr = lmesh.template ref_attribute<Scalar>(id).ref_all();
            for (unsigned int i = 0; i < aimesh.mNumVertices; ++i) {
                const aiVector3D& vec = aimesh.mTextureCoords[uv_set][i];
                for (unsigned int j = 0; j < uv_dim; ++j) {
                    uv_attr[i * uv_dim + j] = Scalar(vec[j]);
                }
            }
        }
    }

    if (aimesh.HasBones() && options.load_weights) {
        using BoneIndexType = uint32_t;
        using BoneWeightScalar = float;
        Eigen::Matrix<BoneWeightScalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> weights(
            aimesh.mNumVertices,
            aimesh.mNumBones);
        weights.setZero();

        for (unsigned int i = 0; i < aimesh.mNumBones; ++i) {
            const aiBone* bone = aimesh.mBones[i];
            for (unsigned int j = 0; j < bone->mNumWeights; ++j) {
                const aiVertexWeight& weight = bone->mWeights[j];
                weights(weight.mVertexId, i) = BoneWeightScalar(weight.mWeight);
            }
        }

        // weights matrix is |V| x |bones|
        lagrange::internal::
            weights_to_indexed_mesh_attribute<Scalar, Index, BoneWeightScalar, BoneIndexType>(
                lmesh,
                weights,
                4);
    }

    if (aimesh.HasTangentsAndBitangents() && options.load_tangents) {
        auto id_tangent = lmesh.template create_attribute<Scalar>(
            AttributeName::tangent,
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            dim);
        auto id_bitangent = lmesh.template create_attribute<Scalar>(
            AttributeName::bitangent,
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            dim);
        auto tangent_attr = lmesh.template ref_attribute<Scalar>(id_tangent).ref_all();
        auto bitangent_attr = lmesh.template ref_attribute<Scalar>(id_bitangent).ref_all();

        for (unsigned int i = 0; i < aimesh.mNumVertices; ++i) {
            const aiVector3D& t = aimesh.mTangents[i];
            for (unsigned int j = 0; j < dim; j++) {
                tangent_attr[i * dim + j] = t[j];
            }
            const aiVector3D& bt = aimesh.mBitangents[i];
            for (unsigned int j = 0; j < dim; j++) {
                bitangent_attr[i * dim + j] = bt[j];
            }
        }
    }

    if (aimesh.HasNormals() && options.load_normals) {
        auto id = lmesh.template create_attribute<Scalar>(
            AttributeName::normal,
            AttributeElement::Vertex,
            AttributeUsage::Normal,
            dim);
        auto normal_attr = lmesh.template ref_attribute<Scalar>(id).ref_all();

        for (unsigned int i = 0; i < aimesh.mNumVertices; ++i) {
            const aiVector3D& t = aimesh.mNormals[i];
            for (unsigned int j = 0; j < dim; j++) {
                normal_attr[i * dim + j] = t[j];
            }
        }
    }

    const unsigned int num_color_channels = aimesh.GetNumColorChannels();
    if (options.load_vertex_colors && num_color_channels > 0) {
        for (unsigned int color_set = 0; color_set < num_color_channels; ++color_set) {
            std::string name;
            if (num_color_channels > 1) {
                name = std::string(AttributeName::color) + "_" + std::to_string(color_set);
            } else {
                name = AttributeName::color;
            }
            auto id = lmesh.template create_attribute<Scalar>(
                name,
                AttributeElement::Vertex,
                AttributeUsage::Color,
                color_dim);
            auto color_attr = lmesh.template ref_attribute<Scalar>(id).ref_all();
            for (unsigned int i = 0; i < aimesh.mNumVertices; ++i) {
                const aiColor4D& color = aimesh.mColors[color_set][i];
                for (unsigned int j = 0; j < color_dim; j++) {
                    color_attr[i * color_dim + j] = color[j];
                }
            }
        }
    }

    return lmesh;
}
    #define LA_X_convert_mesh_assimp_to_lagrange(_, S, I)           \
        template SurfaceMesh<S, I> convert_mesh_assimp_to_lagrange( \
            const aiMesh& mesh,                                     \
            const LoadOptions& options);
LA_SURFACE_MESH_X(convert_mesh_assimp_to_lagrange, 0);
    #undef LA_X_convert_mesh_assimp_to_lagrange


template <typename MeshType>
MeshType load_mesh_assimp(const aiScene& scene, const LoadOptions& options)
{
    la_runtime_assert(scene.mNumMeshes > 0);
    if (scene.mNumMeshes == 1) {
        return convert_mesh_assimp_to_lagrange<MeshType>(*scene.mMeshes[0], options);
    } else {
        std::vector<MeshType> meshes(scene.mNumMeshes);
        for (unsigned int i = 0; i < scene.mNumMeshes; ++i) {
            meshes[i] = convert_mesh_assimp_to_lagrange<MeshType>(*scene.mMeshes[i], options);
        }
        bool preserve_attributes = true;
        return combine_meshes<typename MeshType::Scalar, typename MeshType::Index>(
            meshes,
            preserve_attributes);
    }
}
    #define LA_X_load_mesh_assimp(_, S, I)           \
        template SurfaceMesh<S, I> load_mesh_assimp( \
            const aiScene& mesh,                     \
            const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh_assimp, 0);
    #undef LA_X_load_mesh_assimp


template <typename SceneType>
SceneType load_simple_scene_assimp(const aiScene& scene, const LoadOptions& options)
{
    using MeshType = typename SceneType::MeshType;
    using AffineTransform = typename SceneType::AffineTransform;

    // TODO: handle 2d SimpleScene

    SceneType lscene;

    for (unsigned int i = 0; i < scene.mNumMeshes; ++i) {
        // By adding in the same order, we can assume that assimp's indexing is the same
        // as in the lagrange scene. We use this later.
        lscene.add_mesh(convert_mesh_assimp_to_lagrange<MeshType>(*scene.mMeshes[i], options));
    }
    std::function<void(aiNode*, AffineTransform)> visit_node;
    visit_node = [&](aiNode* node, const AffineTransform& parent_transform) -> void {
        AffineTransform node_transform;
        if constexpr (SceneType::Dim == 3) {
            auto& t = node->mTransformation;
            // clang-format off
            node_transform.matrix() <<
                t.a1, t.a2, t.a3, t.a4,
                t.b1, t.b2, t.b3, t.b4,
                t.c1, t.c2, t.c3, t.c4,
                t.d1, t.d2, t.d3, t.d4;
            //clang-format on
        } else {
            // TODO: convert 3d transforms into 2d
            logger().warn("Ignoring 3d node transform while loading 2d scene");
        }
        AffineTransform global_transform = parent_transform * node_transform;

        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            unsigned int mesh_idx = node->mMeshes[i];
            lscene.add_instance({mesh_idx, global_transform});
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            visit_node(node->mChildren[i], global_transform);
        }
    };
    visit_node(scene.mRootNode, AffineTransform::Identity());
    return lscene;
}
#define LA_X_load_simple_scene_assimp(_, S, I, D) \
    template scene::SimpleScene<S, I, D> load_simple_scene_assimp(const aiScene& scene, const LoadOptions& options);
LA_SIMPLE_SCENE_X(load_simple_scene_assimp, 0);
#undef LA_X_load_simple_scene_assimp

} // namespace lagrange::io::internal


// =====================================
// load_mesh_assimp.h
// =====================================
template <typename MeshType,
    std::enable_if_t<!lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* /*= nullptr*/>
MeshType load_mesh_assimp(const fs::path& filename, const LoadOptions& options) {
    std::unique_ptr<aiScene> scene = internal::load_assimp(filename);
    return internal::load_mesh_assimp<MeshType>(*scene, options);
}

template <typename MeshType,
    std::enable_if_t<!lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* /*= nullptr*/>
MeshType load_mesh_assimp(std::istream& input_stream, const LoadOptions& options) {
    std::unique_ptr<aiScene> scene = internal::load_assimp(input_stream);
    return internal::load_mesh_assimp<MeshType>(*scene, options);
}

#define LA_X_load_mesh_assimp(_, S, I) \
    template SurfaceMesh<S, I> load_mesh_assimp(const fs::path& filename, const LoadOptions& options);\
    template SurfaceMesh<S, I> load_mesh_assimp(std::istream&, const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh_assimp, 0);
#undef LA_X_load_mesh_assimp


// =====================================
// load_simple_scene_assimp.h
// =====================================
template <typename SceneType>
SceneType load_simple_scene_assimp(const fs::path& filename, const LoadOptions& options)
{
    std::unique_ptr<aiScene> scene = internal::load_assimp(filename);
    return internal::load_simple_scene_assimp<SceneType>(*scene, options);
}

template <typename SceneType>
SceneType load_simple_scene_assimp(std::istream& input_stream, const LoadOptions& options)
{
    std::unique_ptr<aiScene> scene = internal::load_assimp(input_stream);
    return internal::load_simple_scene_assimp<SceneType>(*scene, options);
}

#define LA_X_load_simple_scene_assimp(_, S, I, D) \
    template scene::SimpleScene<S, I, D> load_simple_scene_assimp(const fs::path&, const LoadOptions&);\
    template scene::SimpleScene<S, I, D> load_simple_scene_assimp(std::istream&, const LoadOptions&);
LA_SIMPLE_SCENE_X(load_simple_scene_assimp, 0);
#undef LA_X_load_simple_scene_assimp

} // namespace lagrange::io

#endif
