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

#include <lagrange/ui/api.h>
#include <lagrange/ui/types/Color.h>
#include <lagrange/ui/types/Texture.h>

namespace lagrange {
namespace ui {

LA_UI_API Color colormap_viridis(float t);
LA_UI_API Color colormap_magma(float t);
LA_UI_API Color colormap_plasma(float t);
LA_UI_API Color colormap_inferno(float t);
LA_UI_API Color colormap_turbo(float t);
LA_UI_API Color colormap_coolwarm(float t);


LA_UI_API std::shared_ptr<Texture> generate_colormap(
    const std::function<Color(float)>& generator,
    int resolution = 256);

} // namespace ui
} // namespace lagrange
