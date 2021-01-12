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
#include <lagrange/ui/Material.h>
#include <lagrange/ui/MeshBufferFactory.h>
#include <lagrange/ui/MeshModel.h>
#include <lagrange/ui/RenderUtils.h>
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/Scene.h>
#include <lagrange/ui/Viz.h>
#include <lagrange/ui/default_render_passes.h>
#include <lagrange/ui/default_resources.h>


namespace lagrange {
namespace ui {

namespace {

MeshBuffer::Primitive get_primitive_type(Viz::Primitive attribute)
{
    switch (attribute) {
    case Viz::Primitive::POINTS: return MeshBuffer::Primitive::POINTS;
    case Viz::Primitive::LINES: return MeshBuffer::Primitive::LINES;
    case Viz::Primitive::TRIANGLES: return MeshBuffer::Primitive::TRIANGLES;
    default: break;
    }
    return MeshBuffer::Primitive::POINTS;
}

// Todo move somewhere else
using ColorMatrix = Eigen::Matrix<
    Color::BaseType::Scalar,
    Eigen::Dynamic,
    Color::BaseType::RowsAtCompileTime,
    Eigen::RowMajor>;

ColorMatrix create_color_matrix(
    Viz::Attribute attrib_type,
    const ProxyMesh& pm,
    const std::function<Color(int)>& fn)
{
    size_t n = 0;

    switch (attrib_type) {
    case lagrange::ui::Viz::Attribute::NONE: assert(false); break;
    case lagrange::ui::Viz::Attribute::VERTEX: n = pm.get_num_vertices(); break;
    case lagrange::ui::Viz::Attribute::EDGE: n = pm.get_original_edges().size(); break;
    case lagrange::ui::Viz::Attribute::FACET: n = pm.original_facet_num(); break;
    case lagrange::ui::Viz::Attribute::CORNER:
        n = pm.get_num_triangles() * pm.original_facet_dimension();
        break;
    }

    ColorMatrix cm(n, 4);
    for (size_t i = 0; i < n; i++) {
        cm.row(i) = fn(static_cast<int>(i));
    }

    return cm;
}

struct AttribWrapper
{
    virtual ~AttribWrapper() {}
    virtual Viz::AttribValue row(size_t index) const = 0;
};

template <typename T>
struct AttribWrapperDerived : AttribWrapper
{
    const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>* attrib_array_ptr;

