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
#include <lagrange/xatlas/unwrap_scene.h>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/scene/internal/bake_scaling.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/fmt/format.h>
#include <lagrange/xatlas/unwrap_mesh.h>

#include <algorithm>
#include <cmath>

namespace lagrange::xatlas {

namespace {

void validate_scene_options(size_t num_instances, const SceneOptions& scene_options)
{
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

/// Apply a 4x4 affine transform to all mesh vertex positions in place.
/// Requires the mesh to be 3D; throws lagrange::Error otherwise.
template <typename Scalar, typename Index>
void apply_transform(SurfaceMesh<Scalar, Index>& mesh, const Eigen::Matrix<Scalar, 4, 4>& xf)
{
    if (mesh.get_dimension() != Index(3)) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: scene mesh must have dimension 3 (got {})",
                static_cast<uint64_t>(mesh.get_dimension())));
    }
    const Index n = mesh.get_num_vertices();
    auto positions = mesh.ref_vertex_to_position().ref_all();
    for (Index v = 0; v < n; ++v) {
        Eigen::Matrix<Scalar, 4, 1> p;
        p << positions[3 * v + 0], positions[3 * v + 1], positions[3 * v + 2], Scalar(1);
        Eigen::Matrix<Scalar, 4, 1> q = xf * p;
        positions[3 * v + 0] = q[0];
        positions[3 * v + 1] = q[1];
        positions[3 * v + 2] = q[2];
    }
}

