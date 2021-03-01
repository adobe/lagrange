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
#include <lagrange/ui/Camera.h>
#include <lagrange/ui/Options.h>
#include <lagrange/ui/Selection.h>
#include <lagrange/ui/UIPanel.h>
#include <lagrange/ui/Viz.h>
#include <lagrange/ui/default_render_passes.h>
#include <lagrange/ui/default_keybinds.h>

#include <memory>
#include <set>
#include <string>
#include <queue>

struct GLFWwindow;
struct ImGuiContext;

namespace lagrange {
namespace ui {

class Scene;
class Renderer;
class SelectionUI;
class UIPanelBase;
class SceneUI;
class RendererUI;
class CameraUI;
class DetailUI;
class ViewportUI;
class ToolbarUI;
class LogUI;
class SelectionUI;
class KeybindsUI;

///
/// Main Viewer Class
///
class Viewer : public CallbacksBase<Viewer> {
public:
    // Called on window resize
    using OnResize = UI_CALLBACK(void(Viewer&, int, int));

    // Called on file drop
    using OnDrop = UI_CALLBACK(void(Viewer&, int, const char**));

    // Called in destructor
    using OnClose = UI_CALLBACK(void(Viewer&));

    enum class ManipulationMode;
    using OnManipulationModeChange = UI_CALLBACK(void(ManipulationMode));

    // Called after all rendering finished, before buffers are swapped
    using OnRenderFinished = UI_CALLBACK(void(Viewer&));

    /*
        IO functions
    */
    bool is_key_down(int key);
    bool is_key_pressed(int key);
    bool is_key_released(int key);
    bool is_mouse_down(int key);
    bool is_mouse_clicked(int key);
    bool is_mouse_released(int key);

    /// Returns mouse position in pixels
    Eigen::Vector2f get_mouse_pos();

    // Returns mouse position change from last frame in pixels
    Eigen::Vector2f get_mouse_delta();

    ///
    /// Window creation options
    ///
    struct WindowOptions {

        /// Window title
        std::string window_title = "";

        /// Initial window X position. Set -1 for automatic position.
        int pos_x = -1;
        /// Initial window Y position. Set -1 for automatic position.
        int pos_y = -1;
        /// Initial window width
        int width = 1024;
        /// Initial window height
        int height = 768;
        /// Maximizes window (with current resolution)
        bool window_fullscreen = false;
        /// Fullscreen mode - not recommended
        bool fullscreen = false;
        /// Enable vertical sync to limit framerate
        bool vsync = true;
        /// Which monitor to create window on
        int monitor_index = 0;

        ///
        /// Specifies which default passes will be created
        ///
        /// Use as bitmask:
        ///
        /// e.g. default_render_passes &= ~DefaultPasses::PASS_FXAA will disable the FXAA pass
        ///
        /// See DefaultPasses for options
        ///
        DefaultPasses_t default_render_passes = DefaultPasses::PASS_ALL;

        /// Major OpenGL Version
        int gl_version_major = 3;
        /// Minor OpenGL Version
        int gl_version_minor = 3;
        /// Focus the window
        bool focus_on_show = true;

        ///
        /// Generates a dump on crash in the current folder.
        /// This is done by catching normally uncaught exceptions.
        /// \note Windows only
        ///
        bool minidump_on_crash = true;

        ///
        /// Default Image Based Light environment map to load
        ///
        /// For available ibls \see create_default_ibl()
        ///
        /// Set to "" to disable ibl
        std::string default_ibl = "studio011";
    };


    ///
    /// Object manipulation mode
    ///
    enum class ManipulationMode { SELECT, TRANSLATE, ROTATE, SCALE, count };


    ///
    /// Creates a window with given title and size and default options
    ///
    /// @param[in] window_title Window title
    /// @param[in] window_width Initial window width
    /// @param[in] window_height Initial window height
    ///
    Viewer(const std::string& window_title, int window_width, int window_height);

    ///
    /// Creates a window with given options
    ///
    /// @param[in] window_options Window creation options
    ///
    Viewer(const WindowOptions& window_options);

    ~Viewer();


    /// Starts a new frame
    void begin_frame();


    /// Ends current frame and renders it
    void end_frame();

    ///
    /// UI action wants the window to close
    /// @return window should close
    ///
    bool should_close() const;

    ///
    /// Runs the viewer until UI wants to close it.
    ///
    bool run();


    Scene& get_scene();
    const Scene& get_scene() const;

    Renderer& get_renderer();
    const Renderer& get_renderer() const;

    SelectionUI& get_selection();
    const SelectionUI& get_selection() const;

    ///
    /// Adds render pass defined in Viz::Config
    /// @param[in] config   Visualization configuration
    /// @param[in] show     Show or hide the resulting render pass
    ///
    RenderPass<Viz::PassData>* add_viz(const Viz& visualization, bool show = true);


    /// Returns camera of currently focused viewport
    Camera& get_current_camera();
    /// Returns camera of currently focused viewport
    const Camera& get_current_camera() const;


    /// Returns true if viewer initialized
    bool is_initialized() const;

