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

#include <lagrange/ui/Model.h>
#include <lagrange/ui/Renderer.h>
#include <lagrange/ui/Scene.h>

namespace lagrange {
namespace ui {


/*
    Defines single viewport with camera and selected render passes
*/
class Viewport {
public:
    Viewport(const std::shared_ptr<Renderer>& renderer,
        const std::shared_ptr<Scene>& scene,
        const std::shared_ptr<Camera>& camera = std::make_shared<Camera>(
            Camera::default_camera(100, 100)));

    /*
        Copy viewport (Create new framebuffer but copy settings)
    */
    std::unique_ptr<Viewport> clone();

    void enable_render_pass(RenderPassBase* render_pass, bool value);
    bool enable_render_pass(const std::string& name, bool value);

    //Returns number of enabled/disabled passes with tag
    int enable_render_pass_tag(const std::string& tag, bool value);

    bool is_render_pass_enabled(RenderPassBase* render_pass) const;
    bool is_render_pass_enabled(const std::string& name) const;
    bool is_render_pass_enabled_tag(const std::string& tag, bool all_enabled = true) const;

    Camera& get_camera();
    const Camera& get_camera() const;

    std::shared_ptr<Camera>& get_camera_ptr();
    void set_camera_ptr(const std::shared_ptr<Camera>& new_camera);

    Renderer& get_renderer();
    const Renderer& get_renderer() const;

    Scene& get_scene();
    const Scene& get_scene() const;


    void set_dimensions(int width, int height);

    int get_width() const;
    int get_height() const;

    FrameBuffer& get_framebuffer();
    const FrameBuffer& get_framebuffer() const;

    void render();


private:
    std::shared_ptr<Renderer> m_renderer;
    std::shared_ptr<Scene> m_scene;

    std::shared_ptr<Camera> m_camera;
    std::set<RenderPassBase*> m_selected_passes;

    int m_width;
    int m_height;

    Resource<FrameBuffer> m_fbo;
};

} // namespace ui
} // namespace lagrange
