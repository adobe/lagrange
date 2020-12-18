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

struct TextureViewPassData {
    Resource<Shader> shader;
    Resource<CommonPassData> common;
    RenderPipeline* pipeline;
};

using TextureViewPass = RenderPass<TextureViewPassData>;

template <>
std::string default_render_pass_name<TextureViewPass>();

std::unique_ptr<TextureViewPass> create_textureview_pass(
    Resource<CommonPassData> common,
    RenderPipeline & pipeline
);

} // namespace ui
} // namespace lagrange
