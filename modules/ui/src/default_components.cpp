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


#include <lagrange/ui/default_components.h>
#include <lagrange/ui/default_events.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/imgui/UIWidget.h>
#include <lagrange/ui/imgui/buttons.h>
#include <lagrange/ui/utils/colormap.h>
#include <lagrange/ui/utils/events.h>
#include <lagrange/ui/utils/file_dialog.h>
#include <lagrange/ui/utils/ibl.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/lights.h>
#include <lagrange/ui/utils/mesh_picking.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/utils/utils.h>
#include <misc/cpp/imgui_stdlib.h>

#include <lagrange/ui/imgui/progress.h>

#include <IconsFontAwesome5.h>
#include <imgui.h>


namespace lagrange {
namespace ui {


namespace {
bool browse_texture_widget(const std::shared_ptr<Texture>& tex_ptr, int widget_size = 64)
{
    bool browse_clicked = false;
    if (tex_ptr) {
        browse_clicked = UIWidget("##tex")(*tex_ptr, widget_size, widget_size);
    } else {
        browse_clicked = ImGui::Button("Browse", ImVec2(float(widget_size), float(widget_size)));
    }
    return browse_clicked;
}


std::shared_ptr<Texture> load_texture_dialog(const Texture::Params& default_params)
{
    auto path =
        ui::open_file("Load a texture", ".", {{"All Images", "*.jpg *.png *.gif *.exr *.bmp"}});
    if (!path.empty()) {
        try {
            return std::make_shared<Texture>(path, default_params);

        } catch (const std::exception& ex) {
            lagrange::logger().error("Failed to load texture '{}' : {}", path.string(), ex.what());
        }
    }
    return nullptr;
}


void show_mesh_geometry(Registry* rptr, Entity orig_e)
{
    Registry& r = *rptr;
    auto& m = r.get<MeshGeometry>(orig_e);

    auto& mesh_data = r.get<MeshData>(m.entity);
    la_debug_assert(mesh_data.type != entt::type_id<void>(), "Type must be assigned");
    auto& typeinfo = mesh_data.type;

    ImGui::Text("Entity ID: %d", int(m.entity));
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_CHEVRON_RIGHT)) {
        ui::select(r, m.entity);
    }
    ImGui::Text("MeshType: %s", typeinfo.name().data());

    const size_t num_vertices = get_num_vertices(mesh_data);
    if (num_vertices == 0) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::Spectrum::RED400);
    ImGui::Text("Vertices: %zu", num_vertices);
    if (num_vertices == 0) ImGui::PopStyleColor();

    const size_t num_facets = get_num_facets(mesh_data);
    if (num_facets == 0) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::Spectrum::RED400);
    ImGui::Text("Facets: %zu", num_facets);
    if (num_facets == 0) ImGui::PopStyleColor();

    int sub_id = m.submesh_index;
    if (m.submesh_index == entt::null) {
        ImGui::Text("Submesh/Material ID not set");
        ImGui::SameLine();

        if (ImGui::Button("Set")) {
            m.submesh_index = 0;
        }
    } else {
        if (ImGui::InputInt("Submesh/Material ID", &sub_id) && sub_id >= 0) {
            m.submesh_index = entt::id_type(sub_id);
        }
        if (ImGui::Button("Unset Submesh/Material ID")) {
            m.submesh_index = entt::null;
        }
    }

    if (has_accelerated_picking(r, m.entity)) {
        ImGui::Text("Accelerated picking: Yes");
    } else {
        ImGui::Text("Accelerated picking: No");
    }
}


void show_transform(Registry* rptr, Entity e)
{
    auto& t = rptr->get<Transform>(e);
    ImGui::Text("Local:");
    ImGui::Separator();


    {
        ImGui::PushID("Local");
        auto tcol = t.local.matrix().col(3).eval();
        if (ImGui::DragFloat3("Position", tcol.data(), 0.01f)) {
            t.local.matrix().col(3) = tcol;
        }


        const auto r0 = t.local.rotation().eulerAngles(0, 1, 2).eval();
        auto r1 = r0;
        if (ImGui::DragFloat3("Rotation", r1.data(), 0.01f)) {
            const auto d = (r1 - r0).eval();
            if (d(0) != 0.0f) {
                t.local = Eigen::AngleAxisf(d(0), Eigen::Vector3f::UnitX()) * t.local;
            }
            if (d(1) != 0.0f) {
                t.local = Eigen::AngleAxisf(d(1), Eigen::Vector3f::UnitY()) * t.local;
            }
            if (d(2) != 0.0f) {
                t.local = Eigen::AngleAxisf(d(2), Eigen::Vector3f::UnitZ()) * t.local;
            }
        }
        ImGui::PopID();
    }


    ImGui::Text("Global:");
    ImGui::Separator();
    {
        ImGui::PushID("Global");
        auto tcol = t.global.matrix().col(3).eval();
        if (ImGui::DragFloat3("Position", tcol.data(), 0.01f)) {
            lagrange::logger().warn("Global transform UI change not implemented yet");
        }

        const auto r0 = t.global.rotation().eulerAngles(0, 1, 2).eval();
        auto r1 = r0;
        if (ImGui::DragFloat3("Rotation", r1.data(), 0.01f)) {
            lagrange::logger().warn("Global transform UI change not implemented yet");
        }
        ImGui::PopID();
    }

    ImGui::Text("Viewport:");

    ImGui::DragFloat2("Scale", t.viewport.scale.data(), 0.01f);
    ImGui::DragFloat2("Translation", t.viewport.translate.data(), 0.01f);
    ImGui::Checkbox("Clip", &t.viewport.clip);
}


