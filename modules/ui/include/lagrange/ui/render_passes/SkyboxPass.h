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


namespace lagrange {
namespace ui {


struct SkyboxPassData
{
    Resource<Texture> color_output;
    Resource<Texture> depth_output;
    Resource<Shader> shader;
    Resource<CommonPassData> common;
};

using SkyboxPass = RenderPass<SkyboxPassData>;

template <>
std::string default_render_pass_name<SkyboxPass>();

std::unique_ptr<SkyboxPass> create_skybox_pass(Resource<CommonPassData> common);

} // namespace ui
} // namespace lagrange
