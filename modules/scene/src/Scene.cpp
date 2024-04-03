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
#include <lagrange/AttributeValueType.h>
#include <lagrange/scene/Scene.h>

namespace lagrange {
namespace scene {

template <typename Scalar, typename Index>
void Scene<Scalar, Index>::add_child(ElementId parent_id, ElementId child_id)
{
    Node& parent = nodes[parent_id];
    Node& child = nodes[child_id];
    parent.children.push_back(child_id);
    child.parent = parent_id;
}

template struct LA_SCENE_API Scene<float, uint32_t>;
template struct LA_SCENE_API Scene<double, uint32_t>;
template struct LA_SCENE_API Scene<float, uint64_t>;
template struct LA_SCENE_API Scene<double, uint64_t>;

size_t ImageBufferExperimental::get_bits_per_element() const
{
    switch (element_type) {
    case AttributeValueType::e_uint8_t: return 8;
    case AttributeValueType::e_int8_t: return 8;
    case AttributeValueType::e_uint16_t: return 16;
    case AttributeValueType::e_int16_t: return 16;
    case AttributeValueType::e_uint32_t: return 32;
    case AttributeValueType::e_int32_t: return 32;
    case AttributeValueType::e_uint64_t: return 64;
    case AttributeValueType::e_int64_t: return 64;
    case AttributeValueType::e_float: return sizeof(float);
    case AttributeValueType::e_double: return sizeof(double);
    default: return 0;
    }
}

} // namespace scene
} // namespace lagrange
