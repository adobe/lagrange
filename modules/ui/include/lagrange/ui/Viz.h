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
#include <lagrange/ui/Color.h>
#include <lagrange/ui/Model.h>
#include <lagrange/ui/RenderPipeline.h>
#include <lagrange/ui/Selection.h>
#include <lagrange/ui/default_resources.h>
#include <lagrange/ui/render_passes/common.h>

#include <string>

namespace lagrange {
namespace ui {


///
/// Visualization configuration class
/// Used to create a render pass to visualize models.
///
class Viz
{
public:
    ///
    /// Which attribute is being visualized.
    /// Affects the indexing.
    /// If none selected, uses uniform or texture color.
    ///
    enum class Attribute { NONE, VERTEX, EDGE, FACET, CORNER };

    ///
    /// What primitive is being rendered. Correspods to GL_POINTS/LINES/TRIANGLES
    ///
    enum class Primitive { POINTS, LINES, TRIANGLES };

    ///
    /// What kind of shading us used.
    ///
    /// Shading::FLAT: no lighting, but interpolates color where applicable (e.g. Attribute = VERTEX, but not
    ///   Attribute = FACET).
    ///
    /// Shading::PHONG: phong reflectance model
    ///
    /// Shading::PBR: Physically based render
    ///
    enum class Shading { FLAT, PHONG, PBR };

    ///
    /// Color mapping from attribute -> Color
    ///
    ///    Colormapping::UNIFORM applies uniform color (which can be changed later in render pass options)
    ///
    ///    Colormapping::TEXTURE uses texture color
    ///
    ///    Colormapping::CUSTOM uses custom color function to assign color based on attribute index (IndexColorFunc per-vertex/edge/facet/corner) or value (AttribColorFunc)
    ///
    ///    Colormapping::CUSTOM_INDEX_OBJECT applies IndexColorFunc where index is object's index
    ///
    ///
    enum class Colormapping { UNIFORM, TEXTURE, CUSTOM, CUSTOM_INDEX_OBJECT };

    ///
    /// User provided function type for indexed colormapping
    ///
    using IndexColorFunc = std::function<Color(const Model& model, int index)>;

    ///
    /// Value proxy used in AttribColorFunc
    ///
    using AttribValue = Eigen::Matrix<double, 1, Eigen::Dynamic, Eigen::RowMajor, 1, 8>;

    ///
    /// User provided function type for colormapping based on attribute value
    ///
    using AttribColorFunc = std::function<Color(const Model& model, const Viz::AttribValue& value)>;


    ///
    /// Which objects in the scene to show
    ///
    ///    Filter::SHOW_ALL shows all compatible objects
    ///
    ///    Filter::SHOW_SELECTED/Filter::HIDE_SELECTED filter by objects;
    ///
    enum class Filter { SHOW_ALL, SHOW_SELECTED, HIDE_SELECTED };

    ///
    /// By default, uses context's default VBO.
    /// Can specify to create own (for rendering to texture).
    ///
    struct FBOConfig
    {
        bool create_color = false;
        bool create_depth = false;
        Resource<FrameBuffer> target_fbo;
    };


    ///
    /// Visualization configuration
    ///

    /// Unique name
    std::string viz_name = "Unnamed Viz";

    ///
    /// Which attribute to visualize
    /// Applicable only when using Colormapping::CUSTOM and attribute != Attribute::NONE
    ///
    std::string attribute_name = "";

    /// Indexing attribute
    Attribute attribute = Attribute::NONE;

    /// Filter global selection (using CommonPassData::selection_global)
    Filter filter_global = Filter::SHOW_ALL;

    /// Filter local selection (using Viz::PassData::selection_local)
    Filter filter_local = Filter::SHOW_ALL;

    /// How to render the data
    Primitive primitive = Primitive::POINTS;

    Shading shading = Shading::FLAT;
    Colormapping colormapping = Colormapping::UNIFORM;

    /// Intermediate parameters
    IndexColorFunc custom_index_color_fn = nullptr;
    AttribColorFunc custom_attrib_color_fn = nullptr;
    Color uniform_color = Color::random(0);

    /// Advanced parameter
    FBOConfig fbo_config;


    /// Automatic selected if alpha < 0
    float backside_alpha = -1.0f;
    bool cull_backface = true;
    bool replace_with_bounding_box = false;

    ///
    /// Custom sub buffer id
    /// _selected, _hovered indexing
    /// \todo color buffer for soft selection
    std::string custom_sub_buffer_id;

    /// \todo Texture map to choose (string -> mat -> map)


    ///
    /// Initializes and validates the configuration
    ///
    bool validate(std::string & error_out);


    ///
    /// Default Physically Based Render pass
    ///
    /// Initializes Skybox and ShadowMap passes
    ///
    /// @return Viz
    ///
    static Viz create_default_pbr();

    ///
    /// Default Phong pass
    ///
    /// Initializes Skybox and ShadowMap passes
    ///
    /// @return Viz
    ///
    static Viz create_default_phong();

    ///
    /// Default edge pass
    ///
    /// @return Viz
    ///
    static Viz create_default_edge();

    ///
    /// Default vertex pass
    ///
    /// @return Viz
    ///
    static Viz create_default_vertex();

