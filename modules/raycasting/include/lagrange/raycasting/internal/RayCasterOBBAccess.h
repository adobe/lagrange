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

#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/span.h>

#include <variant>

namespace lagrange::raycasting::internal {

/// Friend accessor to RayCaster's OBB overlap queries. Re-exposes the otherwise-private
/// `OrientedBox` type through a using-alias so callers can build OBB packets directly.
struct RayCasterOBBAccess
{
    using OrientedBox = RayCaster::OrientedBox;

    static void overlap_obb(
        const RayCaster& rc,
        const OrientedBox& obb,
        lagrange::function_ref<bool(uint32_t, uint32_t, uint32_t)> callback)
    {
        rc.overlap_obb_internal(obb, callback);
    }

    /// Packet-of-16 OBB overlap query. The callback receives the originating lane index so hits
    /// can be routed back to the OBB that produced them.
    static void overlap_obb16(
        const RayCaster& rc,
        span<const OrientedBox> obbs,
        std::variant<RayCaster::Mask16, size_t> active,
        lagrange::function_ref<
            bool(uint32_t lane, uint32_t mesh_index, uint32_t instance_index, uint32_t facet_index)>
            callback)
    {
        rc.overlap_obb16_internal(obbs, active, callback);
    }
};

} // namespace lagrange::raycasting::internal
