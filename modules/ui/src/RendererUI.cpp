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
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <lagrange/Logger.h>
#include <lagrange/fs/file_utils.h>
#include <lagrange/ui/GLContext.h>
#include <lagrange/ui/Renderer.h>
#include <lagrange/ui/RendererUI.h>
#include <lagrange/ui/UIWidget.h>
#include <lagrange/ui/Viewer.h>
#include <lagrange/ui/ViewportUI.h>
#include <lagrange/utils/la_assert.h>
#include <lagrange/utils/strings.h>
#include <misc/cpp/imgui_stdlib.h>
#include <nfd.h>
#include <stb_image_write.h>
#include <time.h>

#include <iostream>

namespace lagrange {
namespace ui {

class ShaderEditorUI : public UIPanelBase
{
public:
    ShaderEditorUI(Viewer* viewer, Resource<Shader> resource)
        : UIPanelBase(viewer)
        , m_resource(resource)
    {}

    void draw() override
    {
        if (!is_visible()) return;

        ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_Once);

        this->begin();

        Shader* shader = nullptr;
        std::string* source = nullptr;

        try {
            shader = m_resource.get();
            source = &shader->get_source();

            if (m_last_error.length() > 0) {
                ImGui::SetWindowSize(
                    ImVec2(static_cast<float>(m_width / 2), static_cast<float>(m_height)),
                    ImGuiCond_Always);
            }

            m_last_error = "";
        } catch (const std::exception& ex) {
            logger().error("{}", ex.what());

            if (m_last_error.length() == 0) {
                ImGui::SetWindowSize(
                    ImVec2(static_cast<float>(m_width * 2), static_cast<float>(m_height)),
                    ImGuiCond_Always);
            }
            m_last_error = ex.what();
        }

        if (ImGui::Button("Apply changes")) {
            try {
                auto new_shader = Shader(*source, shader->get_defines());
                *shader = new_shader;

                if (m_last_error.length() > 0) {
                    ImGui::SetWindowSize(
                        ImVec2(static_cast<float>(m_width / 2), static_cast<float>(m_height)),
                        ImGuiCond_Always);
                }

                m_last_error = "";
            } catch (const std::exception& ex) {
                logger().error("{}", ex.what());

                if (m_last_error.length() == 0) {
                    ImGui::SetWindowSize(
                        ImVec2(static_cast<float>(m_width * 2), static_cast<float>(m_height)),
                        ImGuiCond_Always);
                }
                m_last_error = ex.what();
            }

            this->end();
            return;
        }


        if (source) {
            if (m_last_error.length() > 0) {
                ImGui::Columns(2);
            } else {
                ImGui::Columns(1);
            }

            // Source code
            ImGui::InputTextMultiline("##", source, ImGui::GetContentRegionAvail());


            // Error
            if (m_last_error.length() > 0) {
                ImGui::NextColumn();
                ImGui::InputTextMultiline("##error", &m_last_error, ImGui::GetContentRegionAvail());
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }


        m_width = static_cast<int>(ImGui::GetWindowWidth());
        m_height = static_cast<int>(ImGui::GetWindowHeight());

        this->end();
    }


