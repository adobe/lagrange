/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/ui/components/Light.h>
#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/default_entities.h>
#include <lagrange/ui/systems/update_lights.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/lights.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/treenode.h>

#include <lagrange/ui/utils/bounds.h>
#include <lagrange/ui/utils/immediate.h>


namespace lagrange {
namespace ui {

struct TemporaryLigthVisualization
{
    Entity entity;
};

ui::Entity add_light_visualization_mesh(Registry& r, ui::Entity light_e)
{
    const auto& lc = r.get<LightComponent>(light_e);


    // Create mesh
    ui::Entity mesh_e = ui::NullEntity;
    if (lc.type == LightComponent::Type::POINT) {
        mesh_e = ui::register_mesh(r, lagrange::create_sphere());
    } else {
        mesh_e = ui::register_mesh(r, lagrange::create_cube());
    }

    // Show mesh
    auto viz = ui::show_mesh(r, mesh_e, DefaultShaders::TrianglesToLines);
    ui::set_parent(r, viz, light_e);
    ui::add_to_layer(r, viz, DefaultLayers::NoShadow);


    if (lc.type == LightComponent::Type::DIRECTIONAL) {
        Eigen::Vector3f dims =
            (Eigen::Vector3f::Ones() + 100 * get_canonical_light_direction()).normalized() * 10.0f;
        ui::set_transform(r, viz, Eigen::Scaling(dims));
    } else if (lc.type == LightComponent::Type::POINT) {
        ui::set_transform(r, viz, Eigen::Scaling(0.1f, 0.1f, 0.1f));
    } else if (lc.type == LightComponent::Type::SPOT) {
        Eigen::Vector3f dims =
            (Eigen::Vector3f::Ones() + 3 * get_canonical_light_direction()).normalized() * 1.0f;
        ui::set_transform(r, viz, Eigen::Scaling(dims));
    }
    return viz;
}


void update_lights_system(Registry& r)
{
    // Show visualization mesh of lights only when they are selected
    auto view = r.view<LightComponent>();
    for (auto e : view) {
        const bool selected = ui::is_selected(r, e) || ui::is_child_selected(r, e, true);


        if (selected) {
            auto bb = get_scene_bounds(r);
            auto len = bb.global.diagonal().norm();
            auto dir = (ui::get_transform(r, e).global.linear() * get_canonical_light_direction())
                           .normalized()
                           .eval();

            render_lines(r, {-len * dir, len * dir});
        }

#if 0

        const auto& lc = view.get<LightComponent>(e);

        if (selected) {
            if (!r.all_of<TemporaryLigthVisualization>(e)) {
                r.emplace<TemporaryLigthVisualization>(
                    e,
                    TemporaryLigthVisualization{add_light_visualization_mesh(r, e)});
            }

            //Update
            const auto viz_e = r.get<TemporaryLigthVisualization>(e).entity;
            const Color render_color = Color(lc.intensity.normalized(), 1.0f);
            ui::get_material(r, viz_e)->set_color("in_color", render_color);

        } else {
            if (r.all_of<TemporaryLigthVisualization>(e)) {
                ui::remove(r, r.get<TemporaryLigthVisualization>(e).entity, true);
                r.remove<TemporaryLigthVisualization>(e);
            }
        }
#endif


        // if (sel && ui::get_children())


        /*const Color render_color = Color(lc.intensity.normalized(), 1.0f);

        ui::foreach_child(r, e, [&](Entity child) {
            if (!r.all_of<MeshGeometry>(child)) return;
            if (sel)
                ui::add_to_layer(r, child, DefaultLayers::Default);
            else
                ui::remove_from_layer(r, child, DefaultLayers::Default);

            ui::get_material(r, child)->set_color("in_color", render_color);
        });*/
    }

    // Handle change of type <> viz mesh
    // TODO
}

} // namespace ui
} // namespace lagrange
