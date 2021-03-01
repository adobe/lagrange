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
#include <lagrange/ui/Viewer.h>
#include <stdio.h>
#ifdef _WIN32
#include <Windows.h>
#endif


#include <lagrange/ui/GLContext.h>

#include <fstream>

// Imgui and related
#include <IconsFontAwesome5.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

// Docking API is currently internal only
// Change to public API once imgui docking is finished and in master
#include <IconsFontAwesome5.h>
#include <ImGuizmo.h>
#include <fonts/fontawesome5.h>
#include <lagrange/Logger.h>
#include <lagrange/ui/CameraUI.h>
#include <lagrange/ui/DetailUI.h>
#include <lagrange/ui/KeybindsUI.h>
#include <lagrange/ui/LogUI.h>
#include <lagrange/ui/MeshBufferFactory.h>
#include <lagrange/ui/MeshModel.h>
#include <lagrange/ui/ModelFactory.h>
#include <lagrange/ui/Renderer.h>
#include <lagrange/ui/RendererUI.h>
#include <lagrange/ui/Scene.h>
#include <lagrange/ui/SceneUI.h>
#include <lagrange/ui/SelectionUI.h>
#include <lagrange/ui/ToolbarUI.h>
#include <lagrange/ui/ViewportUI.h>
#include <lagrange/ui/default_ibls.h>

#ifdef _WIN32
/*
    Windows only - generate minidump on unhandled exception
*/

#include <Dbghelp.h>
#include <stdio.h>
#include <strsafe.h>
#include <tchar.h>
#include <windows.h>

#pragma comment(lib, "DbgHelp")

