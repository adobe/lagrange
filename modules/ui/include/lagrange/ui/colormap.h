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
#include <lagrange/ui/Color.h>

namespace lagrange {
namespace ui {

Color colormap_viridis(float t);
Color colormap_magma(float t);
Color colormap_plasma(float t);
Color colormap_inferno(float t);
Color colormap_turbo(float t);
Color colormap_coolwarm(float t);

} // namespace ui
} // namespace lagrange
