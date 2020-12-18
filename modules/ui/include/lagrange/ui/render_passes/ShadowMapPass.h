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
#include <lagrange/ui/render_passes/common.h>

#include <array>
#include <memory>

#define MAX_DEFAULT_LIGHTS_POINT 1
#define MAX_DEFAULT_LIGHTS_SPOT_DIR 4
#define MAX_DEFAULT_LIGHTS_TOTAL (MAX_DEFAULT_LIGHTS_POINT + MAX_DEFAULT_LIGHTS_SPOT_DIR)


namespace lagrange {
namespace ui {

struct ShadowMapPassData
{
    Resource<FrameBuffer> target_fbo;

    std::array<Resource<Texture>, MAX_DEFAULT_LIGHTS_SPOT_DIR> maps_2D;
    std::array<Resource<Texture>, MAX_DEFAULT_LIGHTS_POINT> maps_cube;

    Resource<std::vector<EmitterRenderData>> emitters;

    Resource<Shader> shader_2D;
    Resource<Shader> shader_cube;

    // If non-empty, renders shadows only of the selected objects
    Selection<BaseObject*> model_selection;

    Resource<CommonPassData> common;
};

using ShadowMapPass = RenderPass<ShadowMapPassData>;

template <>
std::string default_render_pass_name<ShadowMapPass>();

std::unique_ptr<ShadowMapPass> create_shadow_map_pass(Resource<CommonPassData> common);

} // namespace ui
} // namespace lagrange
