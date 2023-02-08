/*
 * Copyright 2017 Adobe. All rights reserved.
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

#include <string_view>

namespace lagrange {

// Valid attribute semantic properties are listed below.
// Attribute semantic property names can be in the form [semantic]_[set_index], e.g. texcoord_0,
// texcoord_1, etc.

struct AttributeName
{
    /**
     * Normal. Paired with AttributeUsage::Normal.
     */
    static constexpr std::string_view normal = "normal";

    /**
     * Color. Paired with AttributeUsage::Color.
     */
    static constexpr std::string_view color = "color";

    /**
     * Texture coordinates. Paired with AttributeUsage::UV. Other common names are UV, UVs.
     */
    static constexpr std::string_view texcoord = "texcoord";

    /**
     * Tangent. Paired with AttributeUsage::Vector.
     */
    static constexpr std::string_view tangent = "tangent";

    /**
     * Bitangent. Paired with AttributeUsage::Vector.
     */
    static constexpr std::string_view bitangent = "bitangent";

    /**
     * Material ID. Paired with AttributeUsage::Scalar.
     */
    static constexpr std::string_view material_id = "material_id";

    /**
     * Object ID (in obj files). Paired with AttributeUsage::Scalar.
     */
    static constexpr std::string_view object_id = "object_id";

    /**
     * Skinning weights, with all joints specified for each vertex. Paired with
     * AttributeUsage::Vector.
     */
    static constexpr std::string_view weight = "weight";

    /**
     * Indexed skinning weights, with a fixed number (typically 4) specified for each vertex. Paired
     * with AttributeUsage::Vector.
     */
    static constexpr std::string_view indexed_weight = "indexed_weight";

    /**
     * Indexed skinning index, with a fixed number (typically 4) specified for each vertex. Paired
     * with AttributeUsage::Vector. Other common names include bone, bone_index, handle.
     */
    static constexpr std::string_view indexed_joint = "indexed_joint";
};

} // namespace lagrange
