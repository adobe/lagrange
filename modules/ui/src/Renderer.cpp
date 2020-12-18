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
#include <lagrange/ui/MeshModel.h>
#include <lagrange/ui/RenderUtils.h>
#include <lagrange/ui/Renderer.h>
#include <lagrange/ui/Scene.h>
#include <lagrange/ui/default_render_passes.h>
#include <lagrange/ui/ui_common.h>
#include <lagrange/ui/SelectionUI.h>


#include <algorithm>

namespace lagrange {
namespace ui {

Renderer::Renderer(DefaultPasses_t default_passes)
{
    /*
        Create global resources
        These will be updated every frame
    */
    m_common_pass_data = Resource<CommonPassData>::create();
    m_common_pass_data->scene = nullptr;
    m_common_pass_data->camera = nullptr;
    m_common_pass_data->final_output_fbo = nullptr;
    m_common_pass_data->pass_counter = Resource<int>::create();
    m_common_pass_data->selection_global = nullptr;

    /*
        Create default fbo and assign it to common pass data
    */
    {
        FBOResourceParams fbo_params;
        fbo_params.managed = false;
        fbo_params.custom_id = 0;
        m_default_fbo = Resource<FrameBuffer>::create(fbo_params);
        m_common_pass_data->final_output_fbo = m_default_fbo.get();
    }

    /*
        Add default render passes
    */


    // Skybox background
    if (default_passes & PASS_SKYBOX) {
        add_default_render_pass(PASS_SKYBOX, create_skybox_pass(m_common_pass_data));
    }

    if (default_passes & PASS_BRDFLUT) {
        add_default_render_pass(PASS_BRDFLUT, create_BRDFLUT_Pass(m_common_pass_data));
    }

    // Shadow mapping (needed for any shaded viz)
    if (default_passes & PASS_SHADOW_MAP) {
        add_default_render_pass(PASS_SHADOW_MAP, create_shadow_map_pass(m_common_pass_data));
    }

    if (default_passes & PASS_GROUND) {
        add_default_render_pass(PASS_GROUND,
            create_grid_pass(m_common_pass_data,
                get_default_pass<PASS_SHADOW_MAP>(),
                get_default_pass<PASS_BRDFLUT>()));
    }

    if (default_passes & PASS_PBR) {
        add_default_viz(PASS_PBR, Viz::create_default_pbr()).add_tag("pbr");
    }
    if (default_passes & PASS_EDGE) {
        add_default_viz(PASS_EDGE, Viz::create_default_edge()).add_tag("post");
    }
    if (default_passes & PASS_VERTEX) {
        add_default_viz(PASS_VERTEX, Viz::create_default_vertex()).add_tag("post");
    }

    /*
        Selection passes
    */
    if (default_passes & PASS_SELECTED_FACET) {
        add_default_viz(PASS_SELECTED_FACET, Viz::create_default_selected_facet())
            .add_tag("post");
    }
    if (default_passes & PASS_SELECTED_EDGE) {
        add_default_viz(PASS_SELECTED_EDGE, Viz::create_default_selected_edge())
            .add_tag("post");
    }
    if (default_passes & PASS_SELECTED_VERTEX) {
        add_default_viz(PASS_SELECTED_VERTEX, Viz::create_default_selected_vertex())
            .add_tag("post");
    }

    if (default_passes & PASS_NORMAL) {
        add_default_render_pass(PASS_NORMAL, create_normals_pass(m_common_pass_data));
    }

    if (default_passes & PASS_BOUNDING_BOX) {
        add_default_viz(PASS_BOUNDING_BOX, Viz::create_default_bounding_box())
            .add_tags({"boundingbox", "post"});
    }

    m_immediate_data = Resource<ImmediateData>::create();
    if (default_passes & PASS_IMMEDIATE) {
        add_default_render_pass(
            PASS_IMMEDIATE, create_immediate_pass(m_common_pass_data, m_immediate_data));
    }

    if (default_passes & PASS_TEXTURE_VIEW) {
        add_default_render_pass(
            PASS_TEXTURE_VIEW, create_textureview_pass(m_common_pass_data, m_pipeline))
            .add_tag("post");
    }

    if (default_passes & PASS_OBJECT_ID || default_passes & PASS_OBJECT_OUTLINE) {
        auto& face_id = add_default_viz(
            PASS_OBJECT_ID, Viz::create_objectid("SelectedFaceIDPass", Viz::Filter::SHOW_SELECTED));
        face_id.add_tags({"outline", "post"});

        add_default_render_pass(PASS_OBJECT_OUTLINE,
            create_object_outline_pass(&face_id,
                m_common_pass_data));

    }

    // Antialiasing
    if (default_passes & PASS_FXAA) {
        add_default_render_pass(PASS_FXAA, create_fxaa_pass(m_common_pass_data)).add_tag("post");
    }
}

Renderer::~Renderer() {}

void Renderer::render(Scene& scene, const Camera& camera, Resource<FrameBuffer> target_fbo /*= nullptr*/
)
{
    m_current_camera = camera;

    auto fbo_resource_ptr = (target_fbo.has_value()) ? target_fbo : m_default_fbo;
    auto& fbo = *fbo_resource_ptr;



    GLScope gl;


    // Clear default fbo
    {
        fbo.bind();

        // If custom fbo, resize first
        if (!target_fbo.has_value()) {
            fbo.resize_attachments(int(camera.get_window_width()), int(camera.get_window_height()));
        }

        // depth
        GL(glEnable(GL_DEPTH_TEST));
        GL(glDepthMask(GL_TRUE));
        GL(glDepthFunc(GL_LESS));

        // blend
        GL(glEnable(GL_BLEND));
        GL(glEnable(GL_MULTISAMPLE));
        GL(glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));
        GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO));
        GL(glEnable(GL_LINE_SMOOTH));
        GL(glHint(GL_LINE_SMOOTH_HINT, GL_NICEST));


