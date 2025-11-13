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
    #include <lagrange/io/api.h>
    #include <lagrange/io/internal/load_assimp.h>
    #include <lagrange/io/load_mesh_assimp.h>
    #include <lagrange/io/load_scene_assimp.h>
    #include <lagrange/io/load_simple_scene_assimp.h>
// ====

    #include <lagrange/Attribute.h>
    #include <lagrange/AttributeValueType.h>
    #include <lagrange/Logger.h>
    #include <lagrange/SurfaceMeshTypes.h>
    #include <lagrange/attribute_names.h>
    #include <lagrange/combine_meshes.h>
    #include <lagrange/internal/skinning.h>
    #include <lagrange/io/internal/scene_utils.h>
    #include <lagrange/scene/Scene.h>
    #include <lagrange/scene/SceneTypes.h>
    #include <lagrange/scene/SimpleSceneTypes.h>
    #include <lagrange/scene/scene_utils.h>
    #include <lagrange/triangulate_polygonal_facets.h>
    #include <lagrange/utils/assert.h>
    #include "stitch_mesh.h"

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
            } else {
                name = std::string(AttributeName::texcoord) + "_" + std::to_string(uv_set);
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

    if (options.stitch_vertices) {
        stitch_mesh(lmesh);
    }
    if (options.triangulate) {
        triangulate_polygonal_facets(lmesh);
    }

    return lmesh;
}
    #define LA_X_convert_mesh_assimp_to_lagrange(_, S, I)           \
        template SurfaceMesh<S, I> convert_mesh_assimp_to_lagrange( \
            const aiMesh& mesh,                                     \
            const LoadOptions& options);
LA_SURFACE_MESH_X(convert_mesh_assimp_to_lagrange, 0);
    #undef LA_X_convert_mesh_assimp_to_lagrange

template <typename AffineTransform>
AffineTransform convert_transform_assimp_to_lagrange(const aiMatrix4x4 t)
{
    AffineTransform transform;
    // clang-format off
    transform.matrix() <<
        t.a1, t.a2, t.a3, t.a4,
        t.b1, t.b2, t.b3, t.b4,
        t.c1, t.c2, t.c3, t.c4,
        t.d1, t.d2, t.d3, t.d4;
    // clang-format on
    return transform;
}

Eigen::Vector3f to_vec3(const aiVector3D v)
{
    return Eigen::Vector3f(v.x, v.y, v.z);
}
Eigen::Vector3f to_color3(const aiColor3D v)
{
    return Eigen::Vector3f(v.r, v.g, v.b);
}
Eigen::Vector4f to_color4(const aiColor4D& v)
{
    return Eigen::Vector4f(v.r, v.g, v.b, v.a);
}

