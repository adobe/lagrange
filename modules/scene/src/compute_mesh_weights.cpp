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
#include <lagrange/scene/compute_mesh_weights.h>

#include <lagrange/compute_area.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>


namespace lagrange::scene {

namespace {

template <typename Scalar, typename Index, size_t Dimension>
Scalar compute_mesh_max_surface_area(
    const SimpleScene<Scalar, Index, Dimension>& scene,
    const Index mesh_index)
{
    typename SimpleScene<Scalar, Index, Dimension>::MeshType mesh_copy =
        scene.get_mesh(mesh_index); // trusting copy on write mechanism

    FacetAreaOptions facet_area_options;
    facet_area_options.output_attribute_name = "@facet_transformed_area";
    const MeshAreaOptions mesh_area_options = {
        facet_area_options.output_attribute_name,
        facet_area_options.use_signed_area};

    auto result = static_cast<Scalar>(0);
    // The area computation depends on axis scaling, and so, must take care of the instances
    // transformation. It is however still possible to restrict the computation to a limited set of
    // transforms by pre-analyzing the transformation (uniform scaling etc.).
    scene.foreach_instances_for_mesh(
        mesh_index,
        [&](const typename SimpleScene<Scalar, Index, Dimension>::InstanceType& instance) {
            compute_facet_area<Scalar, Index, Dimension>(
                mesh_copy,
                instance.transform,
                facet_area_options);
            result = std::max(result, compute_mesh_area(mesh_copy, mesh_area_options));
        });
    return result;
}

} // namespace

template <typename Scalar, typename Index, size_t Dimension>
std::vector<double> compute_mesh_weights(
    const SimpleScene<Scalar, Index, Dimension>& scene,
    const FacetAllocationStrategy facet_allocation_strategy)
{
    std::vector<double> weights;
    weights.reserve(scene.get_num_meshes());

    switch (facet_allocation_strategy) {
    case FacetAllocationStrategy::EvenSplit: {
        const auto weight = 1. / static_cast<double>(scene.get_num_meshes());
        weights.resize(scene.get_num_meshes(), weight);
        break;
    }
    case FacetAllocationStrategy::RelativeToMeshArea: {
        for (auto mesh_index : range(scene.get_num_meshes())) {
            weights.emplace_back(
                static_cast<double>(compute_mesh_max_surface_area(scene, mesh_index)));
        }
        Eigen::Map<Eigen::VectorXd> weights_map(weights.data(), weights.size());
        weights_map /= weights_map.sum();
        break;
    }
    case FacetAllocationStrategy::RelativeToNumFacets: {
        for (auto mesh_index : range(scene.get_num_meshes())) {
            weights.emplace_back(static_cast<double>(scene.get_mesh(mesh_index).get_num_facets()));
        }
        Eigen::Map<Eigen::VectorXd> weights_map(weights.data(), weights.size());
        weights_map /= weights_map.sum();
        break;
    }
    }

    la_debug_assert(weights.size() == scene.get_num_meshes());
    la_debug_assert(Eigen::Map<Eigen::VectorXd>(weights.data(), weights.size()).allFinite());
    return weights;
}

#define LA_X_compute_mesh_weights(_, Scalar, Index, Dim) \
    template std::vector<double> compute_mesh_weights(   \
        const SimpleScene<Scalar, Index, Dim>& scene,    \
        const FacetAllocationStrategy facet_allocation_strategy);
LA_SIMPLE_SCENE_X(compute_mesh_weights, 0)

} // namespace lagrange::scene
