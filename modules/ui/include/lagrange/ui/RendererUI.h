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

#include <memory>
#include <string>
#include <vector>

namespace lagrange {
namespace ui {

class Renderer;
class OptionSet;
class ShaderEditorUI;

/*
UI Panel for Renderer
*/

class RendererUI : public UIPanel<Renderer> {
public:
    RendererUI(Viewer* viewer, std::shared_ptr<Renderer> renderer)
        : UIPanel<Renderer>(viewer, std::move(renderer))
    {}

    const char* get_title() const override { return "Renderer"; }

    void draw() final;
    void update(double dt) final;
    bool reload_shaders();


    struct ScreenshotOptions {
        enum class Mode : int { WINDOW, ACTIVE_VIEWPORT, FBO } mode = Mode::ACTIVE_VIEWPORT;
        Resource<FrameBuffer> selected_fbo;
        std::string folder_path = ".";
    };

    ScreenshotOptions& get_screenshot_options();
    const ScreenshotOptions& get_screenshot_options() const;

private:

    void draw_screenshot_ui();
    ScreenshotOptions m_screenshot_options;

    std::shared_ptr<ShaderEditorUI> m_shader_editor;
    std::deque<float> m_fps_graph_data;
    int m_fps_graph_max_length = 2048;
};

} // namespace ui
} // namespace lagrange
