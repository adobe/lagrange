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
#include "VizBuilder.h"


Color index_turbo(const Model&, int index)
{
    return colormap_turbo((index % 16) / 16.0f);
}

Color index_grayscale(const Model&, int index)
{
    float v = (index % 16) / 16.0f;
    return Color(v, v, v, 1.0f);
}


Color index_rgb_pos_vertex(const Model& model, int index)
{
    Color result;
    model.visit_tuple_const<SupportedMeshTypes3D>([&](auto& meshmodel) {
        const auto& v = meshmodel.get_mesh().get_vertices().row(index).eval();
        auto bb = meshmodel.get_bounds();
        result = Color(bb.normalize_point(v.transpose()), 1.0f);
    });

    return result;
}

Color index_rgb_pos_edge(const Model& model, int index)
{
    Color result;
    model.visit_tuple_const<SupportedMeshTypes3D>([&](auto& meshmodel) {
        auto& mesh = meshmodel.get_mesh();
        const auto& e = mesh.get_edges()[index];
        const auto& v1 = mesh.get_vertices().row(e.v1()).eval();
        const auto& v2 = mesh.get_vertices().row(e.v2()).eval();
        const auto v = (v1 + v2) * 0.5;

        auto bb = meshmodel.get_bounds();
        result = Color(bb.normalize_point(v.transpose()), 1.0f);
    });

    return result;
}

Color index_rgb_pos_facet(const Model& model, int index)
{
    Color result;
    model.visit_tuple_const<SupportedMeshTypes3D>([&](auto& meshmodel) {
        auto& mesh = meshmodel.get_mesh();
        auto& V = mesh.get_vertices();
        auto& F = mesh.get_facets();
        Eigen::Vector3d v(0, 0, 0);
        int count = 0;
        for (auto i = 0; i < F.cols(); i++) {
            auto v_index = F(index, i);
            if (v_index > V.rows()) break;
            v += V.row(v_index).template cast<double>();
            count++;
        }

        v *= 1.0f / (count);
        auto bb = meshmodel.get_bounds();
        result = Color(bb.normalize_point(v), 1.0f);
    });

    return result;
}

Color index_rgb_pos_corner(const Model& model, int index)
{
    Color result;
    model.visit_tuple_const<SupportedMeshTypes3D>([&](auto& meshmodel) {
        auto& mesh = meshmodel.get_mesh();
        auto& V = mesh.get_vertices();
        auto& F = mesh.get_facets();
        auto v = V.row(F(index / F.cols(), index % F.cols())).eval();
        auto bb = meshmodel.get_bounds();
        result = Color(bb.normalize_point(v.transpose()),1.0f);
    });

    return result;
}


Color value_to_rgba(const Model&, const Viz::AttribValue& v)
{
    Color c(0, 0, 0, 1);
    for (auto i = 0; i < std::min(4, int(v.cols())); i++) c(i) = float(v(i));
    return c;
}



Color value_invert_color(const Model&, const Viz::AttribValue& v)
{
    Color c(1, 1, 1, 1);
    for (auto i = 0; i < std::min(4, int(v.cols())); i++) c(i) = float(1.0 - v(i));
    return c;
}

Color value_norm_to_turbo(const Model&, const Viz::AttribValue& v)
{
    return colormap_turbo(static_cast<float>(v.norm()));
}


VizBuilder::VizBuilder()
{
    cfg.attribute_name = "random_vertex_attribute";
    cfg.attribute = Viz::Attribute::VERTEX;
    cfg.primitive = Viz::Primitive::TRIANGLES;
    cfg.colormapping = Viz::Colormapping::CUSTOM;
}

