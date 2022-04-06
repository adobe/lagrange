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

#include <lagrange/ui/panels/ComponentPanel.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/uipanel.h>
#include <lagrange/utils/strings.h>


#include <imgui.h>
#include <functional>

namespace lagrange {
namespace ui {


struct ComponentPanel
{
    entt::type_info selected_type = entt::type_id<void>();
    bool show_unregistered = false;
};


namespace {


std::string get_pretty_name(const entt::type_info& info)
{
    auto type = entt::resolve(info);
    if (!type) {
        return std::string(info.name());
    }

    using namespace entt::literals;
    auto dname_prop = type.prop("display_name"_hs);
    if (dname_prop) {
        return dname_prop.value().cast<std::string>();
    }

    return std::string(info.name());
}

void component_panel_system(Registry& registry, Entity window_entity)
{
    auto& panel = registry.get<ComponentPanel>(window_entity);

    /*
        Combo box listing all registered components
    */
    if (ImGui::BeginCombo("##Component", get_pretty_name(panel.selected_type).c_str())) {
        for (auto it : registry.storage()) {
            const auto& component_type = it.second.type();
            // Only show registered components here
            auto type = entt::resolve(component_type);
            if (!type) {
                return;
            }

            bool is_selected = (component_type == panel.selected_type);

            if (ImGui::Selectable(get_pretty_name(component_type).c_str(), is_selected)) {
                panel.selected_type = component_type;
                return;
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::SameLine();


    bool add_selected_component = false;
    if (ImGui::Button("Add")) {
        add_selected_component = true;
    }

    ImGui::Checkbox("Show unregistered components", &panel.show_unregistered);


    ImGui::Separator();

    using namespace entt::literals;

    auto view = selected_view(registry);
    for (auto e : view) {
        if (add_selected_component) {
            using namespace entt::literals;

            auto type = entt::resolve(panel.selected_type);
            if (type) {
                auto fn = type.func("component_add_default"_hs);
                if (fn) {
                    fn.invoke({}, &registry, e);
                }
            }
        }


        ImGui::PushID(int(e));

        for (auto it : registry.storage()) {
            if (!it.second.contains(e)) continue;

            const auto& component_type = it.second.type();
            auto type = entt::resolve(component_type);
            if (!type) {
                if (panel.show_unregistered) {
                    ImGui::Text(
                        "%s",
                        lagrange::string_format("{} (Not Registered)", component_type.name())
                            .c_str());
                }

                continue;
            }


            auto dname_prop = type.prop("display_name"_hs);
            if (dname_prop) {
                auto display_name = dname_prop.value().cast<std::string>();
                if (!ImGui::CollapsingHeader(display_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    continue;
            }


            if (ImGui::BeginDragDropSource()) {
                PayloadComponent p;
                p.component_hash = type.info().hash();
                p.entity = e;

                ImGui::SetDragDropPayload(PayloadComponent::id(), &p, sizeof(PayloadComponent));
                ImGui::Text("%s of entity %d", type.info().name().data(), int(e));
                ImGui::EndDragDropSource();
            }

            show_widget(registry, e, type);
        }

        ImGui::PopID();
    }
}

} // namespace


Entity add_component_panel(Registry& r, const std::string& name)
{
    auto e = add_panel(r, name, component_panel_system);
    r.emplace<ComponentPanel>(e);
    return e;
}


} // namespace ui
} // namespace lagrange