scene::Light convert_light_assimp_to_lagrange(const aiLight* light)
{
    scene::Light llight;
    llight.name = light->mName.C_Str();
    switch (light->mType) {
    case aiLightSource_DIRECTIONAL: llight.type = scene::Light::Type::Directional; break;
    case aiLightSource_POINT: llight.type = scene::Light::Type::Point; break;
    case aiLightSource_SPOT: llight.type = scene::Light::Type::Spot; break;
    case aiLightSource_AMBIENT: llight.type = scene::Light::Type::Ambient; break;
    case aiLightSource_AREA: llight.type = scene::Light::Type::Area; break;
    case aiLightSource_UNDEFINED: // passthrough to default
    default: llight.type = scene::Light::Type::Undefined; break;
    }
    llight.position = to_vec3(light->mPosition);
    llight.direction = to_vec3(light->mDirection);
    llight.up = to_vec3(light->mUp);
    llight.intensity = 1.0f;
    llight.attenuation_constant = light->mAttenuationConstant;
    llight.attenuation_linear = light->mAttenuationLinear;
    llight.attenuation_quadratic = light->mAttenuationQuadratic;
    llight.color_diffuse = to_color3(light->mColorDiffuse);
    llight.color_specular = to_color3(light->mColorSpecular);
    llight.color_ambient = to_color3(light->mColorAmbient);
    llight.angle_inner_cone = light->mAngleInnerCone;
    llight.angle_outer_cone = light->mAngleOuterCone;
    llight.size = Eigen::Vector2f(light->mSize.x, light->mSize.y);
    return llight;
}

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
        AffineTransform node_transform(AffineTransform::Identity());
        if constexpr (SceneType::Dim == 3) {
            node_transform =
                convert_transform_assimp_to_lagrange<AffineTransform>(node->mTransformation);
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
    #define LA_X_load_simple_scene_assimp(_, S, I, D)                  \
        template scene::SimpleScene<S, I, D> load_simple_scene_assimp( \
            const aiScene& scene,                                      \
            const LoadOptions& options);
LA_SIMPLE_SCENE_X(load_simple_scene_assimp, 0);
    #undef LA_X_load_simple_scene_assimp


template <typename SceneType>
SceneType load_scene_assimp(const aiScene& scene, const LoadOptions& options)
{
    SceneType lscene;
    lscene.name = scene.mName.C_Str();
    for (unsigned int i = 0; i < scene.mNumMeshes; ++i) {
        lscene.add(
            convert_mesh_assimp_to_lagrange<typename SceneType::MeshType>(
                *scene.mMeshes[i],
                options));
    }

    // note that assimp's textures are really images.
    // We must load these before the materials below, or indices will be off.
    for (unsigned int i = 0; i < scene.mNumTextures; ++i) {
        const aiTexture* texture = scene.mTextures[i];
        if (texture->mHeight == 0) {
            logger().error("Unsupported compressed texture");
            // TODO add support
        }

        scene::ImageExperimental limage;
        scene::ImageBufferExperimental buffer;

        buffer.width = texture->mWidth;
        buffer.height = texture->mHeight;
        buffer.num_channels = 4;
        buffer.element_type = AttributeValueType::e_uint8_t;
        const size_t num_pixels = buffer.width * buffer.height;
        buffer.data.resize(num_pixels);
        for (size_t pi = 0; pi < num_pixels; pi++) {
            const auto& pixel = texture->pcData[pi];
            // Note that pixel is in ARGB8888 format. In our image buffer, we use RGBA8888 format.
            buffer.data[pi * 4] = pixel.r;
            buffer.data[pi * 4 + 1] = pixel.g;
            buffer.data[pi * 4 + 2] = pixel.b;
            buffer.data[pi * 4 + 3] = pixel.a;
        }

        limage.image = std::move(buffer);
        limage.name = texture->mFilename.C_Str();

        lscene.add(std::move(limage));
    }

    // load an image from embedded or from disk.
    // If successful, saves the image in the scene and returns its index.
    // Otherwise, returns -1.
    auto try_image_load = [&](const aiMaterial*, const char* s) -> int {
        auto [texture, index] = scene.GetEmbeddedTextureAndIndex(s);
        if (index >= 0) {
            return index;
        } else {
            scene::ImageExperimental limage;
            limage.name = s;
            limage.uri = s;
            if (options.load_images) {
                if (io::internal::try_load_image(s, options, limage)) {
                    return static_cast<int>(lscene.add(std::move(limage)));
                } else {
                    return -1;
                }
            } else {
                return static_cast<int>(lscene.add(std::move(limage)));
            }
        }
    };
    auto convert_map_mode = [](aiTextureMapMode mode) -> scene::Texture::WrapMode {
        switch (mode) {
        case aiTextureMapMode_Wrap: return scene::Texture::WrapMode::Wrap;
        case aiTextureMapMode_Clamp: return scene::Texture::WrapMode::Clamp;
        case aiTextureMapMode_Decal: return scene::Texture::WrapMode::Decal;
        case aiTextureMapMode_Mirror: return scene::Texture::WrapMode::Mirror;
        default: return scene::Texture::WrapMode::Wrap;
        }
    };
    auto try_load_texture =
        [&](const aiMaterial* material, aiTextureType type, scene::TextureInfo& tex_info) -> bool {
        if (tex_info.index != scene::invalid_element)
            return false; // there was a texture here already

        aiString path;
        aiTextureMapping texture_mapping;
        unsigned int uv_index;
        ai_real blend;
        aiTextureOp op;
        aiTextureMapMode map_mode[2];
        if (material
                ->GetTexture(type, 0, &path, &texture_mapping, &uv_index, &blend, &op, map_mode) !=
            aiReturn_SUCCESS)
            return false;

        int image_idx = try_image_load(material, path.C_Str());
        if (image_idx == -1) return false;

        tex_info.texcoord = uv_index;

        scene::Texture ltex;
        ltex.name = path.C_Str();
        ltex.image = image_idx;
        ltex.wrap_u = convert_map_mode(map_mode[0]);
        ltex.wrap_v = convert_map_mode(map_mode[1]);
        tex_info.index = lscene.add(std::move(ltex));
        return true;
    };

    for (unsigned int i = 0; i < scene.mNumMaterials; ++i) {
        scene::MaterialExperimental lmat;

        const aiMaterial* material = scene.mMaterials[i];
        aiString name = material->GetName();
        lmat.name = name.C_Str();

        aiColor3D color3;
        aiColor4D color4;
        aiString string;
        ai_real real;
        bool b;

        // we do our best here, but there may be incompatible materials or missed information
        try_load_texture(material, aiTextureType_BASE_COLOR, lmat.base_color_texture);
        try_load_texture(material, aiTextureType_DIFFUSE, lmat.base_color_texture);
        if (material->Get(AI_MATKEY_BASE_COLOR, color4) == aiReturn_SUCCESS)
            lmat.base_color_value = to_color4(color4);

        try_load_texture(material, aiTextureType_NORMALS, lmat.normal_texture);
        try_load_texture(material, aiTextureType_NORMAL_CAMERA, lmat.normal_texture);

        try_load_texture(material, aiTextureType_EMISSIVE, lmat.emissive_texture);
        try_load_texture(material, aiTextureType_EMISSION_COLOR, lmat.emissive_texture);
        if (material->Get(AI_MATKEY_EMISSIVE_INTENSITY, color3) == aiReturn_SUCCESS)
            lmat.emissive_value = to_color3(color3);

        try_load_texture(material, aiTextureType_METALNESS, lmat.metallic_roughness_texture);
        try_load_texture(
            material,
            aiTextureType_DIFFUSE_ROUGHNESS,
            lmat.metallic_roughness_texture);
        if (material->Get(AI_MATKEY_METALLIC_FACTOR, real) == aiReturn_SUCCESS)
            lmat.metallic_value = real;
        if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, real) == aiReturn_SUCCESS)
            lmat.metallic_value = real;

        try_load_texture(material, aiTextureType_AMBIENT_OCCLUSION, lmat.occlusion_texture);

        if (material->Get(AI_MATKEY_TWOSIDED, b) == aiReturn_SUCCESS) lmat.double_sided = b;

        // we could also iterate over all textures, unnecessary for now:
        // const std::vector<aiTextureType> texture_types = {
        //    // traditional
        //    aiTextureType_DIFFUSE,
        //    aiTextureType_SPECULAR,
        //    aiTextureType_AMBIENT,
        //    aiTextureType_EMISSIVE,
        //    aiTextureType_HEIGHT,
        //    aiTextureType_NORMALS,
        //    aiTextureType_SHININESS,
        //    aiTextureType_OPACITY,
        //    aiTextureType_DISPLACEMENT,
        //    aiTextureType_LIGHTMAP,
        //    aiTextureType_REFLECTION,
        //    // pbr
        //    aiTextureType_BASE_COLOR,
        //    aiTextureType_NORMAL_CAMERA,
        //    aiTextureType_EMISSION_COLOR,
        //    aiTextureType_METALNESS,
        //    aiTextureType_DIFFUSE_ROUGHNESS,
        //    aiTextureType_AMBIENT_OCCLUSION,
        //    // pbr modifiers
        //    aiTextureType_SHEEN,
        //    aiTextureType_CLEARCOAT,
        //    aiTextureType_TRANSMISSION,
        //};
        // for (aiTextureType type : texture_types) {
        //    unsigned int count = material->GetTextureCount(type);
        //    const char* type_string = aiTextureTypeToString(type);
        //    for (unsigned int j = 0; j < count; ++j) {
        //        aiString path;
        //        aiTextureMapping texture_mapping;
        //        unsigned int uv_index;
        //        ai_real blend;
        //        aiTextureOp op;
        //        aiTextureMapMode map_mode[2];
        //        aiReturn ret = material->GetTexture(type, j, &path, &texture_mapping, &uv_index,
        //        &blend, &op, map_mode); if (ret == aiReturn_SUCCESS) {
        //            logger().warn("{}: {}", type_string, path.C_Str());
        //        }
        //    }
        //}

        lscene.materials.emplace_back(std::move(lmat));
    }
    for (unsigned int i = 0; i < scene.mNumAnimations; ++i) {
        // TODO
    }
    for (unsigned int i = 0; i < scene.mNumLights; ++i) {
        lscene.lights.push_back(convert_light_assimp_to_lagrange(scene.mLights[i]));
    }
    for (unsigned int i = 0; i < scene.mNumCameras; ++i) {
        const aiCamera* camera = scene.mCameras[i];
        scene::Camera lcam;
        lcam.name = camera->mName.C_Str();
        lcam.near_plane = camera->mClipPlaneNear;
        lcam.far_plane = camera->mClipPlaneFar;
        lcam.position = to_vec3(camera->mPosition);
        lcam.up = to_vec3(camera->mUp);
        lcam.look_at = to_vec3(camera->mLookAt);
        // assimp docs claim the fov is from the center to the side, so it would be half of our fov.
        // but from testing it appears to be the same as ours.
        // It may depend on the filetype, and it could be a bug.
        lcam.horizontal_fov = camera->mHorizontalFOV;
        lcam.aspect_ratio = camera->mAspect;
        if (camera->mOrthographicWidth != 0) {
            lcam.type = scene::Camera::Type::Orthographic;
            lcam.orthographic_width = camera->mOrthographicWidth;
        } else {
            lcam.type = scene::Camera::Type::Perspective;
        }
        lscene.cameras.push_back(lcam);
    }
    for (unsigned int i = 0; i < scene.mNumSkeletons; ++i) {
        // TODO
    }
    std::function<size_t(aiNode*, size_t parent_idx)> create_node; // creates and returns its index
    create_node = [&](aiNode* node, size_t parent_idx) -> size_t {
        size_t lnode_idx = lscene.nodes.size();
        lscene.nodes.push_back(scene::Node());
        scene::Node& lnode = lscene.nodes.back();

        lnode.name = node->mName.C_Str();
        lnode.transform =
            convert_transform_assimp_to_lagrange<Eigen::Affine3f>(node->mTransformation);
        lnode.parent = parent_idx;

        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            unsigned int mesh_idx = node->mMeshes[i];
            unsigned int material_idx = scene.mMeshes[mesh_idx]->mMaterialIndex;
            lnode.meshes.push_back({mesh_idx, {material_idx}});
        }

        // in assimp, cameras and lights are not linked from the nodes directly.
        // instead, assimp relies on having the camera and light with the same name as the node.
        for (size_t i = 0; i < lscene.lights.size(); ++i) {
            if (lscene.lights[i].name == lnode.name) {
                lnode.lights.push_back(i);
            }
        }
        for (size_t i = 0; i < lscene.cameras.size(); ++i) {
            if (lscene.cameras[i].name == lnode.name) {
                lnode.cameras.push_back(i);
            }
        }
        lnode.children.resize(node->mNumChildren);
        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            lnode.children[i] = create_node(node->mChildren[i], lnode_idx);
        }
        return lnode_idx;
    };
    std::function<size_t(aiNode*)> count_nodes;
    count_nodes = [&count_nodes](aiNode* node) -> size_t {
        size_t count = 1;
        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            count += count_nodes(node->mChildren[i]);
        }
        return count;
    };
    lscene.nodes.reserve(count_nodes(scene.mRootNode));
    size_t root_index = create_node(scene.mRootNode, lagrange::invalid<size_t>());
    lscene.root_nodes.push_back(root_index);

    return lscene;
}
    #define LA_X_load_scene_assimp(_, S, I)            \
        template scene::Scene<S, I> load_scene_assimp( \
            const aiScene& scene,                      \
            const LoadOptions& options);