    // Hotpath
    // performance issue: allocates new memory for each row
    // Solution: expose direct access to data OR use stack for the values (must have known size
    // though)
    Viz::AttribValue row(size_t index) const override
    {
        return attrib_array_ptr->row(index).template cast<double>().eval();
    }
};


std::unique_ptr<AttribWrapper>
get_attrib_wrapper(Model& model, Viz::Attribute attrib_type, std::string attrib_name)
{
    std::unique_ptr<AttribWrapper> result = nullptr;

    model.visit_tuple<SupportedMeshTypes>([&](const auto& mesh_model) {
        auto& mesh = mesh_model.get_mesh();
        using MeshType = std::remove_reference_t<decltype(mesh)>;
        using Scalar = typename MeshType::Scalar;
        using AttributeArray = typename MeshType::AttributeArray;

        const AttributeArray* ptr = nullptr;

        switch (attrib_type) {
        case Viz::Attribute::VERTEX:
            if (mesh.has_vertex_attribute(attrib_name))
                ptr = &mesh.get_vertex_attribute(attrib_name);
            break;
        case Viz::Attribute::EDGE:
            if (mesh.has_edge_attribute(attrib_name)) ptr = &mesh.get_edge_attribute(attrib_name);
            break;
        case Viz::Attribute::FACET:
            if (mesh.has_facet_attribute(attrib_name)) ptr = &mesh.get_facet_attribute(attrib_name);
            break;
        case Viz::Attribute::CORNER:
            if (mesh.has_corner_attribute(attrib_name))
                ptr = &mesh.get_corner_attribute(attrib_name);
            break;
        default: break;
        }

        if (!ptr) return;

        auto derived = std::make_unique<AttribWrapperDerived<Scalar>>();
        derived->attrib_array_ptr = ptr;
        result = std::move(derived);
    });

    return result;
}

void configure_fbo(RenderResourceBuilder& builder, const Viz& cfg, Viz::PassData& pass_data)
{
    assert(pass_data.common->final_output_fbo);

    const auto& fbo_config = cfg.fbo_config;
    // Custom target fbo
    if (fbo_config.target_fbo.has_value()) {
        builder.read(fbo_config.target_fbo);
        pass_data.target_fbo = fbo_config.target_fbo;
    }
    // Create own fbo
    else if (fbo_config.create_color && fbo_config.create_depth) {
        if (fbo_config.create_color)
            pass_data.color_buffer = builder.create<Texture>(
                cfg.viz_name + "_color",
                Texture::Params::rgb());

        if (fbo_config.create_depth)
            pass_data.depth_buffer = builder.create<Texture>(
                cfg.viz_name + "depth",
                Texture::Params::depth());

        pass_data.target_fbo = builder.create<FrameBuffer>(
            cfg.viz_name + "fbo",
            FBOResourceParams{pass_data.color_buffer, pass_data.depth_buffer});
    }
    // Use default
    else {
        pass_data.target_fbo.reset();
    }
}


} // namespace


bool Viz::validate(std::string& error_out)
{
    if (colormapping == Colormapping::CUSTOM && custom_index_color_fn == nullptr &&
        custom_attrib_color_fn == nullptr) {
        error_out = "Must specify custom colormapping function when Colormapping == CUSTOM.";
        return false;
    }

    if (colormapping == Colormapping::CUSTOM_INDEX_OBJECT && custom_index_color_fn == nullptr) {
        error_out = "Must specify custom colormapping function (custom_index_color_fn) "
                    "when Colormapping == CUSTOM_INDEX_OBJECT.";
        return false;
    }

    if (colormapping == Colormapping::TEXTURE && attribute != Attribute::CORNER) {
        error_out = "Attribute must be CORNER (tex coords are stored as corner "
                    "attributes). Reason: colormapping == TEXTURE";
        return false;
    }

    if (backside_alpha < 0.0f) {
        if (primitive == Primitive::LINES) {
            backside_alpha = 0.1f;
        } else {
            backside_alpha = 0.05f;
        }
    }

    return true;
}


Viz Viz::create_default_pbr()
{
    Viz cfg;
    cfg.attribute = Viz::Attribute::CORNER;
    cfg.primitive = Viz::Primitive::TRIANGLES;
    cfg.colormapping = Viz::Colormapping::TEXTURE;
    cfg.shading = Viz::Shading::PBR;
    cfg.viz_name = "PBR";
    cfg.backside_alpha = 0.0f;
    cfg.cull_backface = false;
    return cfg;
}

Viz Viz::create_default_phong()
{
    Viz cfg;
    cfg.attribute = Viz::Attribute::CORNER;
    cfg.primitive = Viz::Primitive::TRIANGLES;
    cfg.colormapping = Viz::Colormapping::TEXTURE;
    cfg.shading = Viz::Shading::PHONG;
    cfg.viz_name = "Phong";
    return cfg;
}

Viz Viz::create_default_edge()
{
    return Viz::create_uniform_color("Edges", Primitive::LINES, Color::black());
}

Viz Viz::create_default_vertex()
{
    return Viz::create_uniform_color("Vertices", Primitive::POINTS, Color::black());
}

Viz Viz::create_default_bounding_box()
{
    Viz cfg;
    cfg.replace_with_bounding_box = true;
    cfg.viz_name = "BoundingBox";
    cfg.primitive = Primitive::LINES;
    cfg.attribute = Attribute::EDGE;
    cfg.colormapping = Viz::Colormapping::CUSTOM_INDEX_OBJECT;
    cfg.custom_index_color_fn = [](const Model&, int) { return Color::black(); };
    cfg.backside_alpha = 1.0f;
    cfg.cull_backface = false;
    cfg.shading = Shading::FLAT;
    return cfg;
}

Viz Viz::create_default_selected_facet()
{
    Viz cfg;
    cfg.viz_name = "SelectedFacet";
    cfg.primitive = Viz::Primitive::TRIANGLES;
    cfg.colormapping = Viz::Colormapping::UNIFORM;
    cfg.uniform_color = Color(1, 0, 0, 0.25f);
    cfg.shading = Viz::Shading::FLAT;
    cfg.filter_global = Viz::Filter::SHOW_ALL;
    cfg.attribute = Viz::Attribute::VERTEX;
    cfg.custom_sub_buffer_id = "_selected";
    return cfg;
}

Viz Viz::create_default_selected_edge()
{
    Viz cfg;
    cfg.viz_name = "SelectedEdge";
    cfg.primitive = Viz::Primitive::LINES;
    cfg.colormapping = Viz::Colormapping::UNIFORM;
    cfg.uniform_color = Color(1, 0, 0, 1.0f);
    cfg.shading = Viz::Shading::FLAT;
    cfg.filter_global = Viz::Filter::SHOW_ALL;
    cfg.attribute = Viz::Attribute::EDGE;
    cfg.custom_sub_buffer_id = "_selected";
    return cfg;
}

Viz Viz::create_default_selected_vertex()
{
    Viz cfg;
    cfg.viz_name = "SelectedVertex";
    cfg.primitive = Viz::Primitive::POINTS;
    cfg.colormapping = Viz::Colormapping::UNIFORM;
    cfg.uniform_color = Color(1, 0, 0, 1);
    cfg.shading = Viz::Shading::FLAT;
    cfg.filter_global = Viz::Filter::SHOW_ALL;
    cfg.attribute = Viz::Attribute::VERTEX;
    cfg.custom_sub_buffer_id = "_selected";
    return cfg;
}

Viz Viz::create_uniform_color(
    const std::string& viz_name,
    Primitive primitive,
    Color uniform_color /*= Color::random()*/,
    Shading shading /*= Shading::FLAT*/)
{
    Viz cfg;
    cfg.viz_name = viz_name;
    cfg.primitive = primitive;
    cfg.colormapping = Colormapping::UNIFORM;
    cfg.uniform_color = uniform_color;
    cfg.shading = shading;
    return cfg;
}

Viz Viz::create_indexed_colormapping(
    const std::string& viz_name,
    Attribute attribute,
    Primitive primitive,
    IndexColorFunc fn_model_and_index_to_color,
    Shading shading /*= Shading::FLAT*/)
{
    Viz cfg;
    cfg.viz_name = viz_name;
    cfg.primitive = primitive;
    cfg.attribute = attribute;
    cfg.shading = shading;
    cfg.colormapping = Colormapping::CUSTOM;
    cfg.custom_index_color_fn = fn_model_and_index_to_color;
    return cfg;
}

Viz Viz::create_attribute_colormapping(
    const std::string& viz_name,
    Attribute attribute,
    std::string attribute_name,
    Primitive primitive,
    AttribColorFunc fn_attribvalue_to_color,
    Shading shading /*= Shading::FLAT*/)
{
    Viz cfg;
    cfg.viz_name = viz_name;
    cfg.primitive = primitive;
    cfg.attribute = attribute;
    cfg.attribute_name = attribute_name;
    cfg.shading = shading;
    cfg.colormapping = Colormapping::CUSTOM;
    cfg.custom_attrib_color_fn = fn_attribvalue_to_color;
    return cfg;
}

Viz Viz::create_objectid(const std::string& viz_name, Filter filter)
{
    Viz cfg;
    cfg.attribute = Viz::Attribute::NONE;
    cfg.viz_name = viz_name;
    cfg.colormapping = Viz::Colormapping::CUSTOM_INDEX_OBJECT;
    cfg.custom_index_color_fn = [](const Model&, int index) {
        return Color::integer_to_color(index + 1);
    };
    cfg.shading = Viz::Shading::FLAT;
    cfg.primitive = Viz::Primitive::TRIANGLES;
    cfg.fbo_config.create_color = true;
    cfg.fbo_config.create_depth = true;
    cfg.backside_alpha = 1.0f;
    cfg.cull_backface = false;
    cfg.filter_global = filter;
    cfg.filter_local = Filter::SHOW_ALL;
    return cfg;
}


template <typename Dependency, typename FactoryFunc>
Dependency* initialize_dependency(RenderPipeline& pipeline, const FactoryFunc& fn)
{
    auto pass =
        reinterpret_cast<Dependency*>(pipeline.get_pass(default_render_pass_name<Dependency>()));

    if (pass) {
        assert(dynamic_cast<Dependency*>(pass));
        return pass;
    }

    pass = reinterpret_cast<Dependency*>(pipeline.add_pass(fn()));
    assert(dynamic_cast<Dependency*>(pass));
    return pass;
}

RenderPass<Viz::PassData>* Viz::add_to(RenderPipeline& pipeline, Resource<CommonPassData> common) const
{
    Viz config = *this;
    std::string validation_error;
    if (!config.validate(validation_error)) {
        throw std::logic_error(validation_error.c_str());
    }


    // If there's shading - add shadow map pass
    ShadowMapPass* shadow_map_pass = nullptr;
    if (config.shading != Shading::FLAT) {
        shadow_map_pass = initialize_dependency<ShadowMapPass>(pipeline, [&]() {
            return create_shadow_map_pass(common);
        });
    }

    // If there's PBR - add brdf lut precomputation pass
    BRDFLUTPass* brdflut_pass = nullptr;
    if (config.shading == Shading::PBR) {
        brdflut_pass = initialize_dependency<BRDFLUTPass>(pipeline, [&]() {
            return create_BRDFLUT_Pass(common);
        });

        // Make sure skybox is initialized
        initialize_dependency<SkyboxPass>(pipeline, [&]() { return create_skybox_pass(common); });
    }


    auto pass = std::make_unique<RenderPass<Viz::PassData>>(
        config.viz_name,
        [config, shadow_map_pass, brdflut_pass, common](
            Viz::PassData& data, OptionSet& options, RenderResourceBuilder& builder) {
            // Copy common pass data (references to camera, scene, etc.)
            data.common = common;


            /*
                Selection & filtering
            */
            data.filter_global = config.filter_global;
            data.filter_local = config.filter_local;

            /*
                FBO Configuration
            */

            configure_fbo(builder, config, data);

            /*
                Shader configuration
            */
            ShaderDefines def;

            // Decide which shader
            std::string shader_path = "";
            switch (config.primitive) {
            case Primitive::POINTS: shader_path = "points/points.shader"; break;
            case Primitive::LINES:
                if (config.attribute == Attribute::EDGE)
                    shader_path = "lines/edge_to_line.shader";
                else
                    shader_path = "lines/triangle_to_lines.shader";

                if (config.replace_with_bounding_box) {
                    shader_path = "surface/simple.shader";
                }

                break;
            case Primitive::TRIANGLES:
                if (config.attribute == Attribute::EDGE) {
                    shader_path = "surface/edge_split_surface.shader";
                }
                else {
                    shader_path = "surface/surface.shader";
                }
                break;
            }
            if (shader_path == "") {
                throw std::logic_error("Could not choose shader, unknown primitive");
            }

            


            switch (config.shading) {
            case Shading::FLAT: def.push_back({"SHADING_FLAT", ""}); break;
            case Shading::PHONG: def.push_back({"SHADING_PHONG", ""}); break;
            case Shading::PBR: def.push_back({"SHADING_PBR", ""}); break;
            default: break;
            }

            if (config.shading != Shading::FLAT) {
                assert(shadow_map_pass);
                data.emitters = builder.read(shadow_map_pass->get_data().emitters);
            }

            if (config.shading == Shading::PBR) {
                assert(brdflut_pass);
                data.brdflut = builder.read(brdflut_pass->get_data().brdf_lut_output);
            }


            /*
                Runtime options
            */

            options.add<float>("Backface Alpha", config.backside_alpha, 0.0f, 1.0f);

            if (config.colormapping == Colormapping::UNIFORM) {
                options.add<Color>("Uniform Color", config.uniform_color);
            }
            if (config.primitive == Primitive::POINTS) {
                options.add<float>("Point Size", 10.0f, 0.0f);
            }
            if (config.primitive == Primitive::LINES) {
                if (config.attribute == Attribute::EDGE) {
                    options.add<float>("Line Width", 3.0f, 0.0f);
                }
                else {
                    options.add<float>("Line Width", 0.0f, 0.0f);
                }
            }

            // Create shader resource
            auto shader_name = shader_path;
            for (auto d : def) shader_name += "-D" + d.first;

            data.shader = builder.create<Shader>(shader_name,
                ShaderResourceParams{ShaderResourceParams::Tag::VIRTUAL_PATH, shader_path, "", def});
        },
        [config](const Viz::PassData& data, const OptionSet& options) {
            auto& scene = *data.common->scene;

            //Use custom fbo if set, otherwise use final output fbo
            auto& fbo =
                (data.target_fbo.has_value()) ? (*data.target_fbo) : (*data.common->final_output_fbo);

            auto& camera = *data.common->camera;
            auto& shader = *data.shader;
            auto& pass_counter = *data.common->pass_counter;
            auto& global_selection = (*data.common->selection_global).get_persistent();

            pass_counter += 1;

            /*
                Build a list of objects to render
            */
            std::vector<Model*> models;
            for (auto model : scene.get_models()) {
                auto model_ptr = model;
                if (!model_ptr->is_visible()) continue;

                const bool in_local = data.filter_local == Filter::SHOW_ALL ||
                                (data.filter_local == Filter::SHOW_SELECTED &&
                                          data.selection_local.has(model_ptr)) ||
                                (data.filter_local == Filter::HIDE_SELECTED &&
                                          !data.selection_local.has(model_ptr));

                const bool in_global = data.filter_global == Filter::SHOW_ALL ||
                                (data.filter_global == Filter::SHOW_SELECTED &&
                                           global_selection.has(model_ptr)) ||
                                (data.filter_global == Filter::HIDE_SELECTED &&
                                           !global_selection.has(model_ptr));

                if (in_local && in_global) {
                    models.push_back(model_ptr);
                }
            }

            //Create local GL scope
            GLScope gl;
            utils::render::set_render_pass_defaults(gl);

            // Bind target fbo and shader
            fbo.bind();
            shader.bind();

            // If this viz creates it's own fbo, clear it now and optionally resize
            if (config.fbo_config.create_color || config.fbo_config.create_depth) {
                fbo.resize_attachments(int(camera.get_window_width()), int(camera.get_window_height()));
                gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
                gl(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }

            /*
                Layering
            */
            utils::render::set_render_layer(gl, pass_counter);

            /*
                Assign global uniforms (not related to rendered model)
            */
            int tex_units_global = 0;
            {
                // Camera position
                shader["cameraPos"] = camera.get_position();

                // Primitive size
                if (config.primitive == Primitive::POINTS) {
                    shader["point_size"] = options.get<float>("Point Size");
                } else if (config.primitive == Primitive::LINES) {
                    shader["line_width"] = options.get<float>("Line Width");
                }

                // Shading uniforms and uniform data
                if (config.shading != Shading::FLAT) {
                    utils::render::set_emitter_uniforms(shader, *data.emitters, tex_units_global);
                }

                // PBR uniforms
                if (config.shading == Shading::PBR) {
                    utils::render::set_pbr_uniforms(shader, *data.brdflut, tex_units_global);
                }

            }


            int model_index = -1;
            for (auto model_ptr : models) {
                model_index++;

                if (!model_ptr->is_visible()) continue;

                if (!model_ptr->is_visualizable() && !((config.shading == Shading::PBR || config.shading == Shading::PHONG) &&
                        config.primitive == Primitive::TRIANGLES)) 
                    continue;


                auto mesh_model_ptr = dynamic_cast<MeshModelBase*>(model_ptr);
                if (!mesh_model_ptr) continue;
                

                
                auto& model = *mesh_model_ptr;
                MeshBuffer* buffer = nullptr;


                if (config.replace_with_bounding_box) {
                    buffer = &MeshBuffer::cube(config.primitive == Primitive::LINES);
                }
                else {
                    buffer = model.get_buffer().get();
                }
                if (!buffer) continue;

                /*
                    Setup update callback if needed
                */
                if (config.colormapping == Colormapping::CUSTOM &&
                    data.callback_setup.count(model_ptr) == 0) {

                    model_ptr->add_callback<Model::OnChange>(
                        [&](Model& cb_model) { data.color_updated.erase(&cb_model); });

                    model_ptr->add_callback<Model::OnDestroy>([&](Model& cb_model) {
                        data.color_updated.erase(&cb_model);
                        data.callback_setup.erase(&cb_model);
                    });

                    data.callback_setup.insert(&model);
                }

                /*
                    Using custom color buffer
                    Check whether we updated already
                */
                if (config.colormapping == Colormapping::CUSTOM &&
                    data.color_updated.count(model_ptr) == 0) {

                    auto& color_buffer =
                        buffer->get_sub_buffer(MeshBuffer::SubBufferType::COLOR, config.viz_name);

                    const auto& pm = model.get_proxy_mesh();

                    ColorMatrix cm;

                    if (config.custom_index_color_fn) {
                        cm = create_color_matrix(config.attribute,
                            pm,
                            [&](int index) { return config.custom_index_color_fn(model, index); });
                    }
                    else if (config.custom_attrib_color_fn) {
                        auto attrib_array =
                            get_attrib_wrapper(model, config.attribute, config.attribute_name);

                        if (attrib_array) {
                            cm = create_color_matrix(
                                config.attribute, pm, [&](int index) {
                                    return config.custom_attrib_color_fn(
                                        model, attrib_array->row(index));
                                });
                        }
                    }
                    if (cm.rows() > 0) {

                        switch (config.attribute) {
                        case Attribute::VERTEX:
                            MeshBufferFactory::upload_vertex_attribute(color_buffer, cm, pm);
                            break;
                        case Attribute::EDGE:
                            MeshBufferFactory::upload_edge_attribute(color_buffer, cm, pm);
                            break;
                        case Attribute::FACET:
                            MeshBufferFactory::upload_facet_attribute(color_buffer, cm, pm);
                            break;
                        case Attribute::CORNER:
                            MeshBufferFactory::upload_corner_attribute(color_buffer, cm, pm);
                            break;
                        default: break;
                        }

                        data.color_updated.insert(model_ptr);
                    }
                }

                /*
                    Create object local GL scope
                */
                GLScope gl_object;

                //Apply viewport transform
                auto object_cam = camera.transformed(model.get_viewport_transform());

                //Resize viewport
                gl_object(glViewport,
                    int(object_cam.get_window_origin().x()),
                    int(object_cam.get_window_origin().y()),
                    int(object_cam.get_window_width()),
                    int(object_cam.get_window_height()));

                //Upload transformations
                shader["PV"] = object_cam.get_PV();
                shader["PVinv"] = object_cam.get_PV().inverse().eval();
                shader["screen_size"] = object_cam.get_window_size();
                if (config.replace_with_bounding_box) {
                    shader["M"] = model.get_bounds().get_cube_transform();
                }
                else {
                    shader["M"] = model.get_transform();
                }
                shader["NMat"] = normal_matrix(model.get_transform());

                auto sub_buffer_id = (config.custom_sub_buffer_id.length() > 0)
                    ? config.custom_sub_buffer_id : MeshBuffer::default_sub_id();



                /*
                    Decide whether to use color attribute or uniform color
                */
                shader["has_color_attrib"] = false;
                shader["uniform_color"] = Eigen::Vector4f(-1,-1,-1,-1);

                if (config.colormapping == Colormapping::CUSTOM) {
                    sub_buffer_id = config.viz_name;

                    auto color_buf =
                        buffer->try_get_sub_buffer(MeshBuffer::SubBufferType::COLOR, sub_buffer_id);
                    if (color_buf && color_buf->count > 0) {
                        shader["has_color_attrib"] = true;
                    }
                }
                else if (config.colormapping == Colormapping::CUSTOM_INDEX_OBJECT) {
                    shader["uniform_color"] =
                        config.custom_index_color_fn(model, model_index).to_vec4();
                }
                else if (config.colormapping == Colormapping::UNIFORM) {
                    shader["uniform_color"] = options.get<Color>("Uniform Color").to_vec4();
                }


                auto primitive_type = get_primitive_type(config.primitive);

                //Override if we're using triangle to line shader (baerenzten)
                if (!config.replace_with_bounding_box &&
                    config.primitive == Viz::Primitive::LINES &&
                    config.attribute != Attribute::EDGE) {
                    primitive_type = MeshBuffer::Primitive::TRIANGLES;
                }

                //Select sub buffers for color
                MeshBuffer::SubBufferSelection sub_buffer_sel = {
                    {MeshBuffer::SubBufferType::COLOR, sub_buffer_id}
                };

                // Select sub buffers for indices
                if (config.custom_sub_buffer_id.length() > 0) {
                    sub_buffer_sel[MeshBuffer::SubBufferType::INDICES] =
                        config.custom_sub_buffer_id;
                } else {
                    switch (primitive_type) {
                    case MeshBuffer::Primitive::POINTS:
                        sub_buffer_sel[MeshBuffer::SubBufferType::INDICES] =
                            MeshBuffer::vertex_index_id();
                        break;
                    case MeshBuffer::Primitive::LINES:
                        sub_buffer_sel[MeshBuffer::SubBufferType::INDICES] =
                            MeshBuffer::edge_index_id();
                        break;
                    default: break;
                    }
                }

                const float backface_alpha = options.get<float>("Backface Alpha");

                auto bind_material = [&](const Material & mat){
                    int tex_units = tex_units_global;
                    utils::render::set_map(shader, "baseColor", mat, tex_units);
                    utils::render::set_map(shader, "roughness", mat, tex_units);
                    utils::render::set_map(shader, "metallic", mat, tex_units);
                    utils::render::set_map(shader, "normal", mat, tex_units);
                    shader["material.opacity"] = mat["opacity"].value.x();
                };

                //Setup render call
                auto render = [&](){

                    //If shading is On and materials exist
                    if (config.shading != Shading::FLAT && model.get_materials().size() > 0) {

                        //Render multi-material object
                        if (model.get_materials().size() > 1 && primitive_type == MeshBuffer::Primitive::TRIANGLES) {
                            for (const auto& mat : model.get_materials()) {

                                //Bind material
                                bind_material(*(mat.second));

                                //Switch indexing to material index sub buffer
                                sub_buffer_sel[MeshBuffer::SubBufferType::INDICES] =
                                        MeshBuffer::material_index_id(mat.first);
                                
                                buffer->render(primitive_type, sub_buffer_sel);
                            }
                        } 
                        // Single material or shading of non-triangle primitives
                        else {
                            bind_material(*model.get_materials().begin()->second);
                            buffer->render(primitive_type, sub_buffer_sel);
                        }
                    } else {
                        //No shading/material
                        buffer->render(primitive_type, sub_buffer_sel);
                    }
                    
                };

                /*
                    Two pass render - front and back faces 
                */
                if (backface_alpha < 1.0f && model_ptr->is_visualizable()) {
                    gl_object(glEnable, GL_CULL_FACE);

                    // Render backfacing first
                    gl_object(glCullFace, GL_FRONT);
                    gl_object(glDisable, GL_DEPTH_TEST);
                    gl_object(glDepthMask, GL_FALSE);

                    shader["alpha_multiplier"] = backface_alpha;
                    shader["cull_backface"] = false;

                    render();

                    // Render front facing on top
                    gl_object(glEnable, GL_DEPTH_TEST);
                    gl_object(glDepthMask, GL_TRUE);
                    gl_object(glCullFace, GL_BACK);
                    shader["alpha_multiplier"] = 1.0f;
                    shader["cull_backface"] = true;

                    render();
                }
                /*
                    One-pass render 
                */
                else {
                    if (config.cull_backface) {
                        gl_object(glEnable, GL_CULL_FACE);
                    }
                    else {
                        gl_object(glDisable, GL_CULL_FACE);
                    }
                    shader["alpha_multiplier"] = 1.0f;
                    shader["cull_backface"] = true;
                    render();
                    //buffer->render(primitive_type, sub_buffer_sel);
                }
            }

            shader.unbind();
        }/*,
        [](Viz::PassData& data, OptionSet& options) { // CLEANUP
            for (auto model : data.callback_setup) {
                // TODO
                // model->clear_callback()<OnChange>(previously created callback);
            }
        }*/);

    auto pass_ptr = pass.get();
    pipeline.add_pass(std::move(pass));


    return pass_ptr;
}


std::string Viz::to_string(Attribute att)
{
    switch (att) {
    case Attribute::NONE: return "NONE";
    case Attribute::VERTEX: return "VERTEX";
    case Attribute::EDGE: return "EDGE";
    case Attribute::FACET: return "FACET";
    case Attribute::CORNER: return "CORNER";
    default: break;
    }
    return "";
}

std::string Viz::to_string(Primitive prim)
{
    switch (prim) {
    case Primitive::POINTS: return "POINTS";
    case Primitive::LINES: return "LINES";
    case Primitive::TRIANGLES: return "TRIANGLES";
    }
    return "";
}

std::string Viz::to_string(Shading shading)
{
    switch (shading) {
    case Shading::FLAT: return "FLAT";
    case Shading::PHONG: return "PHONG";
    case Shading::PBR: return "PBR";
    }
    return "";
}

std::string Viz::to_string(Colormapping colormapping)
{
    switch (colormapping) {
    case Colormapping::UNIFORM: return "UNIFORM";
    case Colormapping::TEXTURE: return "TEXTURE";
    case Colormapping::CUSTOM: return "CUSTOM";
    case Colormapping::CUSTOM_INDEX_OBJECT: return "CUSTOM_INDEX_OBJECT";
    }
    return "";
}

std::string Viz::to_string(Filter filter)
{
    switch (filter) {
    case Filter::SHOW_ALL: return "SHOW_ALL";
    case Filter::SHOW_SELECTED: return "SHOW_SELECTED";
    case Filter::HIDE_SELECTED: return "HIDE_SELECTED";
    }
    return "";
}

} // namespace ui
} // namespace lagrange
