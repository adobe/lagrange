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


#include <lagrange/Logger.h>
#include <lagrange/fs/file_utils.h>
#include <lagrange/ui/components/Viewport.h>
#include <lagrange/ui/imgui/UIWidget.h>
#include <lagrange/ui/utils/uipanel.h>
#include <lagrange/ui/panels/RendererPanel.h>
#include <lagrange/ui/panels/ViewportPanel.h>

#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <nfd.h>
#include <stb_image_write.h>
#include <time.h>

namespace lagrange {
namespace ui {

namespace {

struct RendererPanel
{
    std::deque<float> fps_graph_data;
    int fps_graph_max_length = 2048;

    struct ScreenshotOptions
    {
        enum class Mode : int { WINDOW, ACTIVE_VIEWPORT, FBO } mode = Mode::ACTIVE_VIEWPORT;
        std::shared_ptr<FrameBuffer> selected_fbo;
        std::string folder_path = ".";
    } screenshot_options;
};


void draw_screenshot_ui(Registry& registry, RendererPanel& panel)
{
    if (!ImGui::CollapsingHeader("Screenshot")) return;

    auto& opt = panel.screenshot_options;

    auto& viewport_panel =
        registry.get<ViewportPanel>(registry.ctx<FocusedViewportPanel>().viewport_panel);
    auto& viewport = registry.get<ViewportComponent>(viewport_panel.viewport);


    // Pick Ouput folder
    {
        if (ImGui::Button("Browse ...")) {
            nfdchar_t* path = nullptr;
            nfdresult_t result = NFD_PickFolder(nullptr, &path);
            if (result == NFD_OKAY) {
                opt.folder_path = std::string(path);
            }
            free(path);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x / 2.0f);
        ImGui::InputText("Output Folder", &opt.folder_path);
    }

    // Mode combo box
    {
        const std::array<std::string, 3> labels = {
            "Window",
            "Active Viewport",
            "Framebuffer (Active Viewport)"};
        auto selected_mode = static_cast<size_t>(opt.mode);
        if (ImGui::BeginCombo("Mode", labels[selected_mode].c_str())) {
            for (size_t i = 0; i < labels.size(); i++) {
                bool is_selected = (i == selected_mode);

                if (ImGui::Selectable(labels[i].c_str(), is_selected)) {
                    opt.mode = static_cast<RendererPanel::ScreenshotOptions::Mode>(i);
                    break;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    // FBO combo box
    if (opt.mode == RendererPanel::ScreenshotOptions::Mode::FBO) {
#ifdef LGUI_RENDERER_DEPRECATED
        auto fbos = panel.renderer->get_pipeline().get_resources<FrameBuffer>();

        if (fbos.size() > 0) {
            if (!opt.selected_fbo) {
                opt.selected_fbo = fbos.front();
            }
            // Ensure fbo still exists
            else {
                auto it = std::find(fbos.begin(), fbos.end(), opt.selected_fbo);
                if (it == fbos.end()) opt.selected_fbo = fbos.front();
            }

            auto selected_id = (*opt.selected_fbo).get_id();
            const auto selected_label = lagrange::string_format("ID {}", selected_id);

            if (ImGui::BeginCombo("Framebuffer", selected_label.c_str())) {
                for (auto fbo_res : fbos) {
                    bool is_selected = (fbo_res == opt.selected_fbo);

                    const auto label = lagrange::string_format("ID {}", (*fbo_res).get_id());

                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        opt.selected_fbo = fbo_res;
                        break;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::Text("No Framebuffers Found");
        }
#endif
    }


    std::string save_path = "";

    // Generate timestamp
    if (ImGui::Button("Save to Folder")) {
        time_t rawtime;
        struct tm* timeinfo;
        char buffer[128];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(buffer, 128, "%Y-%m-%d_%H-%M-%S", timeinfo);
        save_path = opt.folder_path + "/" + std::string(buffer);
    }
    ImGui::SameLine();

    // Pick file
    if (ImGui::Button("Save as ...")) {
        nfdchar_t* path = nullptr;
        nfdresult_t result = NFD_SaveDialog("png", nullptr, &path);
        if (result == NFD_OKAY) {
            save_path = path;
        }
        free(path);
    }

    if (save_path.length() > 0) {
        save_path = fs::get_string_ending_with(save_path, ".png");

        if (opt.mode == RendererPanel::ScreenshotOptions::Mode::ACTIVE_VIEWPORT) {
            auto tex = viewport.fbo->get_color_attachement(0);

            if (tex && tex->save_to(save_path)) {
                logger().info("Saved viewport screenshot to: {}", save_path);
            } else {
                logger().error("Failed to save viewport screenshot to: {}", save_path);
            }
        } else if (opt.mode == RendererPanel::ScreenshotOptions::Mode::FBO) {
            auto& fbo = (*opt.selected_fbo);
            auto color = fbo.get_color_attachement(0);

            if (color) {
                if (color->save_to(save_path)) {
                    logger().info("Saved color framebuffer screenshot to: {}", save_path);
                } else {
                    logger().error("Failed to save color framebuffer screenshot to: {}", save_path);
                }
            } else {
                auto depth = fbo.get_depth_attachment();
                if (depth && depth->save_to(save_path)) {
                    logger().info("Saved depth framebuffer screenshot to: {}", save_path);
                } else {
                    logger().error("Failed to save depth framebuffer screenshot to: {}", save_path);
                }
            }
        } else if (opt.mode == RendererPanel::ScreenshotOptions::Mode::WINDOW) {
            lagrange::logger().error("Not implemented yet");

            // Add one-shot callback after rendering is finsihed
            /*get_viewer()->add_callback<Viewer::OnRenderFinished>([=](Viewer& viewer) {
                const auto w = viewer.get_width();
                const auto h = viewer.get_height();
                std::vector<unsigned char> pixels(4 * w * h, 0);
                glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

                stbi_flip_vertically_on_write(1);
                if (stbi_write_png(save_path.c_str(), w, h, 4, pixels.data(), w * 4) != 0) {
                    logger().info("Saved window screenshot to: {}", save_path);
                } else {
                    logger().error("Failed to save window screenshot to: {}", save_path);
                }
                viewer.clear_callback<Viewer::OnRenderFinished>();
            });*/
        }
    }
}


void draw_fps_graph(RendererPanel& panel)
{
    if (!ImGui::CollapsingHeader("FPS Graph")) return;

    ImGui::InputInt("FPS History", &panel.fps_graph_max_length);

    panel.fps_graph_data.push_back(ImGui::GetIO().Framerate);
    if (panel.fps_graph_data.size() >= static_cast<size_t>(panel.fps_graph_max_length)) {
        panel.fps_graph_data.pop_front();
    }

    // Todo use circular buffer
    std::vector<float> plot_data;
    plot_data.reserve(panel.fps_graph_data.size());
    float avg_fps = 0.0f;
    float min_fps = std::numeric_limits<float>::max();
    float max_fps = std::numeric_limits<float>::lowest();
    for (auto v : panel.fps_graph_data) {
        plot_data.push_back(v);
        avg_fps += v;
        min_fps = std::min(v, min_fps);
        max_fps = std::max(v, max_fps);
    }
    avg_fps /= panel.fps_graph_data.size();
    auto overlay = string_format("Avg {} FPS | Min {} FPS | Max {} FPS", avg_fps, min_fps, max_fps);

    ImGui::PlotLines(
        "FPS",
        plot_data.data(),
        static_cast<int>(plot_data.size()),
        0,
        overlay.c_str(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        ImVec2(0, 200));
}


void renderer_panel_system(Registry& registry, Entity e)
{
    auto& panel = registry.get<RendererPanel>(e);

    draw_screenshot_ui(registry, panel);
    draw_fps_graph(panel);


} // namespace
} // namespace


Entity add_renderer_panel(Registry& r, const std::string& name /*= "Renderer"*/)
{
    auto e = add_panel(r, name, renderer_panel_system);
    r.emplace<RendererPanel>(e);
    return e;
}

} // namespace ui
} // namespace lagrange
