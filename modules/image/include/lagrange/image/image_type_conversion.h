/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <lagrange/image/ImageStorage.h>
#include <lagrange/image/ImageType.h>
#include <lagrange/image/ImageView.h>
#include <lagrange/image/RawInputImage.h>

#include <memory>

namespace lagrange {
namespace image {

std::shared_ptr<ImageStorage> image_storage_from_raw_input_image(const image::RawInputImage& image);

image::RawInputImage raw_input_image_from_image_view(
    image::ImageViewBase& in,
    bool copy_buffer = false);

} // namespace image
} // namespace lagrange
