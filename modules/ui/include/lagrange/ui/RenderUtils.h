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
#include <lagrange/ui/default_render_passes.h>
#include <lagrange/ui/default_resources.h>


namespace lagrange {
namespace ui {

class Material;

namespace utils {
namespace render {

///
/// Uploads uniform variables for lights to shader
///
/// Binds shadow maps to next available tex unit
/// tex_unit_counter will be incremented for each used emitter
void set_emitter_uniforms(
    Shader& shader, const std::vector<EmitterRenderData>& emitters, int& tex_unit_counter);


///
/// Uploads uniforms specific to PBR
///
/// Binds provided precomputed BRDF Lookup Table texture 
///
void set_pbr_uniforms(Shader & shader, Texture & brdf_lut_texture, int &tex_unit_counter);

///
/// Sets OpenGL state to offset rendered polygons by an epsilon value
///
/// This makes sure that objects rendered with different render passes are displayed correctly
void set_render_layer(GLScope& scope, int layer_index);


///
/// Sets render pass default OpenGL state
///
/// Multisample: off
/// Blending: a_src, 1-a_src, 1, 1
/// Depth clamping: on
/// Depth func: less or equal
/// Seamless cube maps: on
void set_render_pass_defaults(GLScope& scope);


////
/// Computes a plane perpendicular to direction
///
/// Returns a pair of orthogonal directions, that together with direction form a orthogonal basis
std::pair<Eigen::Vector3f, Eigen::Vector3f> compute_perpendicular_plane(Eigen::Vector3f direction);

/// Uploads uniform variable and optionally binds the texture to the shader
void set_map(Shader& shader, const std::string& name, const Material& material, int& tex_unit_counter);

} // namespace render
} // namespace utils
} // namespace ui
} // namespace lagrange