LONG CALLBACK unhandled_handler(EXCEPTION_POINTERS* e)
{
    SYSTEMTIME sysTime = {0};
    GetSystemTime(&sysTime);

    TCHAR dump_fname[1024] = {0};
    StringCbPrintf(
        dump_fname,
        _countof(dump_fname),
        _T("%s_%4d-%02d-%02d_%02d-%02d-%02d.dmp"),
        _T("lagrange_ui_dump"),
        sysTime.wYear,
        sysTime.wMonth,
        sysTime.wDay,
        sysTime.wHour,
        sysTime.wMinute,
        sysTime.wSecond);

    HANDLE file_handle = CreateFile(
        dump_fname,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        0,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        0);

    if (file_handle == INVALID_HANDLE_VALUE) return EXCEPTION_CONTINUE_SEARCH;

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = e;
    exceptionInfo.ClientPointers = FALSE;

    _tprintf("Unhandled Exception occured, dumping to %s", dump_fname);

    MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        file_handle,
        MINIDUMP_TYPE(
            MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory | MiniDumpWithFullMemory),
        e ? &exceptionInfo : NULL,
        NULL,
        NULL);

    if (file_handle) {
        CloseHandle(file_handle);
        file_handle = NULL;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

#define MODAL_NAME_SHADER_ERROR "Shader Error"

namespace lagrange {
namespace ui {

bool Viewer::is_key_down(int key)
{
    return !ImGui::IsAnyItemActive() && ImGui::IsKeyDown(key);
}

bool Viewer::is_key_pressed(int key)
{
    return !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(key);
}

bool Viewer::is_key_released(int key)
{
    return !ImGui::IsAnyItemActive() && ImGui::IsKeyReleased(key);
}

bool Viewer::is_mouse_down(int key)
{
    return !ImGui::IsAnyItemActive() && ImGui::IsMouseDown(key);
}

bool Viewer::is_mouse_clicked(int key)
{
    return !ImGui::IsAnyItemActive() && ImGui::IsMouseClicked(key);
}

bool Viewer::is_mouse_released(int key)
{
    return !ImGui::IsAnyItemActive() && ImGui::IsMouseReleased(key);
}

Viewer::Viewer(const std::string& window_title, int window_width, int window_height)
    : Viewer(WindowOptions{window_title, -1, -1, window_width, window_height})
{}

Viewer::Viewer(const WindowOptions& window_options)
    : m_initial_window_options(window_options)
    , m_imgui_ini_path(get_config_folder() + "_" + m_initial_window_options.window_title + ".ini")
    , m_keybinds(initialize_default_keybinds())
{
#ifdef _WIN32
    if (window_options.minidump_on_crash) {
        SetUnhandledExceptionFilter(unhandled_handler);
    }
#endif

    register_default_resources();

    // For now register used types, TODO: leave it up to the user to initialize non basic types
    register_mesh_resource<Vertices3Df, Triangles>();
    register_mesh_resource<Vertices3D, Triangles>();
    register_mesh_resource<Vertices3Df, Quads>();
    register_mesh_resource<Vertices3D, Quads>();
    register_mesh_resource<Vertices2Df, Triangles>();
    register_mesh_resource<Vertices2D, Triangles>();
    register_mesh_resource<Vertices2Df, Quads>();
    register_mesh_resource<Vertices2D, Quads>();
    register_mesh_resource<Eigen::MatrixXf, Eigen::MatrixXi>();
    register_mesh_resource<
        Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>,
        Eigen::
            Matrix<typename Triangles::Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>();
    register_mesh_resource<
        Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>,
        Eigen::
            Matrix<typename Triangles::Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>();

    UIPanelBase::reset_ui_panel_counter();
    ViewportUI::reset_viewport_ui_counter();

    m_log_ui_ptr = &(add_ui_panel(std::make_shared<LogUI>(this)));

    if (!init_glfw(window_options)) return;

    if (!init_imgui(window_options)) return;

    // Initialize objects
    m_scene = std::make_shared<Scene>();
    try {
        m_renderer = std::make_shared<Renderer>(window_options.default_render_passes);
    } catch (std::runtime_error& ex) {
        logger().error("Renderer failed to initialize: {}", ex.what());
        return;
    }

    m_width = window_options.width;
    m_height = window_options.height;

    auto ground_pass = m_renderer->get_default_pass<PASS_GROUND>();
    if (ground_pass) {
        m_ground = std::make_unique<Ground>(*ground_pass);
    }

    m_renderer_ui_ptr = &(add_ui_panel(std::make_shared<RendererUI>(this, m_renderer)));
    m_scene_ui_ptr = &(add_ui_panel(std::make_shared<SceneUI>(this, m_scene)));
    m_selection = &(add_ui_panel(std::make_shared<SelectionUI>(this)));
    m_detail_ui_ptr = &(add_ui_panel(std::make_shared<DetailUI>(this)));
    m_toolbar_ui_ptr = &(add_ui_panel(std::make_shared<ToolbarUI>(this)));
    m_focused_viewport_ui_ptr = &(add_viewport_panel(
        std::make_shared<ViewportUI>(this, std::make_shared<Viewport>(m_renderer, m_scene))));

    m_camera_ui_ptr = &(add_ui_panel(
        std::make_shared<CameraUI>(this, m_focused_viewport_ui_ptr->get().get_camera_ptr())));

    m_keybinds_ui_ptr = &(add_ui_panel(std::make_shared<KeybindsUI>(this)));
    m_keybinds_ui_ptr->set_visible(false);


    // Add drop loading
    add_callback<OnDrop>([](Viewer& v, int n, const char** vals) {
        auto& scene = v.get_scene();

        for (auto i = 0; i < n; i++) {
            const fs::path f = vals[i];
            if (f.extension() == ".obj") {
                auto p = v.get_scene_ui().get_mesh_load_params();
                auto res = ModelFactory::load_obj<QuadMesh3D>(f, p);
                scene.add_models(std::move(res));
            }
        }
    });

    resize(0, 0);

    m_width = window_options.width;
    m_height = window_options.height;

    // m_last_timestamp = glfwGetTime();

    m_initialized = true;
    m_instance_initialized = true;

    /*
        Load default ibl
    */
    if (window_options.default_ibl != "") {
        m_scene->add_emitter(create_default_ibl(window_options.default_ibl));
    }

    get_renderer().update_selection(get_selection());
    get_selection().set_selection_mode(SelectionElementType::OBJECT);
}

void Viewer::draw_toolbar()
{
    if (UIWidget::button_toolbar(
            get_manipulation_mode() == ManipulationMode::SELECT,
            ICON_FA_VECTOR_SQUARE,
            "Select",
            "global.manipulation_mode.select",
            &get_keybinds())) {
        set_manipulation_mode(ManipulationMode::SELECT);
    }

    if (UIWidget::button_toolbar(
            get_manipulation_mode() == ManipulationMode::TRANSLATE,
            ICON_FA_ARROWS_ALT,
            "Translate",
            "global.manipulation_mode.translate",
            &get_keybinds())) {
        set_manipulation_mode(ManipulationMode::TRANSLATE);
    }

    if (UIWidget::button_toolbar(
            get_manipulation_mode() == ManipulationMode::ROTATE,
            ICON_FA_REDO,
            "Rotate",
            "global.manipulation_mode.rotate",
            &get_keybinds())) {
        set_manipulation_mode(ManipulationMode::ROTATE);
    }

    if (UIWidget::button_toolbar(
            get_manipulation_mode() == ManipulationMode::SCALE,
            ICON_FA_COMPRESS_ARROWS_ALT,
            "Scale",
            "global.manipulation_mode.scale",
            &get_keybinds())) {
        set_manipulation_mode(ManipulationMode::SCALE);
    }
}

void Viewer::begin_frame()
{
    assert(m_window);
    glfwMakeContextCurrent(m_window);
    assert(m_window);

    m_mouse_delta = {0, 0};
    glfwPollEvents();


    /*
        Process one keyboard event at a time
    */
    if (!m_key_queue.empty()) {
        auto event = m_key_queue.front();
        get_keybinds().set_key_state(event.first, event.second);
        m_key_queue.pop();
    }

    /*
        Process one mouse key event at a time
    */
    if (!m_mouse_key_queue.empty()) {
        auto event = m_mouse_key_queue.front();
        get_keybinds().set_key_state(event.first, event.second);
        m_mouse_key_queue.pop();
    }

    ImGui::SetCurrentContext(m_imgui_context);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Set keybind context
    std::string keybind_context = "global";
    if (get_focused_viewport_ui().hovered()) {
        keybind_context = "viewport";
    }
    m_keybinds.update(keybind_context);


    {
        auto& io = ImGui::GetIO();
        io.FontGlobalScale = 0.5f * m_ui_scaling; // divide by two since we're oversampling
        auto& style = ImGui::GetStyle();

        ImGui::PushStyleVar(
            ImGuiStyleVar_FramePadding,
            ImVec2(style.FramePadding.x * m_ui_scaling, style.FramePadding.y * m_ui_scaling));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, style.FrameRounding * m_ui_scaling);
        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, style.TabRounding * m_ui_scaling);
        ImGui::PushStyleVar(
            ImGuiStyleVar_ScrollbarSize,
            std::max(7.0f, style.ScrollbarSize * m_ui_scaling));
        ImGui::PushStyleVar(
            ImGuiStyleVar_ScrollbarRounding,
            style.ScrollbarRounding * m_ui_scaling);
    }

    // Update scene and renderer
    float dt = ImGui::GetIO().DeltaTime;
    get_selection().update(dt);
    get_scene().update(dt);
    get_renderer().update();

    // Update panels
    for (auto& panel : m_ui_panels) {
        panel->update(dt);
    }


    /*
        Clear default framebuffer
    */
    GLScope gl;
    {
        gl(glBindFramebuffer, GL_FRAMEBUFFER, 0);
        gl(glViewport, 0, 0, m_width, m_height);
        Color bgcolor = Color(0, 0, 0, 0);
        gl(glClearColor, bgcolor.x(), bgcolor.y(), bgcolor.z(), bgcolor.a());
        gl(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    // Imgui Tab styling
    {
        ImGui::PushStyleColor(ImGuiCol_Tab, ImColor(255, 255, 255, 255).Value);
        ImGui::PushStyleColor(ImGuiCol_TabActive, ImColor(255, 255, 255, 255).Value);
        ImGui::PushStyleColor(ImGuiCol_TabHovered, ImColor(255, 255, 255, 255).Value);
        ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImColor(ImGui::Spectrum::GRAY200).Value);
        ImGui::PushStyleColor(ImGuiCol_TabUnfocused, ImColor(ImGui::Spectrum::GRAY400).Value);
    }
    static bool imgui_demo = false;
    static bool imgui_style = false;


    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("File")) {
        m_scene_ui_ptr->draw_menu();

        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_WINDOW_CLOSE " Quit")) {
            glfwSetWindowShouldClose(m_window, GL_TRUE);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        for (auto& panel : m_ui_panels) {
            const char* title = panel->get_title();
            if (title[0] != '#') {
                if (ImGui::MenuItem(title, "", panel->is_visible())) {
                    panel->set_visible(!panel->is_visible());
                }
            }
        }

        ImGui::Separator();
        ImGui::MenuItem("ImGui Demo Window", "", &imgui_demo);
        ImGui::MenuItem("Style Editor", "", &imgui_style);

        ImGui::Separator();


        if (ImGui::MenuItem("New Viewport")) {
            add_viewport_panel();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Reset layout")) {
            // Empty the .ini file
            {
                std::ofstream f(get_imgui_config_path(), std::ios::app);
            }

            reset_layout();
        }

        ImGui::EndMenu();
    }

    for (auto& panel : m_ui_panels) {
        if (panel.get() == m_scene_ui_ptr) continue;
        panel->draw_menu();
    }


#ifdef _DEBUG
    if (m_ui_scaling != 1.0f) {
        ImGui::Text("(dpi scale: %.2f)", m_ui_scaling);
    }
#endif


    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 5);
    ImGui::Text("(%.0f fps)", ImGui::GetIO().Framerate);
    if ((m_initial_window_options.fullscreen || m_initial_window_options.window_fullscreen)) {
        if (ImGui::Button(ICON_FA_CROSS)) {
            glfwSetWindowShouldClose(m_window, GL_TRUE);
        }
    }

    m_menubar_height = ImGui::GetWindowSize().y;
    ImGui::EndMainMenuBar();

    m_dockspace_id = ImGui::GetID("MyDockSpace");
    if (ImGui::DockBuilderGetNode(m_dockspace_id) == NULL) {
        reset_layout();
    }

    // Show tab bar if there's more than one viewport
    if (m_viewports.size() > 1) {
        for (auto* v : m_viewports) {
            v->enable_tab_bar(true);
        }
    }

    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(ToolbarUI::toolbar_width, m_menubar_height));
        ImGui::SetNextWindowSize(ImVec2(
            viewport->Size.x - ToolbarUI::toolbar_width,
            viewport->Size.y - m_menubar_height));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    }

    ImGui::Begin(
        "Dockspace window",
        nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(3);

    ImGui::DockSpace(m_dockspace_id);

    if (imgui_demo) ImGui::ShowDemoWindow(&imgui_demo);

    if (imgui_style) {
        ImGui::Begin("Style Editor", &imgui_style);
        ImGui::ShowStyleEditor();
        ImGui::End();
    }

    for (auto& panel : m_ui_panels) {
        if (panel->is_visible()) panel->draw();
    }

    if (!m_dock_queue.empty()) {
        auto& fn = m_dock_queue.front();
        if (fn()) {
            m_dock_queue.pop();
        }
    }

    ImGui::End();
    ImGui::PopStyleColor(5);
}

