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

#include <lagrange/scene/Scene.h>
#include <lagrange/testing/check_meshes_equal.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::testing {

///
/// Check that two scenes are bitwise identical.
///
/// Verifies scene name, meshes, nodes (hierarchy, transforms, mesh/camera/light references),
/// images, textures, materials, lights, cameras (including optional far_plane),
/// skeletons, and animations.
/// This is suitable for verifying lossless serialization round-trips.
///
/// @note Extensions and user_data are NOT compared by this function.
///
/// @param[in]  a  First scene.
/// @param[in]  b  Second scene.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
void check_scenes_equal(const scene::Scene<Scalar, Index>& a, const scene::Scene<Scalar, Index>& b)
{
    REQUIRE(a.name == b.name);

    // Meshes
    REQUIRE(a.meshes.size() == b.meshes.size());
    for (size_t i = 0; i < a.meshes.size(); ++i) {
        check_meshes_equal(a.meshes[i], b.meshes[i]);
    }

    // Root nodes
    REQUIRE(a.root_nodes.size() == b.root_nodes.size());
    for (size_t i = 0; i < a.root_nodes.size(); ++i) {
        REQUIRE(a.root_nodes[i] == b.root_nodes[i]);
    }

    // Nodes
    REQUIRE(a.nodes.size() == b.nodes.size());
    for (size_t i = 0; i < a.nodes.size(); ++i) {
        const auto& na = a.nodes[i];
        const auto& nb = b.nodes[i];
        REQUIRE(na.name == nb.name);
        REQUIRE(na.transform.matrix().isApprox(nb.transform.matrix(), 0.f));
        REQUIRE(na.parent == nb.parent);
        REQUIRE(na.children.size() == nb.children.size());
        for (size_t j = 0; j < na.children.size(); ++j) {
            REQUIRE(na.children[j] == nb.children[j]);
        }
        REQUIRE(na.meshes.size() == nb.meshes.size());
        for (size_t j = 0; j < na.meshes.size(); ++j) {
            REQUIRE(na.meshes[j].mesh == nb.meshes[j].mesh);
            REQUIRE(na.meshes[j].materials.size() == nb.meshes[j].materials.size());
            for (size_t k = 0; k < na.meshes[j].materials.size(); ++k) {
                REQUIRE(na.meshes[j].materials[k] == nb.meshes[j].materials[k]);
            }
        }
        REQUIRE(na.cameras.size() == nb.cameras.size());
        for (size_t j = 0; j < na.cameras.size(); ++j) {
            REQUIRE(na.cameras[j] == nb.cameras[j]);
        }
        REQUIRE(na.lights.size() == nb.lights.size());
        for (size_t j = 0; j < na.lights.size(); ++j) {
            REQUIRE(na.lights[j] == nb.lights[j]);
        }
    }

    // Images
    REQUIRE(a.images.size() == b.images.size());
    for (size_t i = 0; i < a.images.size(); ++i) {
        REQUIRE(a.images[i].name == b.images[i].name);
        REQUIRE(a.images[i].image.width == b.images[i].image.width);
        REQUIRE(a.images[i].image.height == b.images[i].image.height);
        REQUIRE(a.images[i].image.num_channels == b.images[i].image.num_channels);
        REQUIRE(a.images[i].image.element_type == b.images[i].image.element_type);
        REQUIRE(a.images[i].image.data == b.images[i].image.data);
        REQUIRE(a.images[i].uri == b.images[i].uri);
    }

    // Textures
    REQUIRE(a.textures.size() == b.textures.size());
    for (size_t i = 0; i < a.textures.size(); ++i) {
        const auto& ta = a.textures[i];
        const auto& tb = b.textures[i];
        REQUIRE(ta.name == tb.name);
        REQUIRE(ta.image == tb.image);
        REQUIRE(ta.mag_filter == tb.mag_filter);
        REQUIRE(ta.min_filter == tb.min_filter);
        REQUIRE(ta.wrap_u == tb.wrap_u);
        REQUIRE(ta.wrap_v == tb.wrap_v);
        REQUIRE(ta.scale.isApprox(tb.scale, 0.f));
        REQUIRE(ta.offset.isApprox(tb.offset, 0.f));
        REQUIRE(ta.rotation == tb.rotation);
    }

    // Materials
    REQUIRE(a.materials.size() == b.materials.size());
    for (size_t i = 0; i < a.materials.size(); ++i) {
        const auto& ma = a.materials[i];
        const auto& mb = b.materials[i];
        REQUIRE(ma.name == mb.name);
        REQUIRE(ma.base_color_value.isApprox(mb.base_color_value, 0.f));
        REQUIRE(ma.emissive_value.isApprox(mb.emissive_value, 0.f));
        REQUIRE(ma.metallic_value == mb.metallic_value);
        REQUIRE(ma.roughness_value == mb.roughness_value);
        REQUIRE(ma.alpha_mode == mb.alpha_mode);
        REQUIRE(ma.alpha_cutoff == mb.alpha_cutoff);
        REQUIRE(ma.normal_scale == mb.normal_scale);
        REQUIRE(ma.occlusion_strength == mb.occlusion_strength);
        REQUIRE(ma.double_sided == mb.double_sided);
        REQUIRE(ma.base_color_texture.index == mb.base_color_texture.index);
        REQUIRE(ma.base_color_texture.texcoord == mb.base_color_texture.texcoord);
        REQUIRE(ma.emissive_texture.index == mb.emissive_texture.index);
        REQUIRE(ma.metallic_roughness_texture.index == mb.metallic_roughness_texture.index);
        REQUIRE(ma.normal_texture.index == mb.normal_texture.index);
        REQUIRE(ma.occlusion_texture.index == mb.occlusion_texture.index);
    }

    // Lights
    REQUIRE(a.lights.size() == b.lights.size());
    for (size_t i = 0; i < a.lights.size(); ++i) {
        const auto& la = a.lights[i];
        const auto& lb = b.lights[i];
        REQUIRE(la.name == lb.name);
        REQUIRE(la.type == lb.type);
        REQUIRE(la.position.isApprox(lb.position, 0.f));
        REQUIRE(la.direction.isApprox(lb.direction, 0.f));
        REQUIRE(la.up.isApprox(lb.up, 0.f));
        REQUIRE(la.intensity == lb.intensity);
        REQUIRE(la.attenuation_constant == lb.attenuation_constant);
        REQUIRE(la.attenuation_linear == lb.attenuation_linear);
        REQUIRE(la.attenuation_quadratic == lb.attenuation_quadratic);
        REQUIRE(la.attenuation_cubic == lb.attenuation_cubic);
        REQUIRE(la.range == lb.range);
        REQUIRE(la.color_diffuse.isApprox(lb.color_diffuse, 0.f));
        REQUIRE(la.color_specular.isApprox(lb.color_specular, 0.f));
        REQUIRE(la.color_ambient.isApprox(lb.color_ambient, 0.f));
        REQUIRE(la.angle_inner_cone == lb.angle_inner_cone);
        REQUIRE(la.angle_outer_cone == lb.angle_outer_cone);
        REQUIRE(la.size.isApprox(lb.size, 0.f));
    }

    // Cameras
    REQUIRE(a.cameras.size() == b.cameras.size());
    for (size_t i = 0; i < a.cameras.size(); ++i) {
        const auto& ca = a.cameras[i];
        const auto& cb = b.cameras[i];
        REQUIRE(ca.name == cb.name);
        REQUIRE(ca.type == cb.type);
        REQUIRE(ca.position.isApprox(cb.position, 0.f));
        REQUIRE(ca.up.isApprox(cb.up, 0.f));
        REQUIRE(ca.look_at.isApprox(cb.look_at, 0.f));
        REQUIRE(ca.near_plane == cb.near_plane);
        REQUIRE(ca.far_plane.has_value() == cb.far_plane.has_value());
        if (ca.far_plane.has_value()) {
            REQUIRE(ca.far_plane.value() == cb.far_plane.value());
        }
        REQUIRE(ca.orthographic_width == cb.orthographic_width);
        REQUIRE(ca.aspect_ratio == cb.aspect_ratio);
        REQUIRE(ca.horizontal_fov == cb.horizontal_fov);
    }

    // Skeletons
    REQUIRE(a.skeletons.size() == b.skeletons.size());
    for (size_t i = 0; i < a.skeletons.size(); ++i) {
        REQUIRE(a.skeletons[i].meshes.size() == b.skeletons[i].meshes.size());
        for (size_t j = 0; j < a.skeletons[i].meshes.size(); ++j) {
            REQUIRE(a.skeletons[i].meshes[j] == b.skeletons[i].meshes[j]);
        }
    }

    // Animations
    REQUIRE(a.animations.size() == b.animations.size());
    for (size_t i = 0; i < a.animations.size(); ++i) {
        REQUIRE(a.animations[i].name == b.animations[i].name);
    }
}

} // namespace lagrange::testing