template <typename Scalar, typename Index>
Scalar uniform_scale_from_transform(
    const typename scene::SimpleScene<Scalar, Index, 3>::AffineTransform& xf)
{
    // Conservative scale: max of the three column norms of the linear part. This
    // upper-bounds per-axis scaling so packing density is sized for the largest axis (e.g.
    // when scaleY > scaleX) instead of underestimating from one column.
    const auto m = xf.matrix();
    const Scalar n0 = m.template block<3, 1>(0, 0).norm();
    const Scalar n1 = m.template block<3, 1>(0, 1).norm();
    const Scalar n2 = m.template block<3, 1>(0, 2).norm();
    return std::max({n0, n1, n2});
}

} // namespace

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> unwrap_scene(
    const scene::SimpleScene<Scalar, Index, 3>& scene_,
    const UnwrapOptions& unwrap_options,
    const SceneOptions& scene_options,
    std::function<void(const std::string&, float)> notification_func,
    const std::atomic_bool* cancel)
{
    using SceneType = scene::SimpleScene<Scalar, Index, 3>;
    using InstanceType = typename SceneType::InstanceType;

    const auto num_meshes = static_cast<size_t>(scene_.get_num_meshes());
    const auto num_instances = static_cast<size_t>(scene_.compute_num_instances());
    validate_scene_options(num_instances, scene_options);

    if (num_meshes == 0) return scene_;

    // Bake per-instance importance into the instance transforms via the shared
    // bake_scaling/unbake_scaling utilities. Importance is passed through directly (linear in
    // scale, matching lagrange::anorigami) so callers can swap implementations without changing
    // packing density semantics. The original transforms (and any pre-existing user_data) are
    // restored at the end via unbake_scaling.
    const bool is_scaling_baked = !scene_options.per_instance_importance.empty();
    const SceneType& scene =
        is_scaling_baked
            ? scene::internal::bake_scaling(scene_, scene_options.per_instance_importance)
            : scene_;

    SceneType result;

    if (unwrap_options.enable_sharing_uvs_between_instances) {
        // Shared UVs path: unwrap each mesh once. Per-mesh scale is the max over all (importance-
        // baked) instance scales, so packing density is sized for the largest instance.
        std::vector<Scalar> per_mesh_scale(num_meshes, Scalar(0));
        scene.foreach_instances([&](const InstanceType& inst) {
            const Scalar s = uniform_scale_from_transform<Scalar, Index>(inst.transform);
            const auto m = inst.mesh_index;
            per_mesh_scale[m] = std::max(per_mesh_scale[m], s);
        });
        for (Index m = 0; m < scene.get_num_meshes(); ++m) {
            if (per_mesh_scale[m] <= Scalar(0)) per_mesh_scale[m] = Scalar(1);
        }

        std::vector<SurfaceMesh<Scalar, Index>> unwrapped_meshes;
        unwrapped_meshes.reserve(num_meshes);
        for (Index m = 0; m < scene.get_num_meshes(); ++m) {
            auto mesh_copy = scene.get_mesh(m);
            if (mesh_copy.get_num_facets() == 0) {
                unwrapped_meshes.emplace_back(std::move(mesh_copy));
                continue;
            }
            // Bake the representative uniform scale into the mesh so packing density accounts
            // for the largest instance.
            Eigen::Matrix<Scalar, 4, 4> sxf = Eigen::Matrix<Scalar, 4, 4>::Identity();
            sxf(0, 0) = sxf(1, 1) = sxf(2, 2) = per_mesh_scale[m];
            apply_transform(mesh_copy, sxf);
            unwrapped_meshes.emplace_back(
                unwrap_mesh<Scalar, Index>(
                    std::move(mesh_copy),
                    unwrap_options,
                    notification_func,
                    cancel));
            // Restore positions (we only wanted scale to influence packing).
            unwrapped_meshes.back() = [&](SurfaceMesh<Scalar, Index> m_unwrapped) {
                // Replace positions with the original ones to undo the bake.
                const auto& orig = scene.get_mesh(m);
                auto src = orig.get_vertex_to_position().get_all();
                auto dst = m_unwrapped.ref_vertex_to_position().ref_all();
                std::copy(src.begin(), src.end(), dst.begin());
                return m_unwrapped;
            }(std::move(unwrapped_meshes.back()));
        }

        for (Index m = 0; m < scene.get_num_meshes(); ++m) {
            const Index new_idx = result.add_mesh(std::move(unwrapped_meshes[m]));
            scene.foreach_instances_for_mesh(m, [&](const InstanceType& inst) {
                InstanceType ni = inst;
                ni.mesh_index = new_idx;
                result.add_instance(ni);
            });
        }
    } else {
        // Per-instance UVs path: each instance becomes its own mesh in the output.
        scene.foreach_instances([&](const InstanceType& inst) {
            const auto m = inst.mesh_index;
            auto mesh_copy = scene.get_mesh(m);
            if (mesh_copy.get_num_facets() == 0) {
                const Index new_idx = result.add_mesh(std::move(mesh_copy));
                InstanceType ni = inst;
                ni.mesh_index = new_idx;
                // Preserve original transform; we only avoid baking when there is no geometry.
                result.add_instance(ni);
                return;
            }

            // Bake instance linear transform (rotation + scale; importance is already folded in
            // by bake_scaling above) into vertex positions so packing reflects the world-space
            // size of this instance. Translation is intentionally NOT baked: it doesn't influence
            // chart size and including it would reduce float precision in xatlas inputs for
            // instances placed far from the origin.
            Eigen::Matrix<Scalar, 4, 4> xf = Eigen::Matrix<Scalar, 4, 4>::Identity();
            xf.template block<3, 3>(0, 0) = inst.transform.matrix().template block<3, 3>(0, 0);
            apply_transform(mesh_copy, xf);

            // Baking transforms positions but leaves any input normal hint in the original space,
            // which would feed xatlas inconsistent data (rotation / non-uniform scale would make
            // normals stale relative to the transformed geometry). Drop the normal hint for the
            // per-instance path; transforming + renormalizing normals here is a future enhancement.
            auto unwrap_options_for_instance = unwrap_options;
            if (!unwrap_options_for_instance.input_normal_attribute_name.empty()) {
                logger().warn(
                    "lagrange::xatlas: ignoring input_normal_attribute_name for per-instance "
                    "unwrap because instance transforms are baked into positions only.");
                unwrap_options_for_instance.input_normal_attribute_name = "";
            }

            auto unwrapped = unwrap_mesh<Scalar, Index>(
                std::move(mesh_copy),
                unwrap_options_for_instance,
                notification_func,
                cancel);

            // Restore positions to their original (unbaked) values.
            const auto& orig = scene.get_mesh(m);
            auto src = orig.get_vertex_to_position().get_all();
            auto dst = unwrapped.ref_vertex_to_position().ref_all();
            std::copy(src.begin(), src.end(), dst.begin());

            const Index new_idx = result.add_mesh(std::move(unwrapped));
            InstanceType ni = inst;
            ni.mesh_index = new_idx;
            result.add_instance(ni);
        });
    }

    // Restore original instance transforms / user_data if we baked importance.
    if (is_scaling_baked) {
        result = scene::internal::unbake_scaling(std::move(result));
    }

    return result;
}

#define LA_X_unwrap_scene(_, S, I)                           \
    template scene::SimpleScene<S, I, 3> unwrap_scene<S, I>( \
        const scene::SimpleScene<S, I, 3>&,                  \
        const UnwrapOptions&,                                \
        const SceneOptions&,                                 \
        std::function<void(const std::string&, float)>,      \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(unwrap_scene, 0)
#undef LA_X_unwrap_scene

} // namespace lagrange::xatlas
