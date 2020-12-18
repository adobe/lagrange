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

#include <lagrange/ui/Callbacks.h>

#include <imgui.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


struct ImGuiWindow;

namespace lagrange {
namespace ui {

class OptionSet;
class Material;
class Texture;
class Viewer;

/*
    Base class for UI Panels
*/
class UIPanelBase : public CallbacksBase<UIPanelBase> {
    friend class Viewer;

public:
    using OnChangeFocus = UI_CALLBACK(void(UIPanelBase&, bool));

    UIPanelBase(Viewer* viewer);
    virtual ~UIPanelBase(){};

    virtual const char* get_title() const { return ""; };

    bool begin(int flags = 0);
    void end();

    virtual void draw_menu(){};
    virtual void draw(){};
    virtual bool draw_toolbar() { return false; } // returns true if it draws anything

    virtual void update(double dt) { m_time_elapsed += dt; }

    bool is_visible() const { return m_visible; }
    void set_visible(bool value) { m_visible = value; }

    bool is_focused() const { return m_focused; }

    Viewer* get_viewer() { return m_viewer; }
    const Viewer* get_viewer() const { return m_viewer; }

    bool is_child() const { return m_is_child; }
    void set_is_child(bool value, int width = 0, int height = 0);

    static void reset_ui_panel_counter();

    enum class DockDir : int {
        AS_NEW_TAB = -1, // Dock as tab
        LEFT = 0,
        RIGHT = 1,
        UP = 2,
        DOWN = 3
    };

    bool dock_to(UIPanelBase& target, UIPanelBase::DockDir dir, float ratio, bool split_outer);
    bool is_docked() const;
    unsigned int get_dock_id() const;

    const ImGuiWindow* get_imgui_window() const { return m_imgui_window; }
    ImGuiWindow* get_imgui_window() { return m_imgui_window; }

    void enable_tab_bar(bool value);

protected:
    Callbacks<OnChangeFocus> m_callbacks;

    int get_panel_id() const;

    bool button_toolbar(bool selected,
        const std::string& label,
        const std::string& tooltip = "",
        const std::string& keybind_id = "",
        bool enabled = true);

    bool button_icon(bool selected,
        const std::string& label,
        const std::string& tooltip = "",
        const std::string& keybind_id = "",
        bool enabled = true, 
        ImVec2 size = ImVec2(0, 0));

private:
    bool m_visible = true;
    Viewer* m_viewer = nullptr; // non-owning
    bool m_focused = false;
    bool m_docked = false;
    unsigned int m_dock_id = 0;
    bool m_is_child = false;

    int m_child_width = 0;
    int m_child_height = 0;

    static int m_ui_panel_id_counter;
    int m_ui_panel_id;

    double m_time_elapsed = 0.0;


    bool m_enable_tab_bar_active = false;
    bool m_enable_tab_bar = true;

    ImGuiWindow* m_imgui_window = nullptr; // non-owning

friend CallbacksBase<UIPanelBase>;
};


/*
    UI Panel class that provides UI interface for
    object T.
    Shared ownership of T
*/
template <typename T>
class UIPanel : public UIPanelBase {
public:
    UIPanel(Viewer* viewer, std::shared_ptr<T> object)
        : UIPanelBase(viewer)
        , m_object(std::move(object))
    {
        assert(viewer != nullptr);
    }

    virtual ~UIPanel() {}

    T& get() { return *m_object; }
    const T& get() const { return *m_object; }

    void has_object() const { return m_object != nullptr; }

    void set(std::shared_ptr<T> object) { m_object = std::move(object); }

protected:
    std::shared_ptr<T> m_object;
};


} // namespace ui
} // namespace lagrange