void Viewer::end_frame()
{
    /*
        Render to texture
    */
    try {
        m_last_shader_error = "";
        m_last_shader_error_desc = "";

        for (auto* v : m_viewports) {
            v->get().render();
        }

    } catch (ShaderException& ex) {
        ImGui::OpenPopup(MODAL_NAME_SHADER_ERROR);
        m_last_shader_error = ex.what();
        m_last_shader_error_desc = ex.get_desc();
    }


    if (ImGui::BeginPopupModal(MODAL_NAME_SHADER_ERROR)) {
        ImGui::Text("%s", m_last_shader_error_desc.c_str());

        ImGui::InputTextMultiline(
            "",
            &m_last_shader_error,
            ImVec2(float((get_width() / 3) * 2), float((get_height() / 5) * 4)));

        if (m_last_shader_error.length() == 0) {
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Try again", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }


    m_renderer->end_frame();

    ImGui::PopStyleVar(5);
    ImGui::Render(); // note: this renders to imgui's vertex buffers, not to screen

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // render to screen buffer

    m_callbacks.call<OnRenderFinished>(*this);

    glfwSwapBuffers(m_window);
    m_frame_counter++;
}

bool Viewer::should_close() const
{
    return glfwWindowShouldClose(m_window);
}

bool Viewer::run()
{
    if (!is_initialized()) return false;

    while (!should_close()) {
        begin_frame();
        end_frame();
    }
    return true;
}

Scene& Viewer::get_scene()
{
    return *m_scene;
}

const Scene& Viewer::get_scene() const
{
    return *m_scene;
}

Renderer& Viewer::get_renderer()
{
    return *m_renderer;
}

const Renderer& Viewer::get_renderer() const
{
    return *m_renderer;
}

SelectionUI& Viewer::get_selection()
{
    return *m_selection;
}

const SelectionUI& Viewer::get_selection() const
{
    return *m_selection;
}

RenderPass<Viz::PassData>* Viewer::add_viz(const Viz& config, bool show /*= true*/)
{
    auto ptr = m_renderer->add_viz(config);
    if (!ptr) {
        lagrange::logger().error("Failed to add visualization.");
        return nullptr;
    }

    if (show) {
        for (auto* viewport_ui : m_viewports) {
            viewport_ui->get_viewport().enable_render_pass(ptr, true);
        }
    }
    return ptr;
}

Camera& Viewer::get_current_camera()
{
    assert(m_focused_viewport_ui_ptr);
    return m_focused_viewport_ui_ptr->get().get_camera();
}

const Camera& Viewer::get_current_camera() const
{
    assert(m_focused_viewport_ui_ptr);
    return m_focused_viewport_ui_ptr->get().get_camera();
}

bool Viewer::is_initialized() const
{
    return m_initialized;
}

bool Viewer::remove_ui_panel(const UIPanelBase* panel)
{
    auto it = std::find_if(m_ui_panels.begin(), m_ui_panels.end(), [=](auto& panel_ptr) {
        return panel_ptr.get() == panel;
    });

    if (it == m_ui_panels.end()) return false;
    m_ui_panels.erase(it);

    // Search in viewports and remove there as well
    {
        auto it_v = std::find_if(m_viewports.begin(), m_viewports.end(), [=](auto& panel_ptr) {
            return panel_ptr == panel;
        });
        if (it_v != m_viewports.end()) {
            m_viewports.erase(it_v);
        }
    }

    return true;
}

ViewportUI& Viewer::add_viewport_panel(std::shared_ptr<ViewportUI> viewport_panel)
{
    if (viewport_panel == nullptr) {
        viewport_panel = std::make_shared<ViewportUI>(
            this,
            lagrange::to_shared_ptr(get_focused_viewport_ui().get().clone()));
    }


    viewport_panel->UIPanelBase::add_callback<UIPanelBase::OnChangeFocus>([=](UIPanelBase& panel,
                                                                              bool focused) {
        if (focused) {
            this->m_focused_viewport_ui_ptr = reinterpret_cast<ViewportUI*>(&panel);

            // Set viewport's camera to camera_ui
            if (this->m_camera_ui_ptr) {
                this->m_camera_ui_ptr->set(this->m_focused_viewport_ui_ptr->get().get_camera_ptr());
            }
        }
    });

    m_viewports.push_back(viewport_panel.get());
    return add_ui_panel(std::move(viewport_panel));
}


SceneUI& Viewer::get_scene_ui()
{
    return *m_scene_ui_ptr;
}

CameraUI& Viewer::get_camera_ui()
{
    return *m_camera_ui_ptr;
}

RendererUI& Viewer::get_renderer_ui()
{
    return *m_renderer_ui_ptr;
}

ViewportUI& Viewer::get_focused_viewport_ui()
{
    return *m_focused_viewport_ui_ptr;
}

LogUI& Viewer::get_log_ui()
{
    return *m_log_ui_ptr;
}

DetailUI& Viewer::get_detail_ui()
{
    return *m_detail_ui_ptr;
}

double Viewer::get_frame_elapsed_time() const
{
    return ImGui::GetIO().DeltaTime;
}

void Viewer::reset_layout()
{
    constexpr float right_panel_width = 320.0f;
    constexpr float bottom_panel_width = 150.0f;

    ImGui::DockBuilderRemoveNode(m_dockspace_id);
    ImGui::DockBuilderAddNode(
        m_dockspace_id,
        ImGuiDockNodeFlagsPrivate_::ImGuiDockNodeFlags_DockSpace |
            ImGuiDockNodeFlagsPrivate_::ImGuiDockNodeFlags_HiddenTabBar);
    ImGui::DockBuilderSetNodeSize(m_dockspace_id, ImVec2(float(m_width), float(m_height)));

    ImGuiID dock_id_main;
    ImGuiID dock_id_right;
    ImGuiID dock_id_right_top;
    ImGuiID dock_id_right_bottom;
    ImGuiID dock_id_bottom;

    ImGui::DockBuilderSplitNode(
        m_dockspace_id,
        ImGuiDir_Right,
        right_panel_width / float(m_width),
        &dock_id_right,
        &dock_id_main);

    ImGui::DockBuilderSplitNode(
        dock_id_right,
        ImGuiDir_Down,
        0.7f,
        &dock_id_right_bottom,
        &dock_id_right_top);

    ImGui::DockBuilderSplitNode(
        dock_id_main,
        ImGuiDir_Down,
        bottom_panel_width / float(m_height),
        &dock_id_bottom,
        &dock_id_main);

    // Disable tab bar for viewport
    {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id_main);
        node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_CentralNode;
    }

    ImGui::DockBuilderDockWindow(m_focused_viewport_ui_ptr->get_title(), dock_id_main);

    ImGui::DockBuilderDockWindow(m_scene_ui_ptr->get_title(), dock_id_right_top);

    // docking creation API is still experimental. We can improve this code once it's stable.
    // Note: do not force order using focus (FocusWindow), it'll focus the whole docking window
    // and cover any user-created floating windows.
    //
    // https://github.com/ocornut/imgui/issues/3136
    //
    ImGui::DockBuilderDockWindow(m_renderer_ui_ptr->get_title(), dock_id_right_bottom);
    ImGui::DockBuilderDockWindow(m_camera_ui_ptr->get_title(), dock_id_right_bottom);
    ImGui::DockBuilderDockWindow(m_detail_ui_ptr->get_title(), dock_id_right_bottom);

    ImGui::DockBuilderDockWindow(m_log_ui_ptr->get_title(), dock_id_bottom);

    ImGui::DockBuilderFinish(m_dockspace_id);
}

void Viewer::set_manipulation_mode(ManipulationMode mode)
{
    m_manipulation_mode = mode;
    m_callbacks.call<OnManipulationModeChange>(mode);
}

Viewer::ManipulationMode Viewer::get_manipulation_mode() const
{
    return m_manipulation_mode;
}

const std::string& Viewer::get_imgui_config_path() const
{
    return m_imgui_ini_path;
}

void Viewer::enqueue_dock(
    UIPanelBase& target,
    UIPanelBase& source,
    UIPanelBase::DockDir dir,
    float ratio,
    bool split_outer)
{
    auto target_ptr = &target;
    auto source_ptr = &source;
    m_dock_queue.push([=]() { return source_ptr->dock_to(*target_ptr, dir, ratio, split_outer); });
}

float Viewer::get_window_scaling() const
{
    return m_ui_scaling;
}

void Viewer::enable_ground(bool enable)
{
    auto pass = m_renderer->get_default_pass<PASS_GROUND>();
    LA_ASSERT(pass, "Ground render pass was not enabled");
    for (auto v : m_viewports) {
        v->get().enable_render_pass(pass, enable);
    }
}

Ground& Viewer::get_ground()
{
    LA_ASSERT(m_ground, "Ground render pass was not enabled.");
    return *m_ground;
}

const Ground& Viewer::get_ground() const
{
    LA_ASSERT(m_ground, "Ground render pass was not enabled.");
    return *m_ground;
}

Viewer::~Viewer()
{
    m_callbacks.call<OnClose>(*this);
    // Explicitly remove scene and renderer first before closing
    // the opengl context

    // Remove ui panels first
    m_ui_panels.clear();
    m_scene = nullptr;
    m_renderer = nullptr;

    MeshBuffer::clear_static_data();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(m_imgui_context);
    glfwDestroyWindow(m_window);
    glfwTerminate(); // TODO ref count windows
}

bool Viewer::init_glfw(const WindowOptions& options)
{
    glfwSetErrorCallback(
        [](int error, const char* msg) { logger().error("GLFW Error {}: {}", error, msg); });

    if (!glfwInit()) {
        logger().error("Failed to initialize GLFW");
        return false;
    }

    GLState::major_version = options.gl_version_major;
    GLState::minor_version = options.gl_version_minor;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GLState::major_version);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GLState::minor_version);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (options.gl_version_major > 3 && options.gl_version_minor > 1) {
        // previous two hints were here but that caused crashes under osx...
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);

    glfwWindowHint(GLFW_FOCUSED, options.focus_on_show ? GLFW_TRUE : GLFW_FALSE);

    const char* glfw_version = glfwGetVersionString();
    logger().info("GLFW compile time version: {}", glfw_version);
    logger().info(
        "Requested context: {}.{}, GLSL {}",
        GLState::major_version,
        GLState::minor_version,
        GLState::get_glsl_version_string());

    int monitor_count = 0;
    int monitor_index = options.monitor_index;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
    if (options.monitor_index > monitor_count) monitor_index = 0;

    int width = options.width;
    int height = options.height;

    m_window = glfwCreateWindow(
        width,
        height,
        options.window_title.c_str(),
        (options.fullscreen) ? monitors[options.monitor_index] : nullptr,
        nullptr);

    if (!m_window) {
        logger().error("Failed to create window");
        return false;
    }

    int xpos, ypos;
    glfwGetMonitorPos(monitors[monitor_index], &xpos, &ypos);

    int workarea_x, workarea_y, screen_res_x, screen_res_y;
    glfwGetMonitorWorkarea(
        monitors[monitor_index],
        &workarea_x,
        &workarea_y,
        &screen_res_x,
        &screen_res_y);

    // Center by default
    int user_x_pos = (options.pos_x != -1) ? options.pos_x : (screen_res_x - width) / 2;
    int user_y_pos = (options.pos_y != -1) ? options.pos_y : (screen_res_y - height) / 2;

    if (options.window_fullscreen) {
        glfwMaximizeWindow(m_window);
    } else {
        xpos += user_x_pos;
        ypos += user_y_pos;
        glfwSetWindowPos(m_window, xpos, ypos);
    }


    assert(m_window);
    glfwMakeContextCurrent(m_window);
    assert(m_window);

    gl3wInit();

    glfwSwapInterval(options.vsync ? 1 : 0);

    {
        float xscale, yscale;
        glfwGetWindowContentScale(m_window, &xscale, &yscale);

        int client_api = glfwGetWindowAttrib(m_window, GLFW_CLIENT_API);
        int creation_api = glfwGetWindowAttrib(m_window, GLFW_CONTEXT_CREATION_API);
        int v_major = glfwGetWindowAttrib(m_window, GLFW_CONTEXT_VERSION_MAJOR);
        int v_minor = glfwGetWindowAttrib(m_window, GLFW_CONTEXT_VERSION_MINOR);
        int v_revision = glfwGetWindowAttrib(m_window, GLFW_CONTEXT_REVISION);
        int gl_profile = glfwGetWindowAttrib(m_window, GLFW_OPENGL_PROFILE);
        int gl_forward_compat = glfwGetWindowAttrib(m_window, GLFW_OPENGL_FORWARD_COMPAT);

        std::string client_api_s = "GLFW_NO_API";
        if (client_api == GLFW_OPENGL_API) client_api_s = "GLFW_OPENGL_API";
        if (client_api == GLFW_OPENGL_ES_API) client_api_s = "GLFW_OPENGL_ES_API";

        std::string creation_api_s = "GLFW_NATIVE_CONTEXT_API";
        if (creation_api == GLFW_EGL_CONTEXT_API) creation_api_s = "GLFW_EGL_CONTEXT_API";
        if (creation_api == GLFW_OSMESA_CONTEXT_API) creation_api_s = "GLFW_OSMESA_CONTEXT_API";

        std::string opengl_profile_s = "GLFW_OPENGL_CORE_PROFILE";
        if (gl_profile == GLFW_OPENGL_COMPAT_PROFILE)
            opengl_profile_s = "GLFW_OPENGL_COMPAT_PROFILE";
        if (gl_profile == GLFW_OPENGL_ANY_PROFILE) opengl_profile_s = "GLFW_OPENGL_ANY_PROFILE";

        logger().info("Client API : {}", client_api_s);
        logger().info("Creation API : {}", creation_api_s);
        logger().info(
            "Context version | Major: {}, Minor: {}, Revision: {}",
            v_major,
            v_minor,
            v_revision);
        logger().info("Forward Compatibility: {}", (gl_forward_compat ? "True" : "False"));
        logger().info("OpenGL Profile: {}", opengl_profile_s);

        logger().info("Window scale : {}, {}", xscale, yscale);
    }


    logger().info("OpenGL Driver");
    logger().info("Vendor: {}", glGetString(GL_VENDOR));
    logger().info("Renderer: {}", glGetString(GL_RENDERER));
    logger().info("Version: {}", glGetString(GL_VERSION));
    logger().info("Shading language version: {}", glGetString(GL_SHADING_LANGUAGE_VERSION));

    /**
     * Set up callbacks
     */

    // Set custom pointer to (this) so we can retrieve it later in
    // stateless lambdas.
    glfwSetWindowUserPointer(m_window, this);

    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int w, int h) {
        static_cast<Viewer*>(glfwGetWindowUserPointer(window))->resize(w, h);
    });

    glfwSetWindowPosCallback(m_window, [](GLFWwindow* window, int x, int y) {
        static_cast<Viewer*>(glfwGetWindowUserPointer(window))->resize(x, y);
    });


    glfwSetDropCallback(m_window, [](GLFWwindow* window, int cnt, const char** paths) {
        static_cast<Viewer*>(glfwGetWindowUserPointer(window))->drop(cnt, paths);
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x, double y) {
        static_cast<Viewer*>(glfwGetWindowUserPointer(window))->cursor_pos(x, y);
    });

    glfwSetKeyCallback(
        m_window,
        [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
            static_cast<Viewer*>(glfwGetWindowUserPointer(window))->m_key_queue.push({key, action});
        });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int) {
        static_cast<Viewer*>(glfwGetWindowUserPointer(window))
            ->m_mouse_key_queue.push({button, action});
    });

    return true;
}

