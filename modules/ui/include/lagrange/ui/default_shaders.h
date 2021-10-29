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

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/types/ShaderLoader.h>

namespace lagrange {
namespace ui {

struct DefaultShaders
{
    constexpr static const StringID Simple = "Simple"_hs;
    constexpr static const StringID PBR = "PBR"_hs;
    constexpr static const StringID PBRSkeletal = "PBRSkeletal"_hs;
    constexpr static const StringID TextureView = "TextureView"_hs;
    constexpr static const StringID TrianglesToLines = "TrianglesToLines"_hs;
    constexpr static const StringID Outline = "Outline"_hs;
    constexpr static const StringID ShadowDepth = "ShadowDepth"_hs;
    constexpr static const StringID ShadowCubemap = "ShadowCubemap"_hs;
    constexpr static const StringID SurfaceVertexAttribute = "SurfaceVertexAttribute"_hs;
    constexpr static const StringID LineVertexAttribute = "LineVertexAttribute"_hs;
    constexpr static const StringID SurfaceEdgeAttribute =
        "SurfaceEdgeAttribute"_hs; // Uses special edge attribute interpolation
    constexpr static const StringID ObjectID = "ObjectID"_hs;
    constexpr static const StringID MeshElementID = "MeshElementID"_hs;
    constexpr static const StringID Skybox = "Skybox"_hs;
};

struct ColormapShaderMode
{
    constexpr static const StringID Passthrough = 0;
    constexpr static const StringID Texture = 1;
};

struct PBRMaterial
{
    constexpr static const StringID BaseColor = "material_base_color"_hs;
    constexpr static const StringID Roughness = "material_roughness"_hs;
    constexpr static const StringID Normal = "material_normal"_hs;
    constexpr static const StringID Metallic = "material_metallic"_hs;
    constexpr static const StringID Opacity = "material_opacity"_hs;

    constexpr static const entt::id_type IndexOfRefraction = "material_index_of_refraction"_hs;

    constexpr static const entt::id_type Glow = "material_glow"_hs;
    constexpr static const entt::id_type Translucence = "material_translucence"_hs;

    constexpr static const entt::id_type InteriorColor = "material_interior_color"_hs;
    constexpr static const entt::id_type GlowIntensity = "material_glow_intensity"_hs;
};

void register_default_shaders(Registry& r);

} // namespace ui
} // namespace lagrange