    const char* get_title() const override
    {
        static std::string title = "";

        auto param_pack = std::any_cast<std::tuple<ShaderResourceParams>>(m_resource.get_params());
        auto& desc = std::get<0>(param_pack);

        if (desc.tag != ShaderResourceParams::Tag::CODE_ONLY) {
            title = desc.path + "| Shader Editor";
        } else {
            title = string_format("CODE | Shader Editor");
        }
        return title.c_str();
    }

private:
    Resource<Shader> m_resource;
    std::string m_last_error;
    int m_width = 0;
    int m_height = 0;
};

void RendererUI::draw()
{
    if (!is_visible()) return;

    auto& renderer = get();

    this->begin();


    if (ImGui::CollapsingHeader("Screenshot")) {
        draw_screenshot_ui();
    }


    if (ImGui::CollapsingHeader("FPS Graph")) {
        ImGui::InputInt("FPS History", &m_fps_graph_max_length);

        m_fps_graph_data.push_back(ImGui::GetIO().Framerate);
        if (m_fps_graph_data.size() >= static_cast<size_t>(m_fps_graph_max_length)) {
            m_fps_graph_data.pop_front();
        }

        // Todo use circular buffer
        std::vector<float> plot_data;
        plot_data.reserve(m_fps_graph_data.size());
        float avg_fps = 0.0f;
        float min_fps = std::numeric_limits<float>::max();
        float max_fps = std::numeric_limits<float>::lowest();
        for (auto v : m_fps_graph_data) {
            plot_data.push_back(v);
            avg_fps += v;
            min_fps = std::min(v, min_fps);
            max_fps = std::max(v, max_fps);
        }
        avg_fps /= m_fps_graph_data.size();
        auto overlay =
            string_format("Avg {} FPS | Min {} FPS | Max {} FPS", avg_fps, min_fps, max_fps);

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


    auto& passes = renderer.get_pipeline().get_passes();

    if (ImGui::CollapsingHeader(string_format("Render passes ({})", passes.size()).c_str())) {
        auto pass_ui = [&](RenderPassBase& pass, int index) {
            auto& display_name = pass.get_name();


            if (index != -1) {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("RENDER_PASS_DND", &index, sizeof(int));

                    ImGui::Text("%s", display_name.c_str());

                    ImGui::EndDragDropSource();
                }
            }

            if (index != -1 && ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RENDER_PASS_DND")) {
                    IM_ASSERT(payload->DataSize == sizeof(int));
                    const int source = *(const int*)payload->Data;
                    const auto target = index;

                    renderer.reorder_pass(source, target);
                }
            }

            if (pass.get_options().get_children().size() > 0 ||
                pass.get_options().get_options().size() > 0) {
                if (ImGui::TreeNode((display_name).c_str())) {
                    UIWidget()(pass.get_options());

                    ImGui::TreePop();
                }
            } else {
                ImGuiTreeNodeFlags node_flags =
                    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                ImGui::TreeNodeEx((void*)(intptr_t)index, node_flags, "%s", display_name.c_str());
            }
        };

        if (ImGui::TreeNode("Default")) {
            for (auto& pass_ptr : passes) {
                if (!pass_ptr->has_tag("default")) continue;
                pass_ui(*pass_ptr, -1);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Custom")) {
            for (auto& pass_ptr : passes) {
                if (pass_ptr->has_tag("default")) continue;
                pass_ui(*pass_ptr, -1);
            }
            ImGui::TreePop();
        }
    }

    auto shaders = renderer.get_pipeline().get_resources<Shader>();

    if (ImGui::CollapsingHeader(string_format("Shaders ({})", shaders.size()).c_str())) {
        if (ImGui::Button("Reload")) {
            reload_shaders();
        } else {
            int counter = 0;
            for (auto shader : shaders) {
                auto param_pack =
                    std::any_cast<std::tuple<ShaderResourceParams>>(shader.get_params());
                auto& desc = std::get<0>(param_pack);


                if (desc.tag == ShaderResourceParams::Tag::CODE_ONLY) {
                    ImGui::Text("Custom Code %d", counter++);
                } else {
                    ImGui::Text("%s", desc.path.c_str());
                }

                ImGui::SameLine();
                ImGui::TextColored(
                    ImColor(ImGui::Spectrum::GRAY500).Value,
                    "[%s]",
                    (desc.tag == ShaderResourceParams::Tag::VIRTUAL_PATH)
                        ? "virtual"
                        : ((desc.tag == ShaderResourceParams::Tag::REAL_PATH) ? "real" : "code"));
                ImGui::SameLine();

                ImGui::PushID(reinterpret_cast<void*>(shader.data().get()));

                if (ImGui::Button("Open")) {
                    m_shader_editor = std::make_shared<ShaderEditorUI>(get_viewer(), shader);
                }

                ImGui::PopID();
            }
        }
    }

    auto show_texture = [&](Resource<Texture>& tex) {
        auto w = tex->get_width();
        auto h = tex->get_height();
        if (w > 0 && h > 0) {
            const auto display_w = static_cast<int>(ImGui::GetContentRegionAvail().y - 20.0f);
            const auto display_h = static_cast<int>((float(h) / w) * display_w);

            ImGui::Text("Width: %d, Height: %d", w, h);

            const auto& p = tex->get_params();

            ImGui::Text("Type: ");
            ImGui::SameLine();
            switch (p.type) {
            case GL_TEXTURE_1D: ImGui::Text("GL_TEXTURE_1D"); break;
            case GL_TEXTURE_2D: ImGui::Text("GL_TEXTURE_2D"); break;
            case GL_TEXTURE_3D: ImGui::Text("GL_TEXTURE_3D"); break;
            case GL_TEXTURE_2D_MULTISAMPLE: ImGui::Text("GL_TEXTURE_2D_MULTISAMPLE"); break;
            case GL_TEXTURE_CUBE_MAP: ImGui::Text("GL_TEXTURE_CUBE_MAP"); break;
            default: ImGui::Text("%x", p.type); break;
            }

            ImGui::Text("Format: ");
            ImGui::SameLine();
            switch (p.format) {
            case GL_RGB: ImGui::Text("GL_RGB"); break;
            case GL_RED: ImGui::Text("GL_RED"); break;
            case GL_RGBA: ImGui::Text("GL_RGBA"); break;
            case GL_DEPTH_COMPONENT: ImGui::Text("GL_DEPTH_COMPONENT"); break;
            default: ImGui::Text("%x", p.format); break;
            }

            ImGui::Text("Internal Format: ");
            ImGui::SameLine();
            switch (p.format) {
            case GL_R8: ImGui::Text("GL_R8"); break;
            case GL_RGB: ImGui::Text("GL_RGB"); break;
            case GL_RGBA: ImGui::Text("GL_RGBA"); break;
            case GL_DEPTH_COMPONENT24: ImGui::Text("GL_DEPTH_COMPONENT24"); break;
            case GL_RGB16F: ImGui::Text("GL_RGB16F"); break;
            default: ImGui::Text("%x", p.format); break;
            }

            ImGui::Text("Generate Mipmap: %s", p.generate_mipmap ? "True" : "False");
            ImGui::Text("sRGB: %s", p.sRGB ? "True" : "False");

            auto label = string_format("Texture ID: {}", tex->get_id());


            ImGui::Text("Texture ID: %u", tex->get_id());


            bool clicked = UIWidget()(*tex, display_w, display_h);

            if (clicked) {
                auto tex_pass = get_viewer()->get_renderer().get_default_pass<PASS_TEXTURE_VIEW>();
                if (tex_pass) {
                    auto& opt = tex_pass->get_options();
                    auto current_id = opt.get<int>("Texture ID");
                    auto next_id = int(tex->get_id());
                    if (current_id != next_id) {
                        opt.set<int>("Texture ID", int(tex->get_id()));
                    } else {
                        opt.set<int>("Texture ID", -1);
                    }
                }
            }
        }
    };


    auto fbos = renderer.get_pipeline().get_resources<FrameBuffer>();
    if (ImGui::CollapsingHeader(string_format("FBOs ({})", fbos.size()).c_str())) {
        auto max_colors = FrameBuffer::get_max_color_attachments();

        for (auto fbo_ptr : fbos) {
            ImGui::PushID(fbo_ptr);

            if (ImGui::TreeNode(fbo_ptr.data().get(), "FBO %u", fbo_ptr->get_id())) {
                auto& fbo = *fbo_ptr;

                ImGui::Text("ID: %u", fbo.get_id());

                for (auto i = 0; i < max_colors; i++) {
                    auto tex = fbo.get_color_attachement(i);
                    if (tex) {
                        ImGui::Separator();
                        ImGui::Text("[Color %d] Attachment", i);
                        ImGui::Separator();
                        show_texture(tex);
                    }
                }

                auto depth = fbo.get_depth_attachment();
                ImGui::Separator();
                if (depth) {
                    ImGui::Text("[Depth] Attachment");
                    ImGui::Separator();
                    show_texture(depth);
                } else {
                    ImGui::Text("[Depth] No attachment");
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }


    this->end();

    if (m_shader_editor) {
        m_shader_editor->draw();
    }
}

void RendererUI::update(double LGUI_UNUSED(dt))
{
    if (get_viewer()->is_key_pressed(GLFW_KEY_R) && ImGui::IsKeyDown(GLFW_KEY_LEFT_SHIFT)) {
        reload_shaders();
    }
}

bool RendererUI::reload_shaders()
{
    try {
        auto& renderer = get();
        renderer.reload_shaders();
        logger().info("Shaders reloaded.");
    } catch (std::runtime_error& e) {
        logger().warn("{}", e.what());
        logger().warn("Stopping shader reload.");
        return false;
    }

    return true;
}

RendererUI::ScreenshotOptions& RendererUI::get_screenshot_options()
{
    return m_screenshot_options;
}

const RendererUI::ScreenshotOptions& RendererUI::get_screenshot_options() const
{
    return m_screenshot_options;
}

void RendererUI::draw_screenshot_ui()
{
    auto& opt = m_screenshot_options;

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
                    opt.mode = static_cast<ScreenshotOptions::Mode>(i);
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
    if (opt.mode == ScreenshotOptions::Mode::FBO) {
        auto& renderer = get_viewer()->get_renderer();
        auto fbos = renderer.get_pipeline().get_resources<FrameBuffer>();

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

        if (opt.mode == ScreenshotOptions::Mode::ACTIVE_VIEWPORT) {
            auto& viewport = get_viewer()->get_focused_viewport_ui().get_viewport();
            auto tex = viewport.get_framebuffer().get_color_attachement(0);

            if (tex && tex->save_to(save_path)) {
                logger().info("Saved viewport screenshot to: {}", save_path);
            } else {
                logger().error("Failed to save viewport screenshot to: {}", save_path);
            }
        } else if (opt.mode == ScreenshotOptions::Mode::FBO) {
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
        } else if (opt.mode == ScreenshotOptions::Mode::WINDOW) {
            // Add one-shot callback after rendering is finsihed
            get_viewer()->add_callback<Viewer::OnRenderFinished>([=](Viewer& viewer) {
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
            });
        }
    }
}

} // namespace ui
} // namespace lagrange
