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

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_uv_tile_list.h>
#include <lagrange/foreach_attribute.h>

#include <unordered_set>
#include <array>

namespace lagrange {

template <typename Scalar, typename Index>
std::vector<std::pair<int32_t, int32_t>> compute_uv_tile_list(
    const SurfaceMesh<Scalar, Index>& mesh)
{
    std::unordered_set<uint64_t> uvtiles{};

    seq_foreach_attribute_read(mesh, [&uvtiles](auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        if (attr.get_usage() == AttributeUsage::UV) {
            size_t channelCount = attr.get_num_channels();

            if (channelCount < 2) {
                logger().warn("Incompatible number of channels UV attribute, skipping");
                return;
            }

            if constexpr (std::is_same_v<AttributeType, IndexedAttribute<ValueType, Index>>) {
                auto&& values = attr.values().get_all();
                auto&& indices = attr.indices().get_all();

                for (auto&& index : indices) {
                    std::array<int32_t, 2> const tile = {
                        static_cast<int32_t>(std::floor(values[index * channelCount])),
                        static_cast<int32_t>(std::floor(values[index * channelCount + 1]))};

                    uvtiles.insert(
                        static_cast<uint64_t>(tile[0]) << 32 | static_cast<uint64_t>(tile[1]));
                }
            }

            if constexpr (std::is_same_v<AttributeType, Attribute<ValueType>>) {
                auto&& values = attr.get_all();

                for (size_t index = 0; index < attr.get_num_elements(); index += channelCount) {
                    std::array<int32_t, 2> const tile = {
                        static_cast<int32_t>(std::floor(values[index])),
                        static_cast<int32_t>(std::floor(values[index + 1]))};

                    uvtiles.insert(
                        static_cast<uint64_t>(tile[0]) << 32 | static_cast<uint64_t>(tile[1]));
                }
            }
        }
    });

    std::vector<std::pair<int32_t, int32_t>> result{};
    result.reserve(uvtiles.size());
    for (uint64_t tile : uvtiles)
        result.push_back(
            std::make_pair(static_cast<int32_t>(tile >> 32), static_cast<int32_t>(tile)));

    return result;
}

#define LA_X_compute_uv_tile_list(_, Scalar, Index)                                        \
    template LA_CORE_API std::vector<std::pair<int32_t, int32_t>> compute_uv_tile_list<Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(compute_uv_tile_list, 0)

} // namespace lagrange
