/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include "CistaMesh.h"
#include "CistaValue.h"

#include <cista/containers/optional.h>
#include <cista/containers/string.h>
#include <cista/containers/vector.h>

#include <array>
#include <cstdint>

namespace lagrange::serialization::internal {

namespace data = cista::offset;

constexpr uint64_t k_invalid_element = ~uint64_t(0);

struct CistaTextureInfo
{
    uint64_t index = k_invalid_element;
    int32_t texcoord = 0;
};

struct CistaSceneMeshInstance
{
    uint64_t mesh = k_invalid_element;
    data::vector<uint64_t> materials;
};

struct CistaNode
{
    data::string name;
    std::array<float, 16> transform{}; // Affine3f = 4x4 float matrix
    uint64_t parent = k_invalid_element;
    data::vector<uint64_t> children;
    data::vector<CistaSceneMeshInstance> meshes;
    data::vector<uint64_t> cameras;
    data::vector<uint64_t> lights;
    CistaExtensions extensions;
};

struct CistaImageBuffer
{
    uint64_t width = 0;
    uint64_t height = 0;
    uint64_t num_channels = 0;
    uint8_t element_type = 0; // std::underlying_type_t<AttributeValueType>
    data::vector<uint8_t> data_bytes;
};

struct CistaImage
{
    data::string name;
    CistaImageBuffer image;
    data::string uri;
    CistaExtensions extensions;
};

struct CistaTexture
{
    data::string name;
    uint64_t image = k_invalid_element;
    int32_t mag_filter = 0;
    int32_t min_filter = 0;
    uint8_t wrap_u = 0;
    uint8_t wrap_v = 0;
    std::array<float, 2> scale{};
    std::array<float, 2> offset{};
    float rotation = 0.f;
    CistaExtensions extensions;
};

struct CistaMaterial
{
    data::string name;
    std::array<float, 4> base_color_value{};
    std::array<float, 3> emissive_value{};
    float metallic_value = 0.f;
    float roughness_value = 0.f;
    float alpha_cutoff = 0.f;
    float normal_scale = 0.f;
    float occlusion_strength = 0.f;
    uint8_t alpha_mode = 0;
    bool double_sided = false;
    CistaTextureInfo base_color_texture;
    CistaTextureInfo emissive_texture;
    CistaTextureInfo metallic_roughness_texture;
    CistaTextureInfo normal_texture;
    CistaTextureInfo occlusion_texture;
    CistaExtensions extensions;
};

struct CistaLight
{
    data::string name;
    uint8_t type = 0;
    std::array<float, 3> position{};
    std::array<float, 3> direction{};
    std::array<float, 3> up{};
    float intensity = 0.f;
    float attenuation_constant = 0.f;
    float attenuation_linear = 0.f;
    float attenuation_quadratic = 0.f;
    float attenuation_cubic = 0.f;
    float range = 0.f;
    std::array<float, 3> color_diffuse{};
    std::array<float, 3> color_specular{};
    std::array<float, 3> color_ambient{};
    cista::optional<float> angle_inner_cone;
    cista::optional<float> angle_outer_cone;
    std::array<float, 2> size{};
    CistaExtensions extensions;
};

struct CistaCamera
{
    data::string name;
    uint8_t type = 0;
    std::array<float, 3> position{};
    std::array<float, 3> up{};
    std::array<float, 3> look_at{};
    float near_plane = 0.f;
    cista::optional<float> far_plane;
    float orthographic_width = 0.f;
    float aspect_ratio = 0.f;
    float horizontal_fov = 0.f;
    CistaExtensions extensions;
};

struct CistaSkeleton
{
    data::vector<uint64_t> meshes;
    CistaExtensions extensions;
};

struct CistaAnimation
{
    data::string name;
    CistaExtensions extensions;
};

struct CistaScene
{
    uint32_t version = 1;
    uint8_t scalar_type_size = 0;
    uint8_t index_type_size = 0;

    data::string name;

    data::vector<CistaNode> nodes;
    data::vector<uint64_t> root_nodes;
    data::vector<CistaMesh> meshes;
    data::vector<CistaImage> images;
    data::vector<CistaTexture> textures;
    data::vector<CistaMaterial> materials;
    data::vector<CistaLight> lights;
    data::vector<CistaCamera> cameras;
    data::vector<CistaSkeleton> skeletons;
    data::vector<CistaAnimation> animations;
    CistaExtensions extensions;
};

} // namespace lagrange::serialization::internal