        gl(glViewport, 0, 0, int(camera.get_window_width()), int(camera.get_window_height()));


        Color bgcolor = Color(1, 1, 1, 1);
        gl(glClearColor, bgcolor.x(), bgcolor.y(), bgcolor.z(), bgcolor.a());
        gl(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }


    // Execute pipeline
    {
        m_common_pass_data->final_output_fbo = (fbo_resource_ptr.get());
        m_common_pass_data->camera = &camera;
        m_common_pass_data->scene = &scene;
        *(m_common_pass_data->pass_counter) = 0;


        try {
            m_pipeline.execute();
        }
        catch (ShaderException& ) {
            m_default_fbo->bind();
            std::rethrow_exception(std::current_exception());
        }
        catch (std::exception& ex) {
            logger().error("Can't render: {}", ex.what());
        }
    }


    m_default_fbo->bind();
}

const RenderPipeline& Renderer::get_pipeline() const
{
    return m_pipeline;
}

RenderPipeline& Renderer::get_pipeline()
{
    return m_pipeline;
}


RenderPass<Viz::PassData>* Renderer::add_viz(const Viz& viz)
{
    Viz cfg_validated = viz;
    std::string err;
    if (!cfg_validated.validate(err)) {
        lagrange::logger().error("Failed to validate visualization {} : {}", viz.viz_name, err);
        return nullptr;
    }
    return cfg_validated.add_to(m_pipeline, m_common_pass_data);
}

RenderPassBase& Renderer::add_render_pass(std::unique_ptr<RenderPassBase> pass)
{
    auto& ref = *pass;
    m_pipeline.add_pass(std::move(pass));
    return ref;
}

bool Renderer::reorder_pass(size_t source, size_t target)
{
    if (target == source) return false;

    auto& passes = m_pipeline.get_passes();

    if (target < source) {
        passes.insert(passes.begin() + target, std::move(passes[source]));
        passes.erase(passes.begin() + source + 1);
    }
    else {
        passes.insert(passes.begin() + target + 1, std::move(passes[source]));
        passes.erase(passes.begin() + source);
    }

    return true;
}