void VizBuilder::operator()(Viewer& v)
{
    cfg.viz_name = Viz::to_string(cfg.attribute) + " -> " + Viz::to_string(cfg.primitive) + "[" +
                   Viz::to_string(cfg.colormapping) + "]" + "[" + Viz::to_string(cfg.shading) + "]";

    while (v.get_renderer().get_pipeline().get_pass(cfg.viz_name)) cfg.viz_name += "_";

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Basic")) {
        ImGui::InputText("Viz name", &cfg.viz_name);

        combo_box<Viz::Colormapping, 4>("Colormapping", colormapping_types, cfg.colormapping);

        if (cfg.colormapping == Viz::Colormapping::UNIFORM) {
            UIWidget("Uniform color")(cfg.uniform_color);
        }
        else if (cfg.colormapping == Viz::Colormapping::CUSTOM) {
            if (ImGui::RadioButton("By Index", indexed_colormap)) {
                indexed_colormap = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("By Value", !indexed_colormap)) {
                indexed_colormap = false;
            }

            if (indexed_colormap) {
                ImGui::Combo(
                    "Function (indexed)", &indexed_colormap_fn_index, indexed_colormap_fn_names, 3);
                cfg.attribute_name = "";
            }
            else {
                ImGui::Combo(
                    "Attribute name", &attribute_name_index, attribute_names, 4);

                cfg.attribute_name = attribute_names[attribute_name_index];

                ImGui::Combo(
                    "Function (value)", &value_colormap_fn_index, value_colormap_fn_names, 3);
            }
        }

        if (cfg.colormapping != Viz::Colormapping::UNIFORM) {
            combo_box<Viz::Attribute, 4>("Attributes", attrib_types, cfg.attribute);
        }

        combo_box<Viz::Primitive, 3>("Primitive", primitive_types, cfg.primitive);


        combo_box<Viz::Shading, 3>("Shading", shading_types, cfg.shading);

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Advanced")) {
        combo_box<Viz::Filter, 3>("Filter Global", filter_types, cfg.filter_global);
        combo_box<Viz::Filter, 3>("Filter Local", filter_types, cfg.filter_local);

        ImGui::SliderFloat("Backside Alpha", &cfg.backside_alpha, 0, 1);
        ImGui::Checkbox("Cull Backface", &cfg.cull_backface);
        ImGui::Checkbox("Replace with bounds", &cfg.replace_with_bounding_box);
        ImGui::InputText("Custom Sub Buffer ID", &cfg.custom_sub_buffer_id);
        ImGui::Checkbox("FBOConfig::create_color", &cfg.fbo_config.create_color);
        ImGui::Checkbox("FBOConfig::create_depth", &cfg.fbo_config.create_depth);
        ImGui::Text("FBOConfig::target_fbo %u", cfg.fbo_config.target_fbo->get_id());
        ImGui::TreePop();
    }


    if (ImGui::Button("Create", ImVec2(ImGui::GetContentRegionAvail().x, 40.f))) {
        assign_custom_func();


        try {
            v.add_viz(cfg, true);
            cfg.uniform_color = Color::random(0);
        }
        catch (std::exception& ex) {
            lagrange::logger().error("Couldn't create render pass:\n{}", ex.what());
        }
    }
}

void VizBuilder::assign_custom_func()
{
    cfg.custom_index_color_fn = nullptr;
    cfg.custom_attrib_color_fn = nullptr;


    if (cfg.colormapping == Viz::Colormapping::CUSTOM) {
        if (indexed_colormap) {
            switch (indexed_colormap_fn_index) {
            case 0: cfg.custom_index_color_fn = index_turbo; break;
            case 1: cfg.custom_index_color_fn = index_grayscale; break;
            case 2:
                switch (cfg.attribute) {
                case lagrange::ui::Viz::Attribute::VERTEX:
                    cfg.custom_index_color_fn = index_rgb_pos_vertex;
                    break;
                case lagrange::ui::Viz::Attribute::EDGE:
                    cfg.custom_index_color_fn = index_rgb_pos_edge;
                    break;
                case lagrange::ui::Viz::Attribute::FACET:
                    cfg.custom_index_color_fn = index_rgb_pos_facet;
                    break;
                case lagrange::ui::Viz::Attribute::CORNER:
                    cfg.custom_index_color_fn = index_rgb_pos_corner;
                    break;
                default: break;
                }
                break;
            }
        }
        else {
            switch (value_colormap_fn_index) {
            case 0: cfg.custom_attrib_color_fn = value_norm_to_turbo; break;
            case 1: cfg.custom_attrib_color_fn = value_to_rgba; break;
            case 2: cfg.custom_attrib_color_fn = value_invert_color; break;
            }
        }
    }
}