    ///
    /// Adds a UI panel
    ///
    /// @param[in] ui_panel     must inherit from UIPanelBase
    /// @return                 reference to the ui panel
    ///
    template <typename T>
    T& add_ui_panel(std::shared_ptr<T> ui_panel)
    {
        m_ui_panels.push_back(ui_panel);
        return *ui_panel;
    }


    bool remove_ui_panel(const UIPanelBase* panel);

    ViewportUI& add_viewport_panel(std::shared_ptr<ViewportUI> viewport_panel = nullptr);


    /// Default Scene UI Panel
    SceneUI& get_scene_ui();

    /// Default Camera UI Panel
    CameraUI& get_camera_ui();

    /// Default Renderer UI Panel
    RendererUI& get_renderer_ui();

    /// Returns currently focused viewport UI Panel
    ViewportUI& get_focused_viewport_ui();
    const std::vector<ViewportUI*>& get_viewports() { return m_viewports; }

    /// Default Logger UI Panel
    LogUI& get_log_ui();

    /// Default Detail UI Panel
    DetailUI& get_detail_ui();

    /// DPI scaling factor
    float get_ui_scaling() { return m_ui_scaling; }

    /// Returns elapsed time in seconds from the last frame
    double get_frame_elapsed_time() const;

    int get_width() const { return m_width; }
    int get_height() const { return m_height; }
    float get_menubar_height() const { return m_menubar_height; }

    /// Reset UI Panel layout and docking
    void reset_layout();

    void set_manipulation_mode(ManipulationMode mode);
    ManipulationMode get_manipulation_mode() const;

    const std::string& get_imgui_config_path() const;

    /// Returns all UI Panels
    const std::vector<std::shared_ptr<UIPanelBase>>& get_ui_panels() const { return m_ui_panels; }

    /// Enqueues docking of source to target in direction with ratio
    void enqueue_dock(UIPanelBase& target,
        UIPanelBase& source,
        UIPanelBase::DockDir dir,
        float ratio = 0.5f,
        bool split_outer = false);

    /// Returns UI window scaling (when Windows or retina scaling is active)
    float get_window_scaling() const;

    /// Returns keybinds object
    Keybinds& get_keybinds() { return m_keybinds; }

    /// Returns keybinds object
    const Keybinds& get_keybinds() const { return m_keybinds; }

    /// Enable ground rendering
    void enable_ground(bool enable);

    /// Returns ground object
    Ground& get_ground();

    /// Returns ground object
    const Ground& get_ground() const;

    /// Internal use only!
    void draw_toolbar();

private:
    bool init_glfw(const WindowOptions& options);
    bool init_imgui(const WindowOptions& options);
    bool init_imgui_fonts();

    void resize(int window_width, int window_height);
    void move(int x, int y);
    void update_scale();
    void drop(int count, const char** paths);
    void cursor_pos(double x, double y);

    static std::string get_config_folder();
    static std::string get_options_file_path();

    WindowOptions m_initial_window_options;

    GLFWwindow* m_window;
    ImGuiContext* m_imgui_context;

    Callbacks<OnResize,
        OnDrop,
        OnClose,
        OnManipulationModeChange,
        OnRenderFinished>
        m_callbacks;

    std::shared_ptr<Scene> m_scene;
    std::shared_ptr<Renderer> m_renderer;

    std::vector<std::shared_ptr<UIPanelBase>> m_ui_panels;
    std::vector<ViewportUI*> m_viewports;
    unsigned int m_dockspace_id;

    // Default ui elements, store for later retrieval
    SceneUI* m_scene_ui_ptr = nullptr;
    CameraUI* m_camera_ui_ptr = nullptr;
    RendererUI* m_renderer_ui_ptr = nullptr;
    DetailUI* m_detail_ui_ptr = nullptr;
    ViewportUI* m_focused_viewport_ui_ptr = nullptr;
    ToolbarUI* m_toolbar_ui_ptr = nullptr;
    LogUI* m_log_ui_ptr = nullptr;
    SelectionUI* m_selection = nullptr;
    KeybindsUI* m_keybinds_ui_ptr = nullptr;

    bool m_initialized = false;

    static bool m_instance_initialized;

    const std::string m_imgui_ini_path;

    /*
        IO
    */
    Eigen::Vector2f m_mouse_pos;
    Eigen::Vector2f m_mouse_delta;

    int m_width;
    int m_height;
    float m_menubar_height = 0.0f;

    // DPI aware scaling
    float m_ui_scaling = 1.0f;

    ManipulationMode m_manipulation_mode = ManipulationMode::SELECT;

    size_t m_frame_counter = 0;

    std::queue<std::function<bool()>> m_dock_queue;
    std::queue<std::pair<int, int>> m_key_queue;
    std::queue<std::pair<int, int>> m_mouse_key_queue;

    Keybinds m_keybinds;

    std::unique_ptr<Ground> m_ground;

    std::string m_last_shader_error;
    std::string m_last_shader_error_desc;


    friend CallbacksBase<Viewer>;

};

} // namespace ui
} // namespace lagrange
