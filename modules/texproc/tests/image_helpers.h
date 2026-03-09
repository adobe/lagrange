/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <lagrange/image/Array3D.h>
#include <lagrange/scene/Scene.h>

namespace test {

// This transposes the first two dimensions of the image.
// x_0 maps to width, x_1 maps to height and x_2 maps to channel.
// Also assumes (0, 0) is the top-left corner of the image.

auto scene_image_to_image_array(const lagrange::scene::ImageBufferExperimental& image)
    -> lagrange::image::experimental::Array3D<float>;

} // namespace test