bool Renderer::reorder_pass(const std::string& source_name, const std::string& target_name)
{
    const auto& passes = m_pipeline.get_passes();
    auto it_source = std::find_if(
        passes.begin(), passes.end(), [&](auto& pass) { return pass->get_name() == source_name; });

    if (it_source == passes.end()) return false;

    auto it_target = std::find_if(
        passes.begin(), passes.end(), [&](auto& pass) { return pass->get_name() == target_name; });

    if (it_target == passes.end()) return false;

    reorder_pass(
        std::distance(passes.begin(), it_source), std::distance(passes.begin(), it_target));
    return true;
}

void Renderer::update()
{
    // Keep passes marked with "post" at the end (AFTER custom user passes)
    // Includes things like FXAA, default edge/vertex display, selection etc.
    auto& passes = m_pipeline.get_passes();

    std::stable_sort(passes.begin(), passes.end(), [](auto& a, auto& b) {
        return !a->has_tag("post") && b->has_tag("post");
    });
}

void Renderer::end_frame() {

    // Reset immediate data
    m_immediate_bounds = AABB();
    (*m_immediate_data).lines.clear();
    (*m_immediate_data).lines_colors.clear();
    (*m_immediate_data).points.clear();
    (*m_immediate_data).points_colors.clear();
    (*m_immediate_data).triangles.clear();
    (*m_immediate_data).triangles_colors.clear();
}

void Renderer::reload_shaders()
{
    m_pipeline.reset();
}


void Renderer::update_selection(const SelectionUI& viewerselection)
{
    auto& selection = viewerselection.get_global();
    m_common_pass_data->selection_global = &selection;
    m_selection_type = viewerselection.get_selection_mode();

    /*
        Set up filtering of default passes based on selection mode.
        TODO move stuff from viewer to here
    */
    auto edge = get_default_pass<PASS_EDGE>();
    if (!edge) return;

    if (m_selection_type != SelectionElementType::OBJECT) {
        edge->get_data().filter_global = Viz::Filter::SHOW_SELECTED;
    }
    else {
        edge->get_data().filter_global = Viz::Filter::SHOW_ALL;
    }
}

const CommonPassData& Renderer::get_common_pass_data()
{
    return *m_common_pass_data;
}

void Renderer::draw_line(Eigen::Vector3f a, Eigen::Vector3f b, Color colorA, Color colorB)
{
    auto& data = (*m_immediate_data);
    m_immediate_bounds.extend(a);
    m_immediate_bounds.extend(b);
    data.lines.push_back(a);
    data.lines.push_back(b);
    data.lines_colors.push_back(colorA);
    data.lines_colors.push_back(colorB);
}

void Renderer::draw_line(Eigen::Vector3f a, Eigen::Vector3f b, Color color)
{
    draw_line(a, b, color, color);
}

void Renderer::draw_point(Eigen::Vector3f a, Color color /*= Color::red()*/)
{
    auto& data = (*m_immediate_data);
    data.points.push_back(a);
    data.points_colors.push_back(color);
    m_immediate_bounds.extend(a);
}

void Renderer::draw_triangle(Eigen::Vector3f a,
    Eigen::Vector3f b,
    Eigen::Vector3f c,
    Color colorA,
    Color colorB,
    Color colorC)
{
    auto& data = (*m_immediate_data);
    data.triangles.push_back(a);
    data.triangles.push_back(b);
    data.triangles.push_back(c);
    data.triangles_colors.push_back(colorA);
    data.triangles_colors.push_back(colorB);
    data.triangles_colors.push_back(colorC);
    m_immediate_bounds.extend(a);
    m_immediate_bounds.extend(b);
    m_immediate_bounds.extend(c);
}

void Renderer::draw_triangle(Eigen::Vector3f a, Eigen::Vector3f b, Eigen::Vector3f c, Color color)
{
    draw_triangle(a, b, c, color, color, color);
}