LA_SCENE_X(load_scene_assimp, 0);
    #undef LA_X_load_scene_assimp

} // namespace internal


// =====================================
// load_mesh_assimp.h
// =====================================
template <
    typename MeshType,
    std::enable_if_t<!lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* /*= nullptr*/>
MeshType load_mesh_assimp(const fs::path& filename, const LoadOptions& options)
{
    std::unique_ptr<aiScene> scene = internal::load_assimp(filename);
    return internal::load_mesh_assimp<MeshType>(*scene, options);
}

template <
    typename MeshType,
    std::enable_if_t<!lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* /*= nullptr*/>
MeshType load_mesh_assimp(std::istream& input_stream, const LoadOptions& options)
{
    std::unique_ptr<aiScene> scene = internal::load_assimp(input_stream);
    return internal::load_mesh_assimp<MeshType>(*scene, options);
}

    #define LA_X_load_mesh_assimp(_, S, I)                     \
        template LA_IO_API SurfaceMesh<S, I> load_mesh_assimp( \
            const fs::path& filename,                          \
            const LoadOptions& options);                       \
        template LA_IO_API SurfaceMesh<S, I> load_mesh_assimp( \
            std::istream&,                                     \
            const LoadOptions& options);
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

    #define LA_X_load_simple_scene_assimp(_, S, I, D)                            \
        template LA_IO_API scene::SimpleScene<S, I, D> load_simple_scene_assimp( \
            const fs::path&,                                                     \
            const LoadOptions&);                                                 \
        template LA_IO_API scene::SimpleScene<S, I, D> load_simple_scene_assimp( \
            std::istream&,                                                       \
            const LoadOptions&);
