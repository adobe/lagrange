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

#include <lagrange/scene/filter_instances.h>

#include <lagrange/scene/SimpleSceneTypes.h>

#include <utility>

namespace lagrange::scene {

template <typename Scalar, typename Index, size_t Dimension>
SimpleScene<Scalar, Index, Dimension> filter_instances(
    const SimpleScene<Scalar, Index, Dimension>& scene,
    function_ref<bool(detail::type_identity_t<Index>, detail::type_identity_t<Index>)> keep)
{
    SimpleScene<Scalar, Index, Dimension> result;
    for (Index mi = 0; mi < scene.get_num_meshes(); ++mi) {
        bool mesh_added = false;
        for (Index ii = 0; ii < scene.get_num_instances(mi); ++ii) {
            if (keep(mi, ii)) {
                if (!mesh_added) {
                    result.add_mesh(scene.get_mesh(mi));
                    mesh_added = true;
                }
                auto instance = scene.get_instance(mi, ii);
                instance.mesh_index = result.get_num_meshes() - 1;
                result.add_instance(std::move(instance));
            }
        }
    }
    return result;
}

#define LA_X_filter_instances(_, Scalar, Index, Dimension)                        \
    template LA_SCENE_API SimpleScene<Scalar, Index, Dimension> filter_instances( \
        const SimpleScene<Scalar, Index, Dimension>&,                             \
        function_ref<bool(detail::type_identity_t<Index>, detail::type_identity_t<Index>)>);
LA_SIMPLE_SCENE_X(filter_instances, 0)

} // namespace lagrange::scene
