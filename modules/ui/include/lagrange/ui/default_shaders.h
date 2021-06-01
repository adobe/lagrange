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
    constexpr static const entt::id_type Simple = "Simple"_hs;
    constexpr static const entt::id_type PBR = "PBR"_hs;
    constexpr static const entt::id_type PBRSkeletal = "PBRSkeletal"_hs;
    constexpr static const entt::id_type TextureView = "TextureView"_hs;
    constexpr static const entt::id_type TrianglesToLines = "TrianglesToLines"_hs;
    constexpr static const entt::id_type Outline = "Outline"_hs;
    constexpr static const entt::id_type ShadowDepth = "ShadowDepth"_hs;
    constexpr static const entt::id_type ShadowCubemap = "ShadowCubemap"_hs;
    constexpr static const entt::id_type SurfaceVertexAttribute = "SurfaceVertexAttribute"_hs;
    constexpr static const entt::id_type LineVertexAttribute = "LineVertexAttribute"_hs;
    constexpr static const entt::id_type SurfaceEdgeAttribute =
        "SurfaceEdgeAttribute"_hs; // Uses special edge attribute interpolation
    constexpr static const entt::id_type ObjectID = "ObjectID"_hs;
    constexpr static const entt::id_type MeshElementID = "MeshElementID"_hs;
};

struct ColormapShaderMode
{
    constexpr static const entt::id_type Passthrough = 0;
    constexpr static const entt::id_type Texture = 1;
};

struct PBRMaterial
{
    constexpr static const entt::id_type BaseColor = "material_base_color"_hs;
    constexpr static const entt::id_type Roughness = "material_roughness"_hs;
    constexpr static const entt::id_type Normal = "material_normal"_hs;
    constexpr static const entt::id_type Metallic = "material_metallic"_hs;
    constexpr static const entt::id_type Opacity = "material_opacity"_hs;

    
};

void register_default_shaders(Registry& r);

} // namespace ui
} // namespace lagrange