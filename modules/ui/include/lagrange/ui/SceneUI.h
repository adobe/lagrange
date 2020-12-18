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

#include <vector>
#include <string>
#include <memory>
#include <unordered_set>
#include <unordered_map>

#include <lagrange/io/load_mesh_ext.h>

#include <lagrange/ui/Callbacks.h>
#include <lagrange/ui/UIPanel.h>
#include <lagrange/ui/MeshModel.h>
#include <lagrange/ui/Selection.h>


namespace lagrange {
namespace ui {

class Scene;
class Renderer;

/*
    UI Panel for Scene
*/
class SceneUI : public UIPanel<Scene>, public CallbacksBase<SceneUI>{

public:

    using OnMaterialChange = UI_CALLBACK(void(Model&,Material&));

    SceneUI(Viewer* viewer, std::shared_ptr<Scene> scene)
        : UIPanel<Scene>(viewer, scene)
    {
        m_mesh_load_params.normalize = true;
    }

    void draw_menu() final;
    void draw() final;

    const char* get_title() const override { return "Scene"; }

    io::MeshLoaderParams& get_mesh_load_params();

private:
    std::string load_dialog(const std::string& extension = "obj") const;
    std::string save_dialog(const std::string& extension = "obj") const;

    void load_mesh_dialog(bool clear);
    bool save_mesh_dialog();
    void load_ibl_dialog(bool clear);

    void draw_models();
    void draw_materials();
    void draw_emitters();

    void draw_selectable(
        BaseObject* obj,
        const std::string& label = "N/A",
        const ImVec2& size = ImVec2(0, 0));


    io::MeshLoaderParams m_mesh_load_params;
    bool m_triangulate = false;

    Callbacks<
        OnMaterialChange
    > m_callbacks;

    std::string m_modal_load_path;
    bool m_modal_load_clear;

    std::string m_modal_save_path;
    std::vector<const Model*> m_modal_save_models;

    std::string m_modal_to_open = "";

    bool m_group_by_instance = true;

    bool m_selectable_add = false;
    bool m_selectable_erase = false;

friend CallbacksBase<SceneUI>;
};

}
}
