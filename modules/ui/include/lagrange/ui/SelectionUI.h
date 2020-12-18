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

#include <lagrange/ui/UIPanel.h>
#include <lagrange/ui/Callbacks.h>
#include <lagrange/ui/Selection.h>
#include <lagrange/ui/BaseObject.h>
#include <lagrange/ui/utils/math.h>

namespace lagrange {
namespace ui {

class Viewer;
class Camera;
class MeshModelBase;

class SelectionUI : public CallbacksBase<Viewer>, public UIPanelBase
{
public:
    using OnSelectionElementTypeChange = UI_CALLBACK(void(SelectionElementType));

    SelectionUI(Viewer* viewer); 

    const char* get_title() const override { return "Selection"; }

    ///
    /// Returns current global selection of objects
    ///
    /// There are two types of selection - persistent and transient (hover)
    ///
    inline TwoStateSelection<BaseObject*>& get_global() { return *m_global_selection; }
    inline const TwoStateSelection<BaseObject*>& get_global() const { return *m_global_selection; }

    void set_selection_mode(SelectionElementType mode);
    inline SelectionElementType get_selection_mode() const { return m_selection_mode; }

    // Has selection changed in this frame?
    inline bool get_selection_changed() const { return m_selection_changed; }

    // Is selection of backfacing elements ignored?
    inline bool select_backfaces() const { return m_select_backfaces; }

    // Is painting selection enabled?
    inline bool painting_selection() const { return m_painting_selection; }

    // Returns painting selection radius
    inline float painting_selection_radius() const { return m_painting_selection_radius; }

    // called by Viewer in BeginFrame
    void update(double delta) override;

    bool draw_toolbar() override;

private:
    void interaction();
    bool select_elements(const Camera& cam,
        MeshModelBase* model,
        Eigen::Vector2f begin,
        Eigen::Vector2f end,
        SelectionBehavior behavior);

    SelectionElementType m_selection_mode = SelectionElementType::OBJECT;
    bool m_select_backfaces = false;
    bool m_painting_selection = false;
    float m_painting_selection_radius = 16.0f;

    std::unique_ptr<TwoStateSelection<BaseObject*>> m_global_selection;

    Callbacks<OnSelectionElementTypeChange> m_callbacks;

    bool m_selection_changed = false; // if selection changed in this frame
};

}
}