void Renderer::draw_quad(Eigen::Vector3f a,
    Eigen::Vector3f b,
    Eigen::Vector3f c,
    Eigen::Vector3f d,
    Color colorA,
    Color colorB,
    Color colorC,
    Color colorD)
{
    draw_triangle(a, b, c, colorA, colorB, colorC);
    draw_triangle(c, d, a, colorC, colorD, colorA);
}

void Renderer::draw_quad(
    Eigen::Vector3f a, Eigen::Vector3f b, Eigen::Vector3f c, Eigen::Vector3f d, Color color)
{
    draw_quad(a, b, c, d, color, color, color, color);
}

void Renderer::draw_tetrahedron_lines(Eigen::Vector3f a,
    Eigen::Vector3f b,
    Eigen::Vector3f c,
    Eigen::Vector3f d,
    Color color)
{
    draw_line(a, b, color);
    draw_line(a, c, color);
    draw_line(a, d, color);
    draw_line(b, c, color);
    draw_line(b, d, color);
    draw_line(c, d, color);
}

void Renderer::draw_cone_lines(Eigen::Vector3f pt0,
    Eigen::Vector3f pt1,
    float radius0,
    float radius1,
    Color color,
    int segments /*= 8*/)
{
    auto plane = utils::render::compute_perpendicular_plane((pt1 - pt0).normalized());

    const float angle_increment = (1.0f / float(segments)) * two_pi();

    for (auto i = 0; i < segments; i++) {
        const float angle = i * angle_increment;
        const float angle_next = ((i + 1) % segments) * angle_increment;

        const Eigen::Vector3f offset = (std::sin(angle) * plane.first + std::cos(angle) * plane.second);
        const Eigen::Vector3f offset_next =
            (std::sin(angle_next) * plane.first + std::cos(angle_next) * plane.second);

        const Eigen::Vector3f a = pt0 + radius0 * offset;
        const Eigen::Vector3f b = pt0 + radius0 * offset_next;
        const Eigen::Vector3f c = pt1 + radius1 * offset_next;
        const Eigen::Vector3f d = pt1 + radius1 * offset;
        draw_line(a, b, color);
        draw_line(b, c, color);
        draw_line(c, d, color);
        draw_line(d, a, color);
    }
}

void Renderer::draw_arrow(Eigen::Vector3f start,
    Eigen::Vector3f end,
    Color color,
    float radius /*= 0.1f*/,
    float arrow_length)
{
    Eigen::Vector3f dir = (end - start).normalized();
    draw_cone_lines(end, end - dir * arrow_length, 0.01f, radius, color);
    draw_line(start, end, color);
}



void Renderer::draw_sphere_lines_simple(Eigen::Vector3f center, float radius, Color color, int segments)
{
    const float angle_increment = (1.0f / float(segments)) * two_pi();

    for (auto i = 0; i < segments; i++) {
        const float angle = i * angle_increment;
        const float angle_next = ((i + 1) % segments) * angle_increment;
        const float s0 = std::sin(angle);
        const float s1 = std::sin(angle_next);
        const float c0 = std::cos(angle);
        const float c1 = std::cos(angle_next);

        Eigen::Vector3f a0 = center + radius * Eigen::Vector3f(s0, c0, 0);
        Eigen::Vector3f b0 = center + radius * Eigen::Vector3f(s1, c1, 0);
        Eigen::Vector3f a1 = center + radius * Eigen::Vector3f(s0, 0, c0);
        Eigen::Vector3f b1 = center + radius * Eigen::Vector3f(s1, 0, c1);
        Eigen::Vector3f a2 = center + radius * Eigen::Vector3f(0, s0, c0);
        Eigen::Vector3f b2 = center + radius * Eigen::Vector3f(0, s1, c1);
        draw_line(a0, b0, color);
        draw_line(a1, b1, color);
        draw_line(a2, b2, color);
    }
}


RenderPass<Viz::PassData>& Renderer::add_default_viz(
    DefaultPasses pass_enum,
    const Viz & cfg)
{

    auto& ref = *add_viz(cfg);
    ref.add_tag("default");
    m_default_passes[pass_enum] = &ref;
    return ref;
}

} // namespace ui
} // namespace lagrange