bool Viewer::init_imgui(const WindowOptions& LGUI_UNUSED(window_options))
{
    IMGUI_CHECKVERSION();
    m_imgui_context = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_imgui_context);
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(GLState::get_glsl_version_string().c_str());

    ImGuiIO& io = ImGui::GetIO();
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; experimental
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::Spectrum::StyleColorsSpectrum();


    // Set default color picker options
    ImGui::SetColorEditOptions(
        ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB |
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_PickerHueWheel);

    ImGui::GetIO().IniFilename = get_imgui_config_path().c_str();

    init_imgui_fonts();

    return true;
}

bool Viewer::init_imgui_fonts()
{
    const float base_size = 32.0f;
    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Clear();


    ImGui::Spectrum::LoadFont(base_size);

    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;

    auto font_awesome = io.Fonts->AddFontFromMemoryCompressedTTF(
        fontawesome5_compressed_data,
        fontawesome5_compressed_size,
        base_size,
        &icons_config,
        icons_ranges);

    io.Fonts->Build();

    return font_awesome != nullptr;
}

void Viewer::resize(int window_width, int window_height)
{
    if (window_width < 0 || window_height < 0) {
        return;
    }

    int fwidth, fheight, wwidth, wheight;
    glfwGetFramebufferSize(m_window, &fwidth, &fheight);
    glfwGetWindowSize(m_window, &wwidth, &wheight);

    update_scale();


    m_width = window_width;
    m_height = window_height;

    m_callbacks.call<OnResize>(*this, window_width, window_height);
}

