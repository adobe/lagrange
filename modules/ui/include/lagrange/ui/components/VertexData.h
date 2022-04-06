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

#include <lagrange/ui/types/VertexBuffer.h>

#include <array>
#include <memory>

namespace lagrange {
namespace ui {

struct VertexData
{
    VertexData()
    {
        std::fill_n(attribute_dimensions.begin(), max_attributes, 0);
        std::fill_n(attribute_buffers.begin(), max_attributes, nullptr);
        index_buffer = nullptr;
    }


    constexpr static int max_attributes =
        16; // Minimum is 16 per OpenGL spec (GL_MAX_VERTEX_ATTRIBS)
    std::shared_ptr<VAO> vao;
    std::array<int, max_attributes> attribute_dimensions;
    std::array<std::shared_ptr<GPUBuffer>, max_attributes> attribute_buffers;
    std::shared_ptr<GPUBuffer> index_buffer;
};


} // namespace ui
} // namespace lagrange