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


#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/components/TreeNode.h>
#include <lagrange/ui/imgui/buttons.h>
#include <lagrange/ui/panels/ScenePanel.h>
#include <lagrange/ui/utils/input.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/treenode.h>
#include <lagrange/ui/utils/uipanel.h>

// TODO remove after icon registration is done
#include <lagrange/ui/components/CameraComponents.h>
#include <lagrange/ui/components/IBL.h>
#include <lagrange/ui/components/Light.h>
#include <lagrange/ui/components/MeshData.h>
#include <lagrange/ui/components/MeshRender.h>


#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <imgui_internal.h>


namespace lagrange {
namespace ui {


namespace {
SelectionBehavior imgui_selectable_behavior(InputState& input)
{
    auto& keybind_map = input.keybinds->get();
    {
        auto it = keybind_map.find("viewport.selection.select.add");
        if (it != keybind_map.end() && it->second.size() > 0) {
            if (ImGui::IsKeyDown(it->second.front().modifiers.front()))
                return SelectionBehavior::ADD;
        }
    }
    {
        auto it = keybind_map.find("viewport.selection.select.erase");
        if (it != keybind_map.end() && it->second.size() > 0) {
            if (ImGui::IsKeyDown(it->second.front().modifiers.front()))
                return SelectionBehavior::ERASE;
        }
    }
    return SelectionBehavior::SET;
}

void scene_panel_system(Registry& registry, Entity panel_entity)
{
    ScenePanel& scene_panel_data = registry.get<ScenePanel>(panel_entity);

    auto& input = get_input(registry);

    if (scene_panel_data.show_tree_view &&
        ImGui::CollapsingHeader("Tree View", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Todo via registration mechanism
        auto get_icon = [&](Entity e) -> std::string {
            if (registry.all_of<MeshRender>(e)) return ICON_FA_CUBE " ";
            if (registry.all_of<CameraController>(e)) return ICON_FA_CAMERA " ";
            if (registry.all_of<IBL>(e)) return ICON_FA_SUN " ";
            if (registry.all_of<LightComponent>(e)) return ICON_FA_LIGHTBULB " ";

            // Default
            if (registry.all_of<TreeNode>(e)) return ICON_FA_LAYER_GROUP " ";
            return "";
        };


        struct
        {
            bool pending = false;
            Entity child;
            Entity target;
        } pending_reparent;

        struct
        {
            bool pending = false;
            bool recursive = false;
            Entity target;
        } pending_removal;


        ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                        ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                        ImGuiTreeNodeFlags_SpanAvailWidth;

        iterate_inorder(
            registry,
            [&](Entity e) {
                if (scene_panel_data.tree_view_ignored_entities.find(e) !=
                    scene_panel_data.tree_view_ignored_entities.end())
                    return false;

                ImGui::PushID(int(e));

                const std::string name =
                    registry.try_get<Name>(e) ? registry.get<Name>(e) : std::string("NotNamed");

                const std::string nameicon = get_icon(e) + name;

                ImGuiTreeNodeFlags node_flags = base_flags;

                // if node is selected
                const bool selected = ui::is_selected(registry, e);

                // if the node should be closed later
                bool node_open = false;

                // if node is a leaf
                const bool leaf = registry.get<TreeNode>(e).num_children == 0;

                if (selected) {
                    node_flags |= ImGuiTreeNodeFlags_Selected;
                }
                if (leaf) {
                    node_flags |= (ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                }

                const float eyebutton_width = ImGui::GetTextLineHeightWithSpacing();
                {
                    // Temporarily reduce the work area width so that we have available space for
                    // the visibility button later
                    const float workrect_max = ImGui::GetCurrentWindow()->WorkRect.Max.x;
                    ImGui::GetCurrentWindow()->WorkRect.Max.x =
                        (workrect_max - eyebutton_width - ImGui::GetStyle().ItemSpacing.x);

                    node_open =
                        ImGui::TreeNodeEx((void*)(intptr_t)e, node_flags, "%s", nameicon.c_str()) &&
                        !leaf;

                    ImGui::GetCurrentWindow()->WorkRect.Max.x = workrect_max;
                }

                bool node_clicked = ImGui::IsItemClicked();

                if (node_clicked) {
                    ui::set_selected(registry, e, imgui_selectable_behavior(input));
                }

                /*
                    Control visibility in the Default layer
                */
                {
                    ImGui::SameLine(ImGui::GetContentRegionMax().x - eyebutton_width - ImGui::GetStyle().ItemSpacing.x);
                    bool visible = ui::is_in_layer(registry, e, DefaultLayers::Default);
                    if (button_unstyled(visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH)) {
                        if (visible) {
                            ui::remove_from_layer(registry, e, DefaultLayers::Default);
                            ui::foreach_child_recursive(registry, e, [&](Entity child) {
                                ui::remove_from_layer(registry, child, DefaultLayers::Default);
                            });

                        } else {
                            ui::add_to_layer(registry, e, DefaultLayers::Default);
                            ui::foreach_child_recursive(registry, e, [&](Entity child) {
                                ui::add_to_layer(registry, child, DefaultLayers::Default);
                            });
                        }
                    }
                }

                if (ImGui::BeginPopupContextItem(name.c_str())) {
                    ImGui::Text("%s", name.c_str());
                    ImGui::Separator();

                    bool tmp = false;
                    if (ImGui::MenuItem("Group", nullptr, &tmp, true)) {
                        ui::group(registry, ui::collect_selected(registry));
                    }
                    if (ImGui::MenuItem("Ungroup", nullptr, &tmp, true)) {
                        ui::ungroup(registry, e);
                    }
                    if (ImGui::MenuItem("Remove", nullptr, &tmp, true)) {
                        pending_removal.pending = true;
                        pending_removal.recursive = false;
                        pending_removal.target = e;
                    }
                    if (ImGui::MenuItem("Remove Including Children", nullptr, &tmp, true)) {
                        pending_removal.pending = true;
                        pending_removal.recursive = true;
                        pending_removal.target = e;
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("_TREENODE", &e, sizeof(Entity));
                    ImGui::Text("Entity %d", int(e));
                    ImGui::EndDragDropSource();
                }


                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("_TREENODE")) {
                        Entity payload_e = *reinterpret_cast<const Entity*>(payload->Data);

                        pending_reparent.pending = true;
                        pending_reparent.child = payload_e;
                        pending_reparent.target = e;
                    }

                    if (const ImGuiPayload* payload =
                            ImGui::AcceptDragDropPayload(PayloadComponent::id())) {
                        PayloadComponent p =
                            *reinterpret_cast<const PayloadComponent*>(payload->Data);
                        using namespace entt::literals;

                        auto type = entt::resolve(p.component_hash);
                        if (type) {
                            auto clone_fn = type.func("component_clone"_hs);
                            if (clone_fn) {
                                clone_fn.invoke({}, &registry, p.entity, e);
                            }
                        }
                    }

                    ImGui::EndDragDropTarget();
                }


                return node_open;
            },
            [&](Entity e, bool open) {
                if (open) {
                    ImGui::TreePop();
                }

                if (scene_panel_data.tree_view_ignored_entities.find(e) !=
                    scene_panel_data.tree_view_ignored_entities.end())
                    return;

                ImGui::PopID();
            });


        if (pending_reparent.pending) {
            set_parent(registry, pending_reparent.child, pending_reparent.target);
        }

        if (pending_removal.pending) {
            ui::remove(registry, pending_removal.target, pending_removal.recursive);
        }
    }

    if (scene_panel_data.show_mesh_entities && ImGui::CollapsingHeader("Mesh Entities")) {
        auto view = registry.view<MeshData>();
        ImGui::Indent();
        for (auto e : view) {
            if (!ui::is_in_any_layers(registry, e, scene_panel_data.visible_layers)) {
                continue;
            }

            const std::string name =
                registry.try_get<Name>(e) ? registry.get<Name>(e) : std::string("NotNamed");

            bool selected = ui::is_selected(registry, e);
            bool node_clicked = ImGui::Selectable(name.c_str(), &selected);

            if (node_clicked) {
                ui::set_selected(registry, e, imgui_selectable_behavior(input));
            }
        }
        ImGui::Unindent();
    }

    if (scene_panel_data.show_scene_panel_layers && ImGui::CollapsingHeader("Scene Panel Layers")) {
        ImGui::Indent(); // align with the treeview above
        for (size_t i = 0; i < get_max_layers(); i++) {
            const auto& name = get_layer_name(registry, LayerIndex(i));
            if (name.length() == 0) continue;

            bool value = scene_panel_data.visible_layers.test(i);
            if (ImGui::Checkbox(name.c_str(), &value)) {
                scene_panel_data.visible_layers.flip(i);
            }
        }
        ImGui::Unindent();
    }

    if (scene_panel_data.show_all_entities && ImGui::CollapsingHeader("All Entities")) {
        ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable |
                                ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ContextMenuInBody;


        if (ImGui::BeginTable("Entities", 2, flags)) {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableHeadersRow();

            for (size_t i = 0; i < registry.size(); i++) {
                ImGui::TableNextRow();
                auto e = registry.data()[i];

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", int(e));

                ImGui::TableSetColumnIndex(1);

                if (registry.valid(e)) {
                    const auto name = ui::get_name(registry, e);
                    bool selected = ui::is_selected(registry, e);
                    if (ImGui::Selectable(name.c_str(), &selected)) {
                        ui::set_selected(registry, e);
                    }
                } else {
                    ImGui::Text("INVALID");
                }
            }
            ImGui::EndTable();
        }
    }
}

} // namespace

Entity add_scene_panel(Registry& r, const std::string& name /*= "Scene"*/)
{
    auto e = add_panel(r, name, scene_panel_system);
    r.emplace<ScenePanel>(e);
    return e;
}

} // namespace ui
} // namespace lagrange
