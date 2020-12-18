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
#include <IconsFontAwesome5.h>
#include <lagrange/ui/UI.h>
#include <lagrange/ui/UIWidget.h>
#include <misc/cpp/imgui_stdlib.h>

#include <array>

using namespace lagrange::ui;

struct VizBuilder {
    const std::array<Viz::Attribute, 4> attrib_types = {Viz::Attribute::VERTEX,
        Viz::Attribute::EDGE,
        Viz::Attribute::FACET,
        Viz::Attribute::CORNER};

    const std::array<Viz::Primitive, 3> primitive_types = {Viz::Primitive::POINTS,
        Viz::Primitive::LINES,
        Viz::Primitive::TRIANGLES};

    const std::array<Viz::Shading, 3> shading_types = {Viz::Shading::FLAT,
        Viz::Shading::PHONG,
        Viz::Shading::PBR};

    const std::array<Viz::Colormapping, 4> colormapping_types = {Viz::Colormapping::UNIFORM,
        Viz::Colormapping::TEXTURE,
        Viz::Colormapping::CUSTOM,
        Viz::Colormapping::CUSTOM_INDEX_OBJECT};

    const std::array<Viz::Filter, 3> filter_types = {Viz::Filter::SHOW_ALL,
        Viz::Filter::SHOW_SELECTED,
        Viz::Filter::HIDE_SELECTED};


    bool indexed_colormap = true;
    const char* indexed_colormap_fn_names[3] = {
        "Turbo periodic index",
        "Grayscale periodic index",
        "RGB position",
    };
    int indexed_colormap_fn_index = 0;

    const char* value_colormap_fn_names[3] = {
        "Norm of value to Turbo",
        "Attribute to RGBA",
        "Invert color"
    };
    int value_colormap_fn_index = 0;


    const char* attribute_names[4] = {"dijkstra_distance",
        "random_vertex_attribute",
        "geodesic_distance",
        "polar_angle"
    };
    int attribute_name_index = 0;

    Viz cfg;

    VizBuilder();

    template <typename T, size_t n>
    void combo_box(const char* name, const std::array<T, n>& types, T& value)
    {
        if (ImGui::BeginCombo(name, Viz::to_string(value).c_str())) {
            for (size_t i = 0; i < types.size(); i++) {
                bool selected = types[i] == value;
                if (ImGui::Selectable(Viz::to_string(types[i]).c_str(), selected)) {
                    value = types[i];
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    void operator()(Viewer& v);

    void assign_custom_func();
};
