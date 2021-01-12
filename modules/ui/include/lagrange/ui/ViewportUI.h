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
#include <lagrange/ui/UIPanel.h>
#include <lagrange/ui/Viewport.h>
#include <chrono>

struct ImVec2;

namespace lagrange {
namespace ui {

/*
    Handles viewport render and interaction
*/
class ViewportUI : public UIPanel<Viewport>, public CallbacksBase<ViewportUI>
{
public:
    // Called after model transform updated by the gizmo
    using PostUpdateModelTransform = UI_CALLBACK(
        void(const std::vector<std::pair<const Model*, const Eigen::Affine3f>>&));

    ViewportUI(Viewer* viewer, const std::shared_ptr<Viewport>& viewport);


    void draw() final;
    void update(double dt) final;

    const char* get_title() const override;

    ///
    /// @brief Viewport interaction action
    ///
    ///
    enum class MouseAction { ROTATE, SELECT, CONTEXT, PAN };

    static void reset_viewport_ui_counter();

    ///
    /// @brief Get the viewport screen position
    ///
    /// @return Eigen::Vector2f position in pixels
    ///
    Eigen::Vector2f get_viewport_screen_position() const;

    ///
    /// @brief Toggle selection action
    ///
    /// @param value
    ///
    void enable_selection(bool value);
    bool is_selection_enabled() const;

    Eigen::Vector2f& get_last_mouse_click_pos(int key);
    Eigen::Vector2f& get_last_mouse_pos();
    bool hovered() const;

    ///
    /// @brief Converts screen pixels to viewport relative pixels
    ///
    /// @param pos screen pixel position
    /// @return Eigen::Vector2f viewport pixel position
    ///
    Eigen::Vector2f screen_to_viewport(const Eigen::Vector2f& pos);

    ///
    /// @brief Get the viewport object
    ///
    /// @return Viewport&
    ///
    Viewport& get_viewport();

    ///
    /// @brief Get the viewport object
    ///
    /// @return Viewport&
    ///
    const Viewport& get_viewport() const;

    ///
    /// @brief Set the auto z-clipping feature
    ///
    void set_auto_zclipping(const bool value)
    {
        m_auto_nearfar = value;
    }

    template <typename CallbackType>
    void add_callback(typename CallbackType::FunType&& fun)
    {
        m_callbacks.add<CallbackType>(CallbackType{fun});
    }


    /// Enable dolly zoom. If disabled, field of view zoom is used.
    void enable_dolly(bool enable);

    /// Whether dolly zoom is enabled.
    bool is_dolly_enabled() const;

    /// When enabled x,y panning is used instead of 3D rotation
    void enable_2D_orthographic_panning(bool enable);

    /// Enables automatic clipping planes based on the scene bounds
    void enable_automatic_clipping_planes(bool enable);

private:
    void draw_viewport_toolbar();
    void draw_options();
    void interaction();


    void zoom(float delta, Eigen::Vector2f screen_pos);


    size_t select_objects(
        bool persistent, Eigen::Vector2f begin, Eigen::Vector2f end, SelectionBehavior behavior);

    size_t select_elements(
        bool persistent, Eigen::Vector2f begin, Eigen::Vector2f end, SelectionBehavior behavior);

    void rotate_camera();

    void gizmo(const Selection<BaseObject*>& selection);


    int m_avail_height = 0;
    int m_avail_width = 0;
    int m_id;

    const static int max_mouse_buttons = 5;

    Eigen::Vector2f m_last_mouse_click[max_mouse_buttons];
    std::chrono::high_resolution_clock::time_point m_last_mouse_click_time[max_mouse_buttons];
    std::chrono::high_resolution_clock::time_point m_last_mouse_release_time[max_mouse_buttons];
    Eigen::Vector2f m_last_mouse_pos;
    Eigen::Vector2f m_canvas_pos;
    bool m_rotation_active = false;
    bool m_selection_enabled = true;

    bool m_hovered = false;

    Eigen::Vector2f m_rotation_mouse_start;
    Eigen::Vector3f m_rotation_camera_pos_start;
    Eigen::Vector3f m_rotation_camera_up_start;

    bool m_dolly_active = false;
    Eigen::Vector2f m_dolly_mouse_start;


    bool m_gizmo_active = false;
    Eigen::Affine3f m_gizmo_transform;
    Eigen::Affine3f m_gizmo_transform_start;
    std::unordered_map<BaseObject*, Eigen::Affine3f> m_gizmo_object_transforms;

    static int m_viewport_ui_counter;

    bool m_ortho_interaction_2D = false;

    Callbacks<PostUpdateModelTransform> m_callbacks;


    bool m_auto_nearfar = true;
    bool m_fov_zoom = false;
    friend CallbacksBase<ViewportUI>;
};

} // namespace ui
} // namespace lagrange