    ///
    /// Default bounding box pass
    ///
    /// @return Viz
    ///
    /// \todo Render quads instead of triangles
    ///
    static Viz create_default_bounding_box();

    ///
    /// Default selected facet pass
    ///
    /// @return Viz
    ///
    static Viz create_default_selected_facet();

    ///
    /// Default selected edge pass
    ///
    /// @return Viz
    ///
    static Viz create_default_selected_edge();

    ///
    /// Default selected vertex pass
    ///
    /// @return Viz
    ///
    static Viz create_default_selected_vertex();


    ///
    /// Renders primitives using a specified random color
    ///
    /// @param[in] viz_name         Unique vizualization name
    /// @param[in] primitive        How to render
    /// @param[in] uniform_color    Color
    /// @param[in] shading          Type of shading
    ///
    /// @return Viz
    ///
    static Viz create_uniform_color(const std::string& viz_name,
        Primitive primitive,
        Color uniform_color = Color::random(0),
        Shading shading = Shading::FLAT);

    ///
    /// Assigns color to each rendered attribute using provided function.
    ///
    ///    Indexing depends on chosen attribute
    ///
    ///        VERTEX will call IndexColorFunc with index=<0,mesh.get_num_vertices());
    ///
    ///        EDGE will call IndexColorFunc with index=<0,mesh.get_num_edges());
    ///
    ///        FACET will call IndexColorFunc with index=<0,mesh.get_num_facets());
    ///
    ///        CORNER will call IndexColorFunc with index=<0,mesh.get_vertex_per_facet() *
    ///   mesh.get_num_facets());
    ///
    /// @param[in] viz_name                     Unique vizualization name
    /// @param[in] attribute                    Type of attribute to visualize
    /// @param[in] primitive                    How to render
    /// @param[in] fn_model_and_index_to_color  Function taking a Model and index and returning color
    /// @param[in] shading                      Type of shading
    ///
    /// @return Viz
    ///
    static Viz create_indexed_colormapping(const std::string& viz_name,
        Attribute attribute,
        Primitive primitive,
        IndexColorFunc fn_model_and_index_to_color,
        Shading shading = Shading::FLAT);

    ///
    /// Use AttribColorFunc to assign Color based on a attribute value
    ///
    ///        VERTEX calls mesh.get_vertex_attribute()
    ///
    ///        EDGE calls mesh.get_edge_attribute()
    ///
    ///        FACET calls mesh.get_facet_attribute()
    ///
    ///        CORNER calls mesh.get_corner_attribute()
    ///
    ///    All attributes are converted to AttribValue (a dynamic <double> eigen matrix)
    ///
    /// @param[in] viz_name                 Unique vizualization name
    /// @param[in] attribute                Type of attribute to visualize
    /// @param[in] attribute_name           Which attribute to visualize
    /// @param[in] primitive                How to render
    /// @param[in] fn_attribvalue_to_color  Function taking a Model and AttributeValue and returning color
    /// @param[in] shading                  Type of shading
    ///
    /// @return Viz
    ///

    static Viz create_attribute_colormapping(const std::string& viz_name,
        Attribute attribute,
        std::string attribute_name,
        Primitive primitive,
        AttribColorFunc fn_attribvalue_to_color,
        Shading shading = Shading::FLAT);

    ///
    /// Renders each object with unique id
    /// Used for outline render
    ///
    /// @param[in] viz_name         Unique visualization name
    /// @param[in] global_filter    Global filter (global selection)
    ///
    /// @return Viz
    ///
    static Viz create_objectid(const std::string& viz_name, Filter global_filter);


    ///
    /// Data of the resulting render pass
    ///
    struct PassData
    {
        Resource<Shader> shader;
        Resource<CommonPassData> common;

        /// Selection specific to this viz
        Selection<BaseObject*> selection_local;
        Viz::Filter filter_global;
        Viz::Filter filter_local;

        /// Optionally used for shading (phong or pbr)
        Resource<std::vector<EmitterRenderData>> emitters = {};

        /// Optionally used for pbr (i.e., specular ibl)
        Resource<Texture> brdflut = {};

        /// Target color buffer
        Resource<Texture> color_buffer = {};

        /// Target depth buffer
        Resource<Texture> depth_buffer = {};

        /// Target framebuffer object
        Resource<FrameBuffer> target_fbo = {};

        ///
        /// Tracks whether we uploaded color or not
        /// Gets invalidated whenever OnModelChange
        /// \todo get rid of this
        ///
        mutable std::unordered_set<Model*> color_updated;

        ///
        /// Tracks whether we set up a callback or not for OnModelChange
        /// \todo get rid of this
        ///
        mutable std::unordered_set<Model*> callback_setup;
    };


    ///
    /// Create and add render pass to a pipeline
    /// @param[in] pipeline Target pipeline
    /// @param[in] common Common pass data (with camera, scene, etc. reference)
    ///
    /// @return Pointer to added render pass
    ///
    RenderPass<Viz::PassData>* add_to(RenderPipeline& pipeline, Resource<CommonPassData> common) const;

    static std::string to_string(Attribute att);
    static std::string to_string(Primitive prim);
    static std::string to_string(Shading shading);
    static std::string to_string(Colormapping colormapping);
    static std::string to_string(Filter filter);


};

} // namespace ui
} // namespace lagrange
