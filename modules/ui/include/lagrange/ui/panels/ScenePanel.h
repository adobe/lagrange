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

#include <lagrange/ui/api.h>
#include <lagrange/io/load_mesh_ext.h>
#include <lagrange/ui/Entity.h>
#include <lagrange/ui/components/Layer.h>

#include <unordered_set>

namespace lagrange {
namespace ui {

struct ScenePanel
{
    ScenePanel()
    {
        mesh_params.normalize = true;

        // Set visible layers to every layer
        visible_layers.reset();
        visible_layers.flip();
    }
    io::MeshLoaderParams mesh_params;
    ui::Layer visible_layers;

    bool show_tree_view = true;
    bool show_mesh_entities = true;
    bool show_scene_panel_layers = true;
    bool show_all_entities = true;

    std::unordered_set<Entity> tree_view_ignored_entities;
};


LA_UI_API Entity add_scene_panel(Registry& r, const std::string& name = "Scene");

} // namespace ui
} // namespace lagrange
