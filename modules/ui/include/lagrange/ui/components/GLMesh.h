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
#include <lagrange/ui/Entity.h>
#include <lagrange/ui/types/VertexBuffer.h>
#include <memory>

namespace lagrange {
namespace ui {

struct DefaultShaderAtrribNames
{
    constexpr static const entt::id_type Position = entt::hashed_string{"in_pos"};
    constexpr static const entt::id_type Normal = entt::hashed_string{"in_normal"};
    constexpr static const entt::id_type UV = entt::hashed_string{"in_uv"};
    constexpr static const entt::id_type Color = entt::hashed_string{"in_color"};
    constexpr static const entt::id_type Tangent = entt::hashed_string{"in_tangent"};
    constexpr static const entt::id_type Bitangent = entt::hashed_string{"in_bitangent"};
    constexpr static const entt::id_type Value =
        entt::hashed_string{"in_value"}; // For attribute value
    constexpr static const entt::id_type BoneIDs = entt::hashed_string{"in_bone_ids"};
    constexpr static const entt::id_type BoneWeights = entt::hashed_string{"in_bone_weights"};
};

struct DefaultShaderIndicesNames
{
    constexpr static const entt::id_type VertexIndices = entt::hashed_string{"_vertex_indices"};
    constexpr static const entt::id_type EdgeIndices = entt::hashed_string{"_edge_indices"};
    constexpr static const entt::id_type TriangleIndices = entt::hashed_string{"_triangle_indices"};
};

struct GLMesh
{
    std::shared_ptr<GPUBuffer> get_attribute_buffer(entt::id_type id) const
    {
        auto it = attribute_buffers.find(id);
        if (it == attribute_buffers.end()) return nullptr;
        return it->second;
    }

    std::shared_ptr<GPUBuffer> get_index_buffer(entt::id_type id) const
    {
        auto it = index_buffers.find(id);
        if (it == index_buffers.end()) return nullptr;
        return it->second;
    }

    std::shared_ptr<GPUBuffer> get_submesh_buffer(entt::id_type id) const
    {
        auto it = submesh_indices.find(id);
        if (it == submesh_indices.end()) return nullptr;
        return it->second;
    }

    // Use DefaultShaderAtrribNames or custom ids
    std::unordered_map<entt::id_type, std::shared_ptr<GPUBuffer>> attribute_buffers;

    // Use DefaultShaderIndicesNames or custom ids
    std::unordered_map<entt::id_type, std::shared_ptr<GPUBuffer>> index_buffers;

    std::unordered_map<entt::id_type, std::shared_ptr<GPUBuffer>> submesh_indices;
};

} // namespace ui
} // namespace lagrange