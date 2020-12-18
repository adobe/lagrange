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
#include <lagrange/ui/Material.h>
#include <lagrange/ui/render_passes/BRDFLUTPass.h>
#include <lagrange/ui/render_passes/ShadowMapPass.h>
#include <lagrange/ui/render_passes/common.h>

#include <array>
#include <memory>


namespace lagrange {
namespace ui {

struct GroundPassData
{
    Resource<Shader> shader;
    Resource<CommonPassData> common;

    Resource<std::vector<EmitterRenderData>> emitters = {};
    Resource<Texture> brdflut = {};
    std::shared_ptr<Material> material = Material::create_default_shared();
};

using GroundPass = RenderPass<GroundPassData>;

template <>
std::string default_render_pass_name<GroundPass>();

std::unique_ptr<GroundPass> create_grid_pass(
    Resource<CommonPassData> common,
    ShadowMapPass* shadow_map_pass,
    BRDFLUTPass* brdflut_pass);


class Ground
{
public:
    /// Creates a ground tied to ground rendering pass
    Ground(GroundPass& pass);

    /// Set ground height (in Y direction)
    Ground& set_height(float height);

    /// Get ground height  (in Y direction)
    float get_height() const;


    /// PBR rendered surface

    /// Enable PBR rendered ground surface
    Ground& enable_surface(bool enable);

    /// Set PBR material for the surface
    Ground& set_surface_material(std::shared_ptr<Material> mat);

    /// Get PBR material for the surface
    Material& get_surface_material();

    /// Get PBR material for the surface
    const Material& get_surface_material() const;


    /// Grid

    /// Enable grid
    Ground& enable_grid(bool enable);

    /// Set grid color
    Ground& set_grid_color(Color color);

    /// Get grid color
    Color get_grid_color() const;

    /// Set grid period, primary period being thicker
    Ground& set_grid_period(float primary, float secondary);

    ///  Set primary and secondary period, primary period being thicker
    Ground& set_grid_period(std::pair<float, float> period);

    /// Get grid period (primary and secondary)
    std::pair<float, float> get_grid_period() const;


    /// Axes

    /// Enable XZ axes (rendered red & blue)
    Ground& enable_axes(bool enable);


    /// Shadow catcher

    /// Enable shadow catcher mode, disables PBR surface rendering
    Ground& enable_shadow_catcher(bool enable);

    /// Set opacity of the catched shadow
    Ground& set_shadow_catcher_opacity(float opacity);

    /// Get opacity of the catched shadow
    float get_shadow_catcher_opacity() const;


private:
    GroundPass& m_pass;
};

} // namespace ui
} // namespace lagrange
