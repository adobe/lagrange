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


struct ImmediateData
{
    std::vector<Eigen::Vector3f> lines;
    std::vector<Color> lines_colors;

    std::vector<Eigen::Vector3f> points;
    std::vector<Color> points_colors;

    std::vector<Eigen::Vector3f> triangles;
    std::vector<Color> triangles_colors;
};

struct ImmediatePassData
{
    Resource<ImmediateData> immediate_data;
    Resource<Shader> shader;
    Resource<CommonPassData> common;
    Resource<MeshBuffer> buffer;
};

using ImmediatePass = RenderPass<ImmediatePassData>;

template <>
std::string default_render_pass_name<ImmediatePass>();

std::unique_ptr<ImmediatePass> create_immediate_pass(
    Resource<CommonPassData> common,
    Resource<ImmediateData> immediate_data);

} // namespace ui
} // namespace lagrange
