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

#include <lagrange/ui/Viewport.h>
#include <lagrange/utils/strings.h>

namespace lagrange {
namespace ui {


Viewport::Viewport(const std::shared_ptr<Renderer>& renderer,
    const std::shared_ptr<Scene>& scene,
    const std::shared_ptr<Camera>& camera)
: m_renderer(renderer)
, m_scene(scene)
, m_camera(camera)
, m_width(int(camera->get_window_width()))
, m_height(int(camera->get_window_height()))
{
    
    auto& pipeline = m_renderer->get_pipeline();
    
    //Enable all
    for(auto& pass : pipeline.get_passes()) {
        m_selected_passes.insert(pass.get());
    }

    //Disable specific passes
    enable_render_pass(m_renderer->get_default_pass<PASS_EDGE>(), false);
    enable_render_pass(m_renderer->get_default_pass<PASS_VERTEX>(), false);
    enable_render_pass(m_renderer->get_default_pass<PASS_BOUNDING_BOX>(), false);
    enable_render_pass(m_renderer->get_default_pass<PASS_NORMAL>(), false);
    enable_render_pass(m_renderer->get_default_pass<PASS_GROUND>(), false);


    /*
        Create colora and depth textures
    */
    auto color_param = Texture::Params::rgba16f();

    static int viewport_resource_counter = 0;


    auto color_tex = Resource<Texture>::create(color_param);

    auto depth_tex = Resource<Texture>::create(Texture::Params::depth());

    /*
        Create FBO
    */
    FBOResourceParams fbo_params;
    fbo_params.color_attachment_0 = color_tex;
    fbo_params.depth_attachment = depth_tex;
    m_fbo = Resource<FrameBuffer>::create(fbo_params);

    viewport_resource_counter++;
}

std::unique_ptr<Viewport> Viewport::clone()
{
    auto viewport =
        std::make_unique<Viewport>(m_renderer, m_scene, std::make_shared<Camera>(*m_camera));

    const auto& pipeline = m_renderer->get_pipeline();
    for(auto& pass : pipeline.get_passes()) {
        viewport->enable_render_pass(pass.get(), is_render_pass_enabled(pass.get()));
    }

    viewport->set_dimensions(m_width, m_height);
    return viewport;
}

void Viewport::enable_render_pass(RenderPassBase* render_pass, bool value)
{
    if(value)
        m_selected_passes.insert(render_pass);
    else
        m_selected_passes.erase(render_pass);
}

bool Viewport::enable_render_pass(const std::string& name, bool value)
{
    auto pass = m_renderer->get_pipeline().get_pass(name);
    if(!pass) return false;

    if(value)
        m_selected_passes.insert(pass);
    else
        m_selected_passes.erase(pass);

    return true;
}

int Viewport::enable_render_pass_tag(const std::string& tag, bool value) {
    int counter = 0;

    const auto& all_passes = m_renderer->get_pipeline().get_passes();
    for(auto& pass : all_passes) {
        if(pass->has_tag(tag)) {
            enable_render_pass(pass.get(), value);
            counter++;
        }
    }
    return counter;
}

bool Viewport::is_render_pass_enabled(RenderPassBase* render_pass) const
{
    return m_selected_passes.find(render_pass) != m_selected_passes.end();
}

bool Viewport::is_render_pass_enabled(const std::string& name) const
{
    auto pass = m_renderer->get_pipeline().get_pass(name);
    if(!pass) return false;

    return m_selected_passes.find(pass) != m_selected_passes.end();
}

bool Viewport::is_render_pass_enabled_tag(const std::string& tag, bool all_enabled /*= true*/) const
{
    bool result = all_enabled;
    const auto& all_passes = m_renderer->get_pipeline().get_passes();

    for(auto& pass : all_passes) {
        if(!pass->has_tag(tag))
            continue;

        const bool enabled = is_render_pass_enabled(pass.get());
        result = (all_enabled) ? (result && enabled) : (result || enabled);
    }

    return result;
}

Camera& Viewport::get_camera()
{
    return *m_camera;
}

const Camera& Viewport::get_camera() const
{
    return *m_camera;
}

std::shared_ptr<Camera>& Viewport::get_camera_ptr()
{
    return m_camera;
}

void Viewport::set_camera_ptr(const std::shared_ptr<Camera>& new_camera)
{
    m_camera = new_camera;
}

Renderer& Viewport::get_renderer()
{
    return *m_renderer;
}

const Renderer& Viewport::get_renderer() const
{
    return *m_renderer;
}

Scene& Viewport::get_scene()
{
    return *m_scene;
}

const Scene& Viewport::get_scene() const
{
    return *m_scene;
}

void Viewport::set_dimensions(int width, int height)
{
    m_width = width;
    m_height = height;
    if(int(get_camera().get_window_width()) != width || int(get_camera().get_window_height()) != height) {
        get_camera().set_window_dimensions(float(m_width), float(m_height));
    }
}

int Viewport::get_width() const
{
    return m_width;
}

int Viewport::get_height() const
{
    return m_height;
}

FrameBuffer& Viewport::get_framebuffer()
{
    return *m_fbo;
}

const FrameBuffer& Viewport::get_framebuffer() const
{
    return *m_fbo;
}

void Viewport::render()
{
    if(m_width == 0 || m_height == 0) return;

    auto& pipeline = m_renderer->get_pipeline();

    //Enable selected passes and their dependencies
    auto deps = pipeline.enable_with_dependencies(m_selected_passes);

    //Update selection with dependencies
    m_selected_passes.insert(deps.begin(), deps.end());

    m_renderer->render(*m_scene, *m_camera, m_fbo);
}



} // namespace ui
} // namespace lagrange
