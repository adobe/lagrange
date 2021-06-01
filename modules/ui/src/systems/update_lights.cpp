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
#include <lagrange/ui/systems/update_lights.h>
#include <lagrange/ui/components/Light.h>
#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/utils/lights.h>
#include <lagrange/ui/utils/treenode.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/default_entities.h>


namespace lagrange {
namespace ui {

void update_lights_system(Registry& r){

    //Show visualization mesh of lights only when they are selected
    auto view = r.view<LightComponent>();
    for (auto e : view) {
        const auto & lc = view.get<LightComponent>(e);
        const bool sel = ui::is_selected(r, e);
        const Color render_color = Color(lc.intensity.normalized(), 1.0f);

        ui::foreach_child(r, e, [&](Entity child){ 
            if (!r.has<MeshGeometry>(child)) return;
            if (sel) ui::add_to_layer(r, child, DefaultLayers::Default);
            else
                ui::remove_from_layer(r, child, DefaultLayers::Default);

            ui::get_material(r, child)->set_color("in_color", render_color);
        });
    }

    //Handle change of type <> viz mesh
    //TODO

}

} // namespace ui
} // namespace lagrange