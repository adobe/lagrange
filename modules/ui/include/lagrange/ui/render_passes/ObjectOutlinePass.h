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
#include <lagrange/ui/render_passes/common.h>

#include <array>
#include <memory>


namespace lagrange {
namespace ui {

struct ObjectOutlinePassData
{
    Resource<Texture> input_color;
    Resource<Texture> input_depth;
    Resource<Shader> shader;
    Resource<CommonPassData> common;
};

using ObjectOutlinePass = RenderPass<ObjectOutlinePassData>;

template <>
std::string default_render_pass_name<ObjectOutlinePass>();

std::unique_ptr<ObjectOutlinePass> create_object_outline_pass(
    const RenderPass<Viz::PassData>* object_id_viz,
    Resource<CommonPassData> common);

} // namespace ui
} // namespace lagrange
