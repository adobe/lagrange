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

#include <lagrange/ui/Viz.h>
#include <lagrange/ui/render_passes/BRDFLUTPass.h>
#include <lagrange/ui/render_passes/FXAAPass.h>
#include <lagrange/ui/render_passes/ImmediatePass.h>
#include <lagrange/ui/render_passes/NormalsPass.h>
#include <lagrange/ui/render_passes/ObjectOutlinePass.h>
#include <lagrange/ui/render_passes/ShadowMapPass.h>
#include <lagrange/ui/render_passes/SkyboxPass.h>
#include <lagrange/ui/render_passes/TextureViewPass.h>
#include <lagrange/ui/render_passes/GroundPass.h>

namespace lagrange {
namespace ui {

/*
    Default render passes enum. Use when creating the window to
    automatically initialize these passes.
*/
using DefaultPasses_t = size_t;
enum DefaultPasses : DefaultPasses_t {
    PASS_SHADOW_MAP = 1 << 0,
    PASS_SKYBOX = 1 << 1,
    PASS_PBR = 1 << 2,
    PASS_VERTEX = 1 << 3,
    PASS_EDGE = 1 << 4,
    PASS_OBJECT_ID = 1 << 5,
    PASS_NORMAL = 1 << 6,
    PASS_IMMEDIATE = 1 << 7,
    PASS_FXAA = 1 << 8,
    PASS_TEXTURE_VIEW = 1 << 9,
    PASS_SELECTED_FACET = 1 << 10,
    PASS_SELECTED_EDGE = 1 << 11,
    PASS_SELECTED_VERTEX = 1 << 12,
    PASS_UNSELECTED_EDGE = 1 << 13,
    PASS_UNSELECTED_VERTEX = 1 << 14,
    PASS_BOUNDING_BOX = 1 << 15,
    PASS_OBJECT_OUTLINE = 1 << 16,
    PASS_GROUND = 1 << 17,
    PASS_BRDFLUT = 1 << 18,
    PASS_ALL = ~DefaultPasses_t(0)
};


/// @brief Enum to type trait
/// By default, passes are using the Viz::PassData
/// Specializations are defined below
template <DefaultPasses t>
struct DefaultPassType
{
    using type = RenderPass<Viz::PassData>;
};

template <>
struct DefaultPassType<PASS_SHADOW_MAP>
{
    using type = ShadowMapPass;
};
template <>
struct DefaultPassType<PASS_SKYBOX>
{
    using type = SkyboxPass;
};
template <>
struct DefaultPassType<PASS_NORMAL>
{
    using type = NormalsPass;
};
template <>
struct DefaultPassType<PASS_FXAA>
{
    using type = FXAAPass;
};
template <>
struct DefaultPassType<PASS_OBJECT_OUTLINE>
{
    using type = ObjectOutlinePass;
};
template <>
struct DefaultPassType<PASS_TEXTURE_VIEW>
{
    using type = TextureViewPass;
};
template <>
struct DefaultPassType<PASS_GROUND>
{
    using type = GroundPass;
};

template <>
struct DefaultPassType<PASS_BRDFLUT>
{
    using type = BRDFLUTPass;
};

// TODO traits class for type


} // namespace ui
} // namespace lagrange