void Viewer::move(int LGUI_UNUSED(x), int LGUI_UNUSED(y))
{
    update_scale();
}

void Viewer::update_scale()
{
    Eigen::Vector2f content_scale = Eigen::Vector2f::Ones();
    glfwGetWindowContentScale(m_window, &content_scale.x(), &content_scale.y());

    int fwidth, fheight, wwidth, wheight;
    glfwGetFramebufferSize(m_window, &fwidth, &fheight);
    glfwGetWindowSize(m_window, &wwidth, &wheight);
    if (wwidth <= 0) return;
    m_ui_scaling = (wwidth / float(fwidth)) * content_scale.x();
}

void Viewer::drop(int count, const char** paths)
{
    m_callbacks.call<OnDrop>(*this, count, paths);
}

std::string Viewer::get_config_folder()
{
#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    return std::string(appdata) + "\\";
#else
    LA_ASSERT("Appdata folder not implemented on unix yet");
    return "";
#endif
}

std::string Viewer::get_options_file_path()
{
    return (get_config_folder() + "lagrange-ui.json");
}

Eigen::Vector2f Viewer::get_mouse_pos()
{
    return m_mouse_pos;
}

Eigen::Vector2f Viewer::get_mouse_delta()
{
    return m_mouse_delta;
}

void Viewer::cursor_pos(double x, double y)
{
    Eigen::Vector2f new_pos = Eigen::Vector2f(x, y);
    m_mouse_delta += new_pos - m_mouse_pos;
    m_mouse_pos = new_pos;
}

bool Viewer::m_instance_initialized = false;

} // namespace ui
} // namespace lagrange
