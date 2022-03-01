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


#include <lagrange/ui/Entity.h>
#include <lagrange/ui/default_components.h>
#include <lagrange/ui/default_keybinds.h>
#include <lagrange/ui/panels/ViewportPanel.h>
#include <lagrange/ui/types/Camera.h>
#include <lagrange/ui/types/Systems.h>
#include <lagrange/ui/types/Tools.h>
#include <lagrange/ui/utils/input.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/uipanel.h>

#include <imgui.h>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>

struct GLFWwindow;
struct ImGuiContext;

namespace lagrange {
namespace ui {


class Renderer;

/// @brief Viewer
/// use `systems()` to add functions that should be called every frame
/// use `registry()` or util functions to read and manipulate application's state
class Viewer
{
public:
    /*
        IO functions
    */
    bool is_key_down(int key);
    bool is_key_pressed(int key);
    bool is_key_released(int key);
    bool is_mouse_down(int key);
    bool is_mouse_clicked(int key);
    bool is_mouse_released(int key);

    /// Returns keyboard and mouse state
    InputState& get_input() { return lagrange::ui::get_input(registry()); }

    /// Returns keyboard and mouse state
    const InputState& get_input() const { return lagrange::ui::get_input(registry()); }

    ///
    /// Window creation options
    ///
    struct WindowOptions
    {
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

        /// Major OpenGL Version
        int gl_version_major = 3;
        /// Minor OpenGL Version
        int gl_version_minor = 3;
        /// Focus the window
        bool focus_on_show = true;

        ///
        /// Default Image Based Light environment map to load
        ///
        /// For available ibls \see create_default_ibl()
        ///
        /// Set to "" to disable ibl
        bool show_default_ibl = true;
        size_t default_ibl_resolution = 256;

        /// Path to imgui .ini file
        /// If not set, the .ini file will be at %APPDATA%/_window_title.ini
        ///
        std::string imgui_ini_path = "";
    };


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

    virtual ~Viewer();

    operator ui::Registry&() { return registry(); }

    operator const ui::Registry&() const { return registry(); }

    ///
    /// UI action wants the window to close
    /// @return window should close
    ///
    bool should_close() const;

    ///
    /// Runs the viewer until closed by the user or main_loop returns false
    ///
    bool run(const std::function<bool(Registry& r)>& main_loop);

    ///
    /// Runs the viewer until closed by the user
    ///
    bool run(const std::function<void(void)>& main_loop = {});

    /// Returns true if viewer initialized succesfully
    bool is_initialized() const;

    /// Returns elapsed time in seconds from the last frame
    double get_frame_elapsed_time() const;

    // Window width in pixels
    int get_width() const { return m_width; }

    // Window height in pixels
    int get_height() const { return m_height; }

    const std::string& get_imgui_config_path() const;

    /// Returns UI window scaling (when Windows or retina scaling is active)
    float get_window_scaling() const;

    /// Returns keybinds object
    Keybinds& get_keybinds();

    /// Returns keybinds object
    const Keybinds& get_keybinds() const;

    /// Returns reference to the registry
    Registry& registry() { return m_registry; }

    /// Returns reference to the registry
    const Registry& registry() const { return m_registry; }

    /// Returns reference to the systems
    const Systems& systems() const { return m_systems; }

    /// Returns reference to the systems
    Systems& systems() { return m_systems; }

    /// Enqueues `fn` to be run on the main thread
    std::future<void> run_on_main_thread(std::function<void(void)> fn);

    /// Returns the limit of how many enqueued functions can be run during a single frame on the main thread
    unsigned int get_main_thread_max_func_per_frame() const
    {
        return m_main_thread_max_func_per_frame;
    }

    /// Sets the limit of how many enqueued functions can be run during a single frame on the main thread
    /// Use std::numeric_limits<unsigned int>::max() for no limit
    void set_main_thread_max_func_per_frame(unsigned int limit)
    {
        m_main_thread_max_func_per_frame = limit;
    }


private:
    bool init_glfw(const WindowOptions& options);
    bool init_imgui();
    bool init_imgui_fonts();


    void resize(int window_width, int window_height);
    void move(int x, int y);
    void update_scale();
    void drop(int count, const char** paths);


    static std::string get_config_folder();

    void process_input();

    void update_time();

    void start_imgui_frame();

    void end_imgui_frame();

    void make_current();

    void draw_menu();

    void start_dockspace();
    void end_dockspace();

    void show_last_shader_error();

    WindowOptions m_initial_window_options;

    GLFWwindow* m_window;
    ImGuiContext* m_imgui_context;


    bool m_initialized = false;

    static bool m_instance_initialized;

    std::string m_imgui_ini_path;

    int m_width;
    int m_height;

    // DPI aware scaling
    float m_ui_scaling = 1.0f;

    std::queue<std::pair<int, int>> m_key_queue;
    std::queue<std::pair<int, int>> m_mouse_key_queue;


    std::string m_last_shader_error;
    std::string m_last_shader_error_desc;

    bool m_show_imgui_demo = false;
    bool m_show_imgui_style = false;

    entt::registry m_registry;
    Systems m_systems;

    Entity m_main_viewport;

    // Functions that will be run on next frame on the main thread
    // Used for code interacting with OpenGL context
    std::mutex m_mutex;
    std::queue<std::pair<std::promise<void>, std::function<void(void)>>> m_main_thread_fn;
    std::atomic_uint m_main_thread_max_func_per_frame = std::numeric_limits<unsigned int>::max();
};

} // namespace ui
} // namespace lagrange
