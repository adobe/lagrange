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
#include <lagrange/xatlas/repack_scene.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/fmt/format.h>
#include <lagrange/xatlas/repack_mesh.h>

#include <cmath>

namespace lagrange::xatlas {

namespace {

void validate_scene_options(size_t num_instances, const SceneOptions& scene_options)
{
    // Note: `repack_scene` ignores `per_instance_importance` (xatlas repacking uses existing
    // UVs only and is not affected by importance). We still validate sizes so callers catch
    // mismatched arrays early.
    if (!scene_options.per_instance_importance.empty() &&
        scene_options.per_instance_importance.size() != num_instances) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: per_instance_importance size ({}) must equal number of "
                "instances ({})",
                scene_options.per_instance_importance.size(),
                num_instances));
    }
    for (float w : scene_options.per_instance_importance) {
        if (!(w > 0.f) || !std::isfinite(w)) {
            throw Error(
                lagrange::format(
                    "lagrange::xatlas: per_instance_importance values must be > 0 and finite (got "
                    "{})",
                    w));
        }
    }
}

} // namespace

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> repack_scene(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RepackOptions& repack_options,
    const SceneOptions& scene_options,
    std::function<void(const std::string&, float)> notification_func,
    const std::atomic_bool* cancel)
{
    using SceneType = scene::SimpleScene<Scalar, Index, 3>;
    using InstanceType = typename SceneType::InstanceType;

    const auto num_meshes = static_cast<size_t>(scene.get_num_meshes());
    const auto num_instances = static_cast<size_t>(scene.compute_num_instances());
    validate_scene_options(num_instances, scene_options);

    if (num_meshes == 0) return scene;

    SceneType result;

    if (repack_options.enable_sharing_uvs_between_instances) {
        for (Index m = 0; m < scene.get_num_meshes(); ++m) {
            auto mesh_copy = scene.get_mesh(m);
            if (mesh_copy.get_num_facets() == 0) {
                const Index new_idx = result.add_mesh(std::move(mesh_copy));
                scene.foreach_instances_for_mesh(m, [&](const InstanceType& inst) {
                    InstanceType ni = inst;
                    ni.mesh_index = new_idx;
                    result.add_instance(ni);
                });
                continue;
            }
            auto repacked = repack_mesh<Scalar, Index>(
                std::move(mesh_copy),
                repack_options,
                notification_func,
                cancel);
            const Index new_idx = result.add_mesh(std::move(repacked));
            scene.foreach_instances_for_mesh(m, [&](const InstanceType& inst) {
                InstanceType ni = inst;
                ni.mesh_index = new_idx;
                result.add_instance(ni);
            });
        }
    } else {
        scene.foreach_instances([&](const InstanceType& inst) {
            const auto m = inst.mesh_index;
            auto mesh_copy = scene.get_mesh(m);
            if (mesh_copy.get_num_facets() == 0) {
                const Index new_idx = result.add_mesh(std::move(mesh_copy));
                InstanceType ni = inst;
                ni.mesh_index = new_idx;
                result.add_instance(ni);
                return;
            }
            auto repacked = repack_mesh<Scalar, Index>(
                std::move(mesh_copy),
                repack_options,
                notification_func,
                cancel);
            const Index new_idx = result.add_mesh(std::move(repacked));
            InstanceType ni = inst;
            ni.mesh_index = new_idx;
            result.add_instance(ni);
        });
    }

    return result;
}

#define LA_X_repack_scene(_, S, I)                           \
    template scene::SimpleScene<S, I, 3> repack_scene<S, I>( \
        const scene::SimpleScene<S, I, 3>&,                  \
        const RepackOptions&,                                \
        const SceneOptions&,                                 \
        std::function<void(const std::string&, float)>,      \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(repack_scene, 0)
#undef LA_X_repack_scene

} // namespace lagrange::xatlas
