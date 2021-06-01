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

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/components/Layer.h>
#include <string>

namespace lagrange {
namespace ui {

// TODO move to default_layers or one setup_defaults file
struct DefaultLayers
{
    constexpr static const LayerIndex Default = 0;
    constexpr static const LayerIndex Selection = 255 - 1;
    constexpr static const LayerIndex Hover = 255 - 2;
    constexpr static const LayerIndex NoShadow = 255 - 3;
};


void add_to_layer(Registry& registry, Entity e, LayerIndex index);

void remove_from_layer(Registry& registry, Entity e, LayerIndex index);

bool is_in_layer(Registry& registry, Entity e, LayerIndex index);

bool is_in_any_layers(Registry& registry, Entity e, Layer layers_bitset);

bool is_visible_in(
    const Registry& registry,
    Entity e,
    const Layer& visible_layers,
    const Layer& hidden_layers);

LayerIndex get_next_available_layer_index(Registry& r);

LayerIndex register_layer_name(Registry& registry, const std::string& name, LayerIndex index);
LayerIndex register_layer_name(Registry& registry, const std::string& name);


const std::string& get_layer_name(Registry& registry, LayerIndex index);

void register_default_layer_names(Registry& registry);


} // namespace ui
} // namespace lagrange