/*
 * Copyright 2021 Adobe. All rights reserved.
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

#include <lagrange/ui/api.h>
#include <lagrange/ui/Entity.h>


namespace lagrange {
namespace ui {

struct ViewportComponent;
struct SelectionContext;

/// Translates shader output color to numerial ID
LA_UI_API int color_to_id(unsigned char r, unsigned char g, unsigned char b);

/// Is numerical ID from shader a background?
LA_UI_API bool is_id_background(int id);

/// Read pixels of viewport in rectangle at x,y of size w,h
LA_UI_API const std::vector<unsigned char>&
read_pixels(Registry& r, ViewportComponent& v, int x, int y, int w, int h);


/// Copies properties of active viewport to offscreen viewport, sets up a material override using override_shader
LA_UI_API ViewportComponent& setup_offscreen_viewport(
    Registry& r,
    Entity ofscreen_viewport_entity,
    Entity active_viewport_entity,
    StringID override_shader);

/// Sets rasterizer scissor based on SelectionContext
LA_UI_API std::tuple<int, int, int, int>
setup_scissor(Registry& r, ViewportComponent& offscreen_viewport, const SelectionContext& sel_ctx);


} // namespace ui
} // namespace lagrange