void show_name(Registry* rptr, Entity e)
{
    auto& name = rptr->get<Name>(e);
    ImGui::InputText("Name", &name);
}

void show_camera_controller(Registry* rptr, Entity e)
{
    auto& cc = rptr->get<CameraController>(e);
    ImGui::Checkbox("Ortho Interaction 2D", &cc.ortho_interaction_2D);
    ImGui::Checkbox("Auto near/far", &cc.auto_nearfar);
    ImGui::Checkbox("Fov Zoom", &cc.fov_zoom);
}

void show_camera_turntable(Registry* rptr, Entity e)
{
    auto& turntable = rptr->get<CameraTurntable>(e);

    if (!rptr->all_of<Camera>(e)) {
        ImGui::Text("Warning: this entity does not have Camera component");
        return;
    }

    if (ImGui::Checkbox("Enabled", &turntable.enabled)) {
        if (turntable.enabled) turntable.start_pos = rptr->get<Camera>(e).get_position();
    }

    ImGui::DragFloat("Speed", &turntable.speed, 0.01f, 0.0f, 10.0f);

    if (ImGui::DragFloat3("Axis", turntable.axis.data(), 0.01f))
        turntable.axis = turntable.axis.normalized();


    if (ImGui::Button("Remove", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        rptr->remove<CameraTurntable>(e);
    }
}

void show_camera(Registry* rptr, Entity e)
{
    auto& cam = rptr->get<Camera>(e);
    bool changed = false;

    if (ImGui::Button("Reset Camera", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        cam = Camera::default_camera(cam.get_window_width(), cam.get_window_height());
        changed = true;
    }

    if (ImGui::Button("Add Turntable", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        rptr->emplace_or_replace<CameraTurntable>(e);
    }

    ImGui::Separator();

    ImGui::Text("%s", "Rotation Type");
    auto rotation_mode = cam.get_rotation_mode();
    if (ImGui::RadioButton(
            "Tumble (Blender/Maya-like)",
            rotation_mode == Camera::RotationMode::TUMBLE)) {
        cam.set_rotation_mode(Camera::RotationMode::TUMBLE);
        changed = true;
    }

    if (ImGui::RadioButton("Turntable", rotation_mode == Camera::RotationMode::TURNTABLE)) {
        cam.set_rotation_mode(Camera::RotationMode::TURNTABLE);
        changed = true;
    }

    if (ImGui::RadioButton(
            "Arcball (Meshlab-like)",
            rotation_mode == Camera::RotationMode::ARCBALL)) {
        cam.set_rotation_mode(Camera::RotationMode::ARCBALL);
        changed = true;
    }

    ImGui::Separator();

    Eigen::Vector3f pos = cam.get_position();
    Eigen::Vector3f lookat = cam.get_lookat();
    Eigen::Vector3f up = cam.get_up();

    Eigen::Vector3f side = cam.get_up().cross(cam.get_direction()).normalized();

    if (ImGui::DragFloat3("Pos", pos.data(), 0.01f)) {
        cam.set_position(pos);
        changed = true;
    }
    if (ImGui::DragFloat3("Lookat", lookat.data(), 0.01f)) {
        cam.set_lookat(lookat);
        changed = true;
    }
    if (ImGui::DragFloat3("Up", up.data(), 0.01f)) {
        cam.set_up(up);
        changed = true;
    }
    ImGui::DragFloat3("Side/Right", side.data(), 0.01f);


    Eigen::Vector3f look_vec_n = (cam.get_position() - cam.get_lookat()).normalized();
    float yaw = lagrange::to_degrees(std::atan2(look_vec_n.z(), look_vec_n.x())); // azimuth
    float pitch = lagrange::to_degrees(std::acos(look_vec_n.y())); // inclination

    ImGui::DragFloat("Yaw/Azimuth", &yaw, 0.01f, 0, 0, "%.3f degrees");
    ImGui::DragFloat("Pitch/Inclination", &pitch, 0.01f, 0, 0, "%.3f degrees");


    ImGui::Separator();
    ImGui::Text(
        "Type: %s",
        cam.get_type() == Camera::Type::PERSPECTIVE ? "Perspective" : "Orthographic");
    auto ortho_viewport = cam.get_ortho_viewport();
    if (ImGui::DragFloat4("Orthographic viewport", ortho_viewport.data(), 0.01f)) {
        cam.set_ortho_viewport(ortho_viewport);
        changed = true;
    }
    ImGui::Separator();


    ImGui::Text("Projection matrix");
    float fov = lagrange::to_degrees(cam.get_fov());

    if (ImGui::DragFloat("fov degrees", &fov, 0.1f)) {
        cam.set_fov(lagrange::to_radians(fov));
        changed = true;
    }
    float near_plane = cam.get_near();
    float far_plane = cam.get_far();
    if (ImGui::DragFloat("near plane", &near_plane) || ImGui::DragFloat("far plane", &far_plane)) {
        cam.set_planes(near_plane, far_plane);
        changed = true;
    }

    float width = cam.get_window_width();
    float height = cam.get_window_height();
    if (ImGui::DragFloat("width", &width) || ImGui::DragFloat("height", &height)) {
        cam.set_window_dimensions(std::floor(width), std::floor(height));
        changed = true;
    }

    if (changed) {
        ui::publish<CameraChangedEvent>(*rptr, e);
    }
}


bool show_colormap_popup(Registry& registry, Material& mat, const ui::StringID& property_id)
{
    struct ColormapCache
    {
        std::shared_ptr<Texture> viridis = ui::generate_colormap(ui::colormap_viridis);
        std::shared_ptr<Texture> magma = ui::generate_colormap(ui::colormap_magma);
        std::shared_ptr<Texture> plasma = ui::generate_colormap(ui::colormap_plasma);
        std::shared_ptr<Texture> inferno = ui::generate_colormap(ui::colormap_inferno);
        std::shared_ptr<Texture> turbo = ui::generate_colormap(ui::colormap_turbo);
        std::shared_ptr<Texture> coolwarm = ui::generate_colormap(ui::colormap_coolwarm);
    };

    const auto& cc = registry.ctx_or_set<ColormapCache>();

    auto w = 250;
    auto h = 25;

    if (UIWidget("viridis")(*cc.viridis, w, h)) {
        mat.set_texture(property_id, cc.viridis);
        return true;
    }
    ImGui::SameLine();
    ImGui::Text("Viridis");

    if (UIWidget("magma")(*cc.magma, w, h)) {
        mat.set_texture(property_id, cc.magma);
        return true;
    }
    ImGui::SameLine();
    ImGui::Text("Magma");

    if (UIWidget("plasma")(*cc.plasma, w, h)) {
        mat.set_texture(property_id, cc.plasma);
        return true;
    }
    ImGui::SameLine();
    ImGui::Text("Plasma");

    if (UIWidget("inferno")(*cc.inferno, w, h)) {
        mat.set_texture(property_id, cc.inferno);
        return true;
    }
    ImGui::SameLine();
    ImGui::Text("Inferno");

    if (UIWidget("turbo")(*cc.turbo, w, h)) {
        mat.set_texture(property_id, cc.turbo);
        return true;
    }
    ImGui::SameLine();
    ImGui::Text("Turbo");

    if (UIWidget("coolwarm")(*cc.coolwarm, w, h)) {
        mat.set_texture(property_id, cc.coolwarm);
        return true;
    }
    ImGui::SameLine();
    ImGui::Text("Coolwarm");

    return false;
}

void show_material(Registry* rptr, Entity e, Material& mat)
{
    std::string shader_btn_text = "Shader: ";

    const auto& reg_shaders = ui::get_registered_shaders(*rptr);
    const auto reg_it = reg_shaders.find(mat.shader_id());
    if (reg_it == reg_shaders.end()) {
        shader_btn_text = "Not found";
        return;
    }

    bool changed = false;


    shader_btn_text = reg_it->second.display_name;

    if (ImGui::Button(shader_btn_text.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 32))) {
        ImGui::OpenPopup("ChooseShader");
    }

    if (ImGui::BeginPopup("ChooseShader")) {
        for (auto& it : reg_shaders) {
            if (ImGui::Button(it.second.display_name.c_str(), ImVec2(256, 32))) {
                mat = Material(*rptr, it.first);
                changed = true;
            }
        }

        ImGui::EndPopup();
    }


    auto shader = ui::get_shader(*rptr, mat.shader_id());
    if (!shader) {
        return;
    }

    ImGui::Separator();


    auto glenum_str = [](GLenum v) -> std::string {
        switch (v) {
        case GL_FLOAT_VEC2: return "vec2";
        case GL_FLOAT_VEC3: return "vec3";
        case GL_FLOAT_VEC4: return "vec4";
        case GL_SAMPLER_1D: return "sampler1D";
        case GL_SAMPLER_2D: return "sampler2D";
        case GL_SAMPLER_3D: return "sampler3D";
        case GL_SAMPLER_CUBE: return "samplerCube";
        case GL_FLOAT_MAT2: return "mat2";
        case GL_FLOAT_MAT3: return "mat3";
        case GL_FLOAT_MAT4: return "mat4";
        case GL_INT: return "int";
        case GL_BOOL: return "bool";
        case GL_SHORT: return "short";
        case GL_FLOAT: return "float";
        case GL_DOUBLE: return "double";
        default: break;
        }
        return lagrange::string_format("{:X}{}", int(v), int(v));
    };


    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ContextMenuInBody;

    ImGui::Indent();

    static int tex_size = 64;


    if (ImGui::BeginTable(
            "Props",
            2,
            /*ImGuiTableFlags_Resizable |*/ ImGuiTableFlags_BordersInnerH |
                ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable |
                ImGuiTableFlags_Hideable)) {
        ImGui::TableSetupColumn("First", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Second", ImGuiTableColumnFlags_WidthStretch);
        // Texture maps values
        {
            auto& values = mat.texture_values;


            for (auto& it : values) {
                const auto& id = it.first;

                auto& val = it.second;

                auto has_property =
                    shader->texture_properties().find(id) != shader->texture_properties().end();

                ImGui::TableNextRow();
                ImGui::PushID(it.first);


                ImGui::TableSetColumnIndex(0);

                bool browse_clicked = browse_texture_widget(val.texture, tex_size);

                ImGui::TableSetColumnIndex(1);

                if (has_property) {
                    const auto& prop = shader->texture_properties().at(id);

                    ImGui::Text("%s", prop.display_name.c_str());

                    if (prop.colormap) {
                        if (ImGui::BeginPopup("###Choose Colormap")) {
                            if (show_colormap_popup(*rptr, mat, it.first))
                                ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                        }

                        if (ImGui::Button("Choose Colormap " ICON_FA_CARET_DOWN)) {
                            ImGui::OpenPopup("###Choose Colormap");
                        }
                        ImGui::SameLine();
                    }

                    if (val.texture) {
                        if (ImGui::Button("Clear texture")) {
                            val.texture = nullptr;
                            changed = true;
                        }
                    } else {
                        if (prop.value_dimension == 1) {
                            ImGui::SliderFloat("##value", val.color.data(), 0, 1);
                            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
                        } else if (prop.value_dimension == 2) {
                            UIWidget("##value")(reinterpret_cast<Eigen::Vector2f&>(val.color));
                        } else if (prop.value_dimension == 3) {
                            Color c = Color(val.color.x(), val.color.y(), val.color.z(), 1.0f);
                            if (UIWidget("##value")(c)) {
                                val.color = c;
                            }
                        } else if (prop.value_dimension == 4) {
                            Color c = val.color;
                            if (UIWidget("##value")(c)) {
                                val.color = c;
                                changed = true;
                            }
                        }
                    }

                    if (browse_clicked) {
                        Texture::Params p;
                        // todo srgb for base color?

                        // If shader specifies value dimension, override texture upload defaults
                        if (prop.value_dimension == 1) {
                            p.format = GL_RED;
                        } else if (prop.value_dimension == 2) {
                            p.format = GL_RG;
                        } else if (prop.value_dimension == 3) {
                            p.format = GL_RGB;
                        } else if (prop.value_dimension == 4) {
                            p.format = GL_RGBA;
                        }
                        auto new_tex = load_texture_dialog(p);
                        if (new_tex) {
                            val.texture = new_tex;
                            changed = true;
                        }
                    }


                    if (prop.transformable) {
                        ImGui::Text("Transform:");
                        UIWidget("rotation")(val.transform.rotation);
                        UIWidget("offset")(val.transform.offset);
                        UIWidget("scale")(val.transform.scale);
                    }
                } else {
                    ImGui::Text("%s", "Unknown texture property");
                    if (val.texture) {
                        if (ImGui::Button("Clear texture")) {
                            val.texture = nullptr;
                        }
                    }
                }

                ImGui::PopID();
            }
        }

        // Float values
        {
            auto& values = mat.float_values;
            for (auto& it : values) {
                ImGui::TableNextRow();
                ImGui::PushID(it.first);

                ImGui::TableSetColumnIndex(0);
                ImGui::InvisibleButton("##_", ImVec2(1, float(tex_size))); // padding

                ImGui::TableSetColumnIndex(1);


                const auto& ID = it.first;
                auto& val = it.second;
                auto prop_it = shader->float_properties().find(ID);

                if (prop_it != shader->float_properties().end()) {
                    const auto& prop = prop_it->second;

                    ImGui::Text("%s", prop.display_name.c_str());
                    if (prop.min_value != std::numeric_limits<float>::lowest() &&
                        prop.min_value != std::numeric_limits<float>::max()) {
                        ImGui::SliderFloat("##value", &val, prop.min_value, prop.max_value);
                        if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
                    } else {
                        if (ImGui::InputFloat("##value", &val)) changed = true;
                    }
                } else {
                    ImGui::Text("Unknown float uniform %d", ID);
                    ImGui::InputFloat("##value", &val);
                }


                ImGui::PopID();
            }
        }

        // Int values
        {
            auto& values = mat.int_values;
            for (auto& it : values) {
                ImGui::TableNextRow();
                ImGui::PushID(it.first);

                ImGui::TableSetColumnIndex(0);
                ImGui::InvisibleButton("##_", ImVec2(1, float(tex_size))); // padding

                ImGui::TableSetColumnIndex(1);


                const auto& ID = it.first;
                auto& val = it.second;
                auto prop_it = shader->int_properties().find(ID);

                if (prop_it != shader->int_properties().end()) {
                    const auto& prop = prop_it->second;

                    ImGui::Text("%s", prop.display_name.c_str());
                    if (prop.min_value != std::numeric_limits<int>::lowest() &&
                        prop.min_value != std::numeric_limits<int>::max()) {
                        ImGui::SliderInt("##value", &val, prop.min_value, prop.max_value);
                        if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
                    } else {
                        if (ImGui::InputInt("##value", &val)) changed = true;
                    }
                } else {
                    ImGui::Text("Unknown int uniform %d", ID);
                    ImGui::InputInt("##value", &val);
                }
                ImGui::PopID();
            }
        }

        // Bool values
        {
            auto& values = mat.bool_values;
            for (auto& it : values) {
                ImGui::TableNextRow();
                ImGui::PushID(it.first);

                ImGui::TableSetColumnIndex(0);
                ImGui::InvisibleButton("##_", ImVec2(1, float(tex_size))); // padding

                ImGui::TableSetColumnIndex(1);

                const auto& ID = it.first;
                auto& val = it.second;
                auto prop_it = shader->bool_properties().find(ID);

                if (prop_it != shader->bool_properties().end()) {
                    const auto& prop = prop_it->second;

                    ImGui::Text("%s", prop.display_name.c_str());
                    changed = ImGui::Checkbox("##value", &val);
                } else {
                    ImGui::Text("Unknown bool uniform %d", ID);
                    changed = ImGui::Checkbox("##value", &val);
                }
                ImGui::PopID();
            }
        }


        // Color values
        {
            auto& values = mat.color_values;
            for (auto& it : values) {
                ImGui::TableNextRow();
                ImGui::PushID(it.first);

                ImGui::TableSetColumnIndex(0);
                ImGui::InvisibleButton("##_", ImVec2(1, float(tex_size))); // padding

                ImGui::TableSetColumnIndex(1);

                auto prop_it = shader->color_properties().find(it.first);
                const std::string display_name = (prop_it == shader->color_properties().end())
                                                     ? std::string("Unknown color uniform")
                                                     : prop_it->second.display_name;

                UIWidget widget(display_name);
                widget(it.second);
                ImGui::PopID();
            }
        }

        // Vector values
        {
            auto& values = mat.vec4_values;
            for (auto& it : values) {
                ImGui::TableNextRow();
                ImGui::PushID(it.first);

                ImGui::TableSetColumnIndex(0);
                ImGui::InvisibleButton("##_", ImVec2(1, float(tex_size))); // padding

                ImGui::TableSetColumnIndex(1);

                auto prop_it = shader->vector_properties().find(it.first);
                const std::string display_name = (prop_it == shader->vector_properties().end())
                                                     ? std::string("Unknown vector uniform")
                                                     : prop_it->second.display_name;

                UIWidget widget(display_name);
                widget(it.second);
                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }


    if (shader && ImGui::CollapsingHeader("Shader Uniforms")) {
        auto& uniforms = shader->uniforms();
        if (ImGui::BeginTable("Shader Uniforms", 4, flags)) {
            ImGui::TableSetupColumn("Location");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Size");
            ImGui::TableHeadersRow();

            for (auto& it : uniforms) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", it.second.location);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", shader->name(it.first).c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", glenum_str(it.second.type).c_str());

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", it.second.size);
            }
            ImGui::EndTable();
        }
    }

    if (shader && ImGui::CollapsingHeader("Shader Attributes")) {
        auto& uniforms = shader->attribs();

        if (ImGui::BeginTable("Shader Attributes", 4, flags)) {
            ImGui::TableSetupColumn("Location");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Size");
            ImGui::TableHeadersRow();

            for (auto& it : uniforms) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", it.second.location);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", shader->name(it.first).c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", glenum_str(it.second.type).c_str());

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", it.second.size);
            }
            ImGui::EndTable();
        }
    }

    if (shader && ImGui::CollapsingHeader("Shader Properties (Defaults)")) {
        ImGui::Text("Textures");
        if (ImGui::BeginTable("Textures", 6, flags)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("texture");
            ImGui::TableSetupColumn("value");
            ImGui::TableSetupColumn("value_dim");
            ImGui::TableSetupColumn("sampler_type");
            ImGui::TableSetupColumn("normal");
            ImGui::TableHeadersRow();

            const auto& props = shader->texture_properties();

            for (const auto& it : props) {
                ImGui::PushID(it.first);

                const auto& prop = it.second;
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", it.second.display_name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("nullptr");

                ImGui::TableSetColumnIndex(2);
                auto local_value = prop.default_value;
                if (prop.value_dimension == 1) {
                    ImGui::InputFloat("#", local_value.color.data());
                } else if (prop.value_dimension == 2) {
                    ImGui::InputFloat2("#", local_value.color.data());
                } else if (prop.value_dimension == 3) {
                    ImGui::InputFloat3("#", local_value.color.data());
                } else if (prop.value_dimension == 4) {
                    ImGui::InputFloat4("#", local_value.color.data());
                }

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", prop.value_dimension);

                ImGui::TableSetColumnIndex(4);

                switch (prop.sampler_type) {
                case GL_SAMPLER_1D: ImGui::Text("GL_SAMPLER_1D"); break;
                case GL_SAMPLER_2D: ImGui::Text("GL_SAMPLER_2D"); break;
                case GL_SAMPLER_3D: ImGui::Text("GL_SAMPLER_3D"); break;
                case GL_SAMPLER_CUBE: ImGui::Text("GL_SAMPLER_CUBE"); break;
                }

                ImGui::TableSetColumnIndex(5);
                bool normal = prop.normal;
                ImGui::Checkbox("##normal", &normal);

                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::Text("Floats");
        if (ImGui::BeginTable("Floats", 4, flags)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("value");
            ImGui::TableSetupColumn("min_val");
            ImGui::TableSetupColumn("max_val");
            ImGui::TableHeadersRow();

            const auto& props = shader->float_properties();

            for (const auto& it : props) {
                ImGui::PushID(it.first);

                const auto& prop = it.second;
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", it.second.display_name.c_str());

                auto local_prop = prop;
                ImGui::TableSetColumnIndex(1);
                ImGui::InputFloat("##value", &local_prop.default_value);
                ImGui::TableSetColumnIndex(2);
                ImGui::InputFloat("##min", &local_prop.min_value);
                ImGui::TableSetColumnIndex(3);
                ImGui::InputFloat("##max", &local_prop.max_value);

                ImGui::PopID();
            }
            ImGui::EndTable();
        }


        ImGui::Text("Colors");
        if (ImGui::BeginTable("Colors", 2, flags)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("value");
            ImGui::TableHeadersRow();

            const auto& props = shader->color_properties();

            for (const auto& it : props) {
                ImGui::PushID(it.first);

                const auto& prop = it.second;
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);

                auto name_it = shader->names().find(it.first);
                if (name_it == shader->names().end()) {
                    ImGui::Text("Unused");
                } else {
                    ImGui::Text("%s", name_it->second.c_str());
                }


                ImGui::TableSetColumnIndex(1);
                auto col = prop.default_value;
                UIWidget()(col);

                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::Text("Vectors");
        if (ImGui::BeginTable("Vectors", 2, flags)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("value");
            ImGui::TableHeadersRow();

            const auto& props = shader->vector_properties();

            for (const auto& it : props) {
                ImGui::PushID(it.first);

                const auto& prop = it.second;
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", it.second.display_name.c_str());

                ImGui::TableSetColumnIndex(1);
                auto col = prop.default_value;
                UIWidget()(col);

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }


    if (shader && ImGui::CollapsingHeader("Rasterizer Options")) {
        auto option_bool = [](Material& mat, const StringID& id, const std::string& name) {
            ImGui::PushID(id);
            ImGui::Text("%s", name.c_str());
            ImGui::SameLine();
            if (mat.int_values.count(id)) {
                auto& val = mat.int_values[id];
                bool toggle = val == GL_TRUE;
                if (ImGui::Checkbox("##", &toggle)) {
                    if (toggle)
                        val = GL_TRUE;
                    else
                        val = GL_FALSE;
                }

            } else {
                if (ImGui::SmallButton("Set")) {
                    mat.int_values[id] = GL_TRUE;
                }
            }
            ImGui::PopID();
        };


        {
            ImGui::PushID(RasterizerOptions::PolygonMode);
            ImGui::Text("Polygon Mode");
            ImGui::SameLine();
            if (mat.int_values.count(RasterizerOptions::PolygonMode)) {
                auto& val = mat.int_values[RasterizerOptions::PolygonMode];

                if (ImGui::RadioButton("GL_FILL", val == GL_FILL)) {
                    val = GL_FILL;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("GL_LINE", val == GL_LINE)) {
                    val = GL_LINE;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("GL_POINT", val == GL_POINT)) {
                    val = GL_POINT;
                }
            } else {
                if (ImGui::SmallButton("Set")) {
                    mat.int_values[RasterizerOptions::PolygonMode] = GL_FILL;
                }
            }
            ImGui::PopID();
        }

        {
            ImGui::PushID(RasterizerOptions::Pass);
            ImGui::Text("Pass / Depth Offset Layer");
            ImGui::SameLine();
            if (mat.int_values.count(RasterizerOptions::Pass)) {
                auto& val = mat.int_values[RasterizerOptions::Pass];
                ImGui::InputInt("##pass", &val);
            } else {
                if (ImGui::SmallButton("Set")) {
                    mat.set_int(RasterizerOptions::Pass, 0);
                }
            }
            ImGui::PopID();
        }


        {
            ImGui::PushID(RasterizerOptions::PointSize);
            ImGui::Text("Point Size");
            ImGui::SameLine();
            if (mat.int_values.count(RasterizerOptions::PointSize)) {
                auto& val = mat.int_values[RasterizerOptions::PointSize];
                ImGui::InputInt("##pointsize", &val);
            } else {
                if (ImGui::SmallButton("Set")) {
                    mat.int_values[RasterizerOptions::PointSize] = 1;
                }
            }
            ImGui::PopID();
        }

        option_bool(mat, RasterizerOptions::CullFaceEnabled, "Cull Face Enabled");
        option_bool(mat, RasterizerOptions::ScissorTest, "Scissor Test");
        option_bool(mat, RasterizerOptions::DepthTest, "Depth Test");
    }


    ImGui::Unindent();

    if (changed) {
        ui::publish<MeshRenderChangedEvent>(*rptr, e);
    }
}


void show_meshrender(Registry* rptr, Entity e)
{
    auto& mr = rptr->get<MeshRender>(e);


    if (mr.material) {
        show_material(rptr, e, *mr.material);
    } else {
        ImGui::Text("No Material");
    }

    ImGui::Separator();

    ImGui::Indent();
    if (ImGui::CollapsingHeader("Primitive")) {
        ImGui::Text("Raster primitive:");
        if (ImGui::RadioButton("Triangles", mr.primitive == PrimitiveType::TRIANGLES)) {
            mr.primitive = PrimitiveType::TRIANGLES;
        }
        if (ImGui::RadioButton("Lines", mr.primitive == PrimitiveType::LINES)) {
            mr.primitive = PrimitiveType::LINES;
        }
        if (ImGui::RadioButton("Points", mr.primitive == PrimitiveType::POINTS)) {
            mr.primitive = PrimitiveType::POINTS;
        }
    }
    ImGui::Unindent();
}


void show_ibl(Registry* rptr, Entity e)
{
    auto& ibl = rptr->get<IBL>(e);

    IBLChangedEvent ev;
    ev.entity = e;

    if (browse_texture_widget(ibl.background_rect, int(ImGui::GetContentRegionAvail().x) - 20)) {
        Texture::Params psrgb;
        psrgb.sRGB = false;
        auto t_new = load_texture_dialog(psrgb);
        if (t_new) {
            ibl = generate_ibl(t_new);
            ev.image_changed = true;
        }
    }


    if (ImGui::Button("Save ...")) {
        auto path = ui::open_folder("Save IBL to folder");
        if (!path.empty()) {
            ui::save_ibl(ibl, path);
        }
    }


    ImGui::Text("Background cube map: ");
    ImGui::SameLine();
    if (ibl.background) {
        ImGui::Text("6x%dx%d", ibl.background->get_width(), ibl.background->get_height());
    } else {
        ImGui::Text("None");
    }

    ImGui::Text("Diffuse cube map: ");
    ImGui::SameLine();
    if (ibl.diffuse) {
        ImGui::Text("6x%dx%d", ibl.diffuse->get_width(), ibl.diffuse->get_height());
    } else {
        ImGui::Text("None");
    }

    ImGui::Text("Specular cube map: ");
    ImGui::SameLine();
    if (ibl.specular) {
        ImGui::Text("6x%dx%d", ibl.specular->get_width(), ibl.specular->get_height());
    } else {
        ImGui::Text("None");
    }

    ev.parameters_changed |= ImGui::SliderFloat("Blur (mip level)", &ibl.blur, 0.0f, 16.0f);

    if (ev.parameters_changed || ev.image_changed) {
        ui::publish<IBLChangedEvent>(*rptr, ev);
    }
}


void show_light(Registry* rptr, Entity e)
{
    auto& lc = rptr->get<LightComponent>(e);

    bool changed = false;

    ImGui::Text("Type:");

    ImGui::SameLine();
    if (button_icon(lc.type == LightComponent::Type::POINT, "Point", "Point Light")) {
        lc.type = LightComponent::Type::POINT;
    }
    ImGui::SameLine();
    if (button_icon(
            lc.type == LightComponent::Type::DIRECTIONAL,
            "Directional",
            "Directional Light")) {
        lc.type = LightComponent::Type::DIRECTIONAL;
    }
    ImGui::SameLine();
    if (button_icon(lc.type == LightComponent::Type::SPOT, "Spot", "Spot Light")) {
        lc.type = LightComponent::Type::SPOT;
    }

    float intensity = lc.intensity.norm();
    Color color = Color(lc.intensity / intensity, 1.0f);

    if (UIWidget("Color")(color)) {
        const auto new_color = Eigen::Vector3f(color.x(), color.y(), color.z());
        lc.intensity = intensity * new_color * (1.0f / new_color.norm());
        changed = true;
    }

    if (UIWidget("Absolute intensity")(intensity)) {
        lc.intensity = intensity * Eigen::Vector3f(color.x(), color.y(), color.z());
        changed = true;
    }

    if (UIWidget("RGB Intensity")(lc.intensity)) {
        changed = true;
    };


    ImGui::Checkbox("Casts shadow", &lc.casts_shadow);

    if (lc.type == LightComponent::Type::SPOT) {
        ImGui::SliderAngle("Cone angle", &lc.cone_angle, 0.0f, 180.0f);
        if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
    }

    if (changed) {
        ui::publish<LightComponentChangedEvent>(*rptr, e);
    }
}


void show_layer(Registry* rptr, Entity e)
{
    auto& layer = rptr->get<Layer>(e);


    for (size_t i = 0; i < get_max_layers(); i++) {
        const auto& name = get_layer_name(*rptr, LayerIndex(i));
        if (name.length() == 0) continue;
        bool value = layer.test(i);
        if (ImGui::Checkbox(name.c_str(), &value)) {
            layer.flip(i);
        }
    }
}


void show_bounds(Registry* rptr, Entity e)
{
    const auto& bounds = rptr->get<Bounds>(e);

    const auto show_bb = [](const AABB& bb) {
        if (bb.isEmpty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::Spectrum::RED400);
            ImGui::Text("Empty");
            ImGui::PopStyleColor();
        } else {
            Eigen::Vector3f tmp_min = bb.min(), tmp_max = bb.max(), tmp_extent = bb.diagonal(),
                            tmp_center = bb.center();
            ImGui::DragFloat3("Min", tmp_min.data());
            ImGui::DragFloat3("Max", tmp_max.data());
            ImGui::DragFloat3("Extent", tmp_extent.data());
            ImGui::DragFloat3("Center", tmp_center.data());
        }
    };

    ImGui::Text("Local");
    show_bb(bounds.local);

    ImGui::Separator();

    ImGui::Text("Global");
    show_bb(bounds.global);

    ImGui::Separator();

    ImGui::Text("BVH Node");
    show_bb(bounds.bvh_node);
}


void show_tree_node(Registry* rptr, Entity e)
{
    auto& r = *rptr;
    const auto& t = r.get<TreeNode>(e);

    // parent: 14 "parent name" >

    const auto row = [&](const char* label, Entity e, const char* icon) {
        ImGui::Text("%s", label);
        ImGui::NextColumn();

        ImGui::Text("%d", int(e));
        ImGui::NextColumn();

        ImGui::Text("%s", ui::get_name(r, e).c_str());
        ImGui::NextColumn();

        if (ImGui::SmallButton(icon)) {
            ui::select(r, e);
        }
        ImGui::NextColumn();
    };

    const auto empty_row = [](const char* label) {
        ImGui::Text("%s", label);
        ImGui::NextColumn();
        ImGui::Text("None");
        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::NextColumn();
    };

    ImGui::Text("Num Children: %d", int(t.num_children));

    ImGui::Columns(4);


    if (t.parent != NullEntity) {
        row("Parent", t.parent, ICON_FA_CHEVRON_UP);
    } else {
        empty_row("Parent");
    }

    if (t.first_child != NullEntity) {
        row("First Child", t.first_child, ICON_FA_CHEVRON_DOWN);
    } else {
        empty_row("First Child");
    }

    if (t.prev_sibling != NullEntity) {
        row("Previous Sibling", t.prev_sibling, ICON_FA_CHEVRON_LEFT);
    } else {
        empty_row("Previous Sibling");
    }

    if (t.next_sibling != NullEntity) {
        row("Next Sibling", t.next_sibling, ICON_FA_CHEVRON_RIGHT);
    } else {
        empty_row("Next Sibling");
    }


    ImGui::Columns(1);
    // ImGui::Text("Parent");
}

} // namespace


void register_default_component_widgets()
{
    register_component<MeshGeometry>("MeshGeometry");
    register_component_widget<MeshGeometry, &show_mesh_geometry>();

    register_component<Transform>("Transform");
    register_component_widget<Transform, &show_transform>();

    register_component<Name>("Name");
    register_component_widget<Name, &show_name>();


    register_component<CameraController>("CameraController");
    register_component_widget<CameraController, &show_camera_controller>();

    register_component<CameraTurntable>("CameraTurntable");
    register_component_widget<CameraTurntable, &show_camera_turntable>();

    register_component<Camera>("Camera");
    register_component_widget<Camera, &show_camera>();


    register_component<MeshRender>("MeshRender");
    register_component_widget<MeshRender, &show_meshrender>();

    register_component<IBL>("IBL");
    register_component_widget<IBL, &show_ibl>();

    register_component<LightComponent>("Light");
    register_component_widget<LightComponent, &show_light>();


    register_component<Layer>("Layer");
    register_component_widget<Layer, &show_layer>();

    register_component<Bounds>("Bounds");
    register_component_widget<Bounds, &show_bounds>();

    register_component<TreeNode>("TreeNode");
    register_component_widget<TreeNode, &show_tree_node>();
}

} // namespace ui
} // namespace lagrange
