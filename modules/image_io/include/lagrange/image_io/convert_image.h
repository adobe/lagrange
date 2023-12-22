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

#include <lagrange/fs/filesystem.h>
#include <lagrange/image/ImageType.h>

namespace lagrange {
namespace image_io {

template <typename SRC, typename DST, typename CONVERTOR>
bool convert_image_file(
    const fs::path& path_src,
    const fs::path& path_dst,
    const CONVERTOR& convertor)
{
    image::ImageView<SRC> src;
    image::ImageView<DST> dst;
    return load_image(path_src, src) && dst.convert_from(src, 1, convertor) &&
           save_image(path_dst, dst);
}

template <typename SRC, typename DST>
bool convert_image_file(const fs::path& path_src, const fs::path& path_dst)
{
    return convert_image_file(path_src, path_dst, image::convert_image_pixel<SRC, DST>());
}

} // namespace image_io
} // namespace lagrange
