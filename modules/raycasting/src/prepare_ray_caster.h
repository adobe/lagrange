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

#include <lagrange/raycasting/RayCaster.h>

namespace lagrange::raycasting {

template <typename Scalar, typename Index>
std::unique_ptr<RayCaster> prepare_ray_caster(
    const SurfaceMesh<Scalar, Index>& source,
    const RayCaster* ray_caster)
{
    std::unique_ptr<RayCaster> engine;
    if (!ray_caster) {
        engine = std::make_unique<RayCaster>(SceneFlags::Robust, BuildQuality::High);
        SurfaceMesh<Scalar, Index> source_copy = source;
        engine->add_mesh(std::move(source_copy));
        engine->commit_updates();
        ray_caster = engine.get();
    }
    return engine;
}

} // namespace lagrange::raycasting