LA_SIMPLE_SCENE_X(load_simple_scene_assimp, 0);
    #undef LA_X_load_simple_scene_assimp

// =====================================
// load_scene_assimp.h
// =====================================
template <typename SceneType>
SceneType load_scene_assimp(const fs::path& filename, const LoadOptions& options)
{
    std::unique_ptr<aiScene> scene = internal::load_assimp(filename);
    LoadOptions opt2 = options;
    if (opt2.search_path.empty()) opt2.search_path = filename.parent_path();
    return internal::load_scene_assimp<SceneType>(*scene, opt2);
}
template <typename SceneType>
SceneType load_scene_assimp(std::istream& input_stream, const LoadOptions& options)
{
    std::unique_ptr<aiScene> scene = internal::load_assimp(input_stream);
    return internal::load_scene_assimp<SceneType>(*scene, options);
}
    #define LA_X_load_scene_assimp(_, S, I)                      \
        template LA_IO_API scene::Scene<S, I> load_scene_assimp( \
            const fs::path&,                                     \
            const LoadOptions&);                                 \
        template LA_IO_API scene::Scene<S, I> load_scene_assimp(std::istream&, const LoadOptions&);
LA_SCENE_X(load_scene_assimp, 0);
    #undef LA_X_load_scene_assimp

} // namespace lagrange::io

#endif
