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

#include <lagrange/ui/Camera.h>
#include <lagrange/ui/Color.h>
#include <lagrange/ui/Model.h>
#include <lagrange/ui/RenderPipeline.h>
#include <lagrange/ui/Viz.h>
#include <lagrange/ui/default_render_passes.h>
#include <lagrange/ui/default_resources.h>

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace lagrange {
namespace ui {

class Scene;
class Texture;
class FrameBuffer;
class RenderableManager;
class SelectionUI;

template <typename MeshType>
class MeshModel;

struct CommonPassData;

/*
    Wrapper for RenderPipeline
    Sets up default render passes
*/
class Renderer {
public:
    friend class SelectionUI;

    Renderer(DefaultPasses_t default_passes = PASS_ALL);
    virtual ~Renderer();

    const RenderPipeline& get_pipeline() const;
    RenderPipeline& get_pipeline();

    RenderPass<Viz::PassData>* add_viz(const Viz & viz);

    RenderPassBase& add_render_pass(std::unique_ptr<RenderPassBase> pass);

    template <typename T>
    RenderPass<T>& add_render_pass(std::unique_ptr<RenderPass<T>> pass)
    {
        auto& ref = *pass;
        m_pipeline.add_pass(std::move(pass));
        return ref;
    }

    /*
        Moves source pass at the position of target
    */
    bool reorder_pass(size_t source, size_t target);
    bool reorder_pass(const std::string& source_name, const std::string& target_name);

    void render(Scene& scene, const Camera& camera, Resource<FrameBuffer> target_fbo);

    void update();

    void end_frame();

    void reload_shaders();

    const Camera& get_current_camera() const { return m_current_camera; }

    void update_selection(const SelectionUI& selection);

    const CommonPassData& get_common_pass_data();

    /*
        Immediate mode
    */
    void draw_line(Eigen::Vector3f a, Eigen::Vector3f b, Color colorA, Color colorB);
    void draw_line(Eigen::Vector3f a, Eigen::Vector3f b, Color color);
    void draw_point(Eigen::Vector3f a, Color color = Color::red());
    void draw_triangle(Eigen::Vector3f a, Eigen::Vector3f b, Eigen::Vector3f c, Color color);
    void draw_triangle(Eigen::Vector3f a,
        Eigen::Vector3f b,
        Eigen::Vector3f c,
        Color colorA,
        Color colorB,
        Color colorC);

    void draw_quad(
        Eigen::Vector3f a, Eigen::Vector3f b, Eigen::Vector3f c, Eigen::Vector3f d, Color color);

    void draw_quad(
        Eigen::Vector3f a, Eigen::Vector3f b, Eigen::Vector3f c, Eigen::Vector3f d, 
        Color colorA,
        Color colorB,
        Color colorC,
        Color colorD);

    void draw_tetrahedron_lines(Eigen::Vector3f a, Eigen::Vector3f b, Eigen::Vector3f c, Eigen::Vector3f d, Color color);

    void draw_cone_lines(Eigen::Vector3f pt0,
        Eigen::Vector3f pt1,
        float radius0,
        float radius1,
        Color color,
        int segments = 8);

    void draw_arrow(Eigen::Vector3f start,
        Eigen::Vector3f end,
        Color color,
        float radius = 0.1f,
        float arrow_length = 0.5f);

    void draw_sphere_lines_simple(Eigen::Vector3f center,
        float radius,
        Color color,
        const int segments = 16);




    /*
        Returns a default pass with the underlying type
    */
    template <DefaultPasses which_pass>
    typename DefaultPassType<which_pass>::type * get_default_pass()
    {
        auto it = m_default_passes.find(which_pass);
        if (it == m_default_passes.end()) return nullptr;
        return reinterpret_cast<typename DefaultPassType<which_pass>::type *>(it->second);
    }

    const AABB& immediate_data_bounds() const { return m_immediate_bounds; }


protected:

    RenderPass<Viz::PassData> & add_default_viz(DefaultPasses pass_enum, const Viz & visualization);

    template <typename T>
    RenderPass<T>& add_default_render_pass(DefaultPasses pass_enum,  std::unique_ptr<RenderPass<T>> pass)
    {
        auto& ref = *pass;
        m_pipeline.add_pass(std::move(pass));
        ref.add_tag("default");
        m_default_passes[pass_enum] = &ref;
        return ref;
    }

    RenderPipeline m_pipeline;

    // Common resources
    Resource<CommonPassData> m_common_pass_data;

    std::unordered_map<DefaultPasses, RenderPassBase*> m_default_passes;

    // Final output framebuffer
    Resource<FrameBuffer> m_default_fbo;

    // Cached camera
    Camera m_current_camera;

    // Immediate draw call data
    Resource<ImmediateData> m_immediate_data;
    AABB m_immediate_bounds;

    // Get selection from viewer
    // TwoStateSelection<BaseObject*> * m_selection = nullptr;
    SelectionElementType m_selection_type;

};

} // namespace ui
} // namespace lagrange
