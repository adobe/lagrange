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

#include <lagrange/ui/utils/layer.h>

namespace lagrange {
namespace ui {


using LayerNames = std::array<std::string, get_max_layers()>;


LayerIndex register_layer_name(Registry& registry, const std::string& name, LayerIndex index)
{
    auto& names = registry.ctx().emplace<LayerNames>();
    names[index] = name;
    return index;
}

LayerIndex register_layer_name(Registry& registry, const std::string& name)
{
    auto index = get_next_available_layer_index(registry);
    return register_layer_name(registry, name, index);
}

const std::string& get_layer_name(Registry& registry, LayerIndex index)
{
    auto& names = registry.ctx().emplace<LayerNames>();
    return names[index];
}

void register_default_layer_names(Registry& registry)
{
    register_layer_name(registry, "Default", DefaultLayers::Default);
    register_layer_name(registry, "Selection", DefaultLayers::Selection);
    register_layer_name(registry, "Hover", DefaultLayers::Hover);
    register_layer_name(registry, "NoShadow", DefaultLayers::NoShadow);
}


void add_to_layer(Registry& registry, Entity e, LayerIndex index)
{
    auto& c = registry.get_or_emplace<Layer>(e);
    c.set(size_t(index), true);
}

void remove_from_layer(Registry& registry, Entity e, LayerIndex index)
{
    auto& c = registry.get_or_emplace<Layer>(e);
    c.set(size_t(index), false);
}

bool is_in_layer(Registry& registry, Entity e, LayerIndex index)
{
    auto& c = registry.get_or_emplace<Layer>(e);
    return c.test(index);
}

bool is_in_any_layers(Registry& registry, Entity e, Layer layers_bitset)
{
    auto& c = registry.get_or_emplace<Layer>(e);
    return (c & layers_bitset).any();
}


bool is_visible_in(
    const Registry& registry,
    Entity e,
    const Layer& visible_layers,
    const Layer& hidden_layers)
{
    // If is in any visible layer and not in any hidden layer
    if (registry.all_of<Layer>(e)) {
        const auto& layer = registry.get<Layer>(e);

        if (!(layer & visible_layers).any()) return false;

        if ((layer & hidden_layers).any()) {
            return false;
        }
    }
    // Or if they have default layer set implicitly (by not having Layer component)
    // and the rendering context doesn't show default layer
    else if (!visible_layers.test(DefaultLayers::Default)) {
        return false;
    }

    return true;
}

LayerIndex get_next_available_layer_index(Registry& r)
{
    const auto& names = r.ctx().get<LayerNames>();
    for (size_t i = 0; i < names.size(); i++) {
        if (names[i].length() == 0) return LayerIndex(i);
    }
    return 0;
}

} // namespace ui
} // namespace lagrange
