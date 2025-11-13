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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/subdivision/legacy/mesh_subdivision.h>
#endif

#include <lagrange/SurfaceMesh.h>

#include <optional>

namespace lagrange::subdivision {

/// @addtogroup module-subdivision
/// @{

/// Subdivision scheme.
enum class SchemeType {
    Bilinear, /// Bilinear subdivision scheme. Useful to subdivide a mesh prior to applying a
    /// displacement map.
    CatmullClark, ///< Catmull-Clark is more widely used and suited to quad-dominant meshes.
    Loop, ///< Loop is preferred for (and requires) purely triangulated meshes.
};

///
/// Boundary Interpolation Rules.
///
/// Boundary interpolation rules control how subdivision and the limit surface behave for faces
/// adjacent to boundary edges and vertices.
///
enum class VertexBoundaryInterpolation {
    /// No boundary edge interpolation is applied by default; boundary faces are tagged as holes so
    /// that the boundary vertices continue to support the adjacent interior faces, but no surface
    /// corresponding to the boundary faces is generated; boundary faces can be selectively
    /// interpolated by sharpening all boundary edges incident the vertices of the face.
    None,

    /// A sequence of boundary vertices defines a smooth curve to which the limit surface along
    /// boundary faces extends.
    EdgeOnly,

    /// Similar to edge-only but the smooth curve resulting on the boundary is made to interpolate
    /// corner vertices (vertices with exactly one incident face)
    EdgeAndCorner,
};

///
/// Face-varying Interpolation Rules.
///
/// Face-varying interpolation rules control how face-varying data is interpolated both in the
/// interior of face-varying regions (smooth or linear) and at the boundaries where it is
/// discontinuous (constrained to be linear or "pinned" in a number of ways). Where the topology is
/// continuous and the interpolation chosen to be smooth, the behavior of face-varying interpolation
/// will match that of the vertex interpolation.
///
enum class FaceVaryingInterpolation {
    None, ///< Smooth everywhere the mesh is smooth
    CornersOnly, ///< Linearly interpolate (sharpen or pin) corners only
    CornersPlus1, ///< CornersOnly + sharpening of junctions of 3 or more regions
    CornersPlus2, ///< CornersPlus2 + sharpening of darts and concave corners
    Boundaries, ///< Linear interpolation along all boundary edges and corners
    All, ///< Linear interpolation everywhere (boundaries and interior)
};

///
/// Topology refinement method.
///
enum class RefinementType {
    /// Each facet is subdivided a fixed number of time.
    Uniform,

    // TODO: Implement these. This involves creating a Far::PatchTable, extracting the subdivided
    // facets, and welding T-junctions...
    /// Each facet is subdivided adaptively according to the mesh curvature. Highly curved regions
    /// will be refined more. Best suited for rendering applications.
    // CurvatureAdaptive,

    /// Each facet is tessellated based on a target edge length and max subdiv level. Best suited to
    /// produce meshes with a uniform edge length when the input mesh has varying facet sizes.
    EdgeAdaptive,
};

///
/// Helper class to select which attributes to interpolate. By default, all compatible attributes
/// will be smoothly interpolated (i.e. using "vertex" weights for per-vertex attributes, and using
/// "face-varying" weights for indexed attributes).
///
/// An attribute can be interpolated if:
/// - Its value type is either `float` or `double`.
/// - Its element type is either Vertex or Indexed.
///
class InterpolatedAttributes
{
public:
    ///
    /// Selection tag.
    ///
    enum class SelectionType { All, None, Selected };

public:
    ///
    /// Interpolate all compatible attribute.
    ///
    /// @return     The interpolated attributes configuration.
    ///
    static InterpolatedAttributes all() { return {SelectionType::All, {}, {}}; }

    ///
    /// Do not interpolate any attribute.
    ///
    /// @return     The interpolated attributes configuration.
    ///
    static InterpolatedAttributes none() { return {SelectionType::None, {}, {}}; }

    ///
    /// Only interpolate the specified attributes. Will raise an error if a selected attribute
    /// cannot be interpolated (because of an incompatible value type or element type).
    ///
    /// @param[in]  smooth  Per-vertex or indexed attribute ids to smoothly interpolate.
    /// @param[in]  linear  Per-vertex attribute ids to smoothly interpolate.
    ///
    /// @return     The interpolated attributes configuration.
    ///
    static InterpolatedAttributes selected(
        std::vector<AttributeId> smooth,
        std::vector<AttributeId> linear = {})
    {
        return {SelectionType::Selected, std::move(smooth), std::move(linear)};
    }

    ///
    /// Sets selection to all.
    ///
    void set_all() { *this = all(); }

    ///
    /// Sets selection to none.
    ///
    void set_none() { *this = none(); }

    ///
    /// Set selection to a specific list of attribte ids.
    ///
    /// @param[in]  smooth  Per-vertex or indexed attribute ids to smoothly interpolate.
    /// @param[in]  linear  Per-vertex attribute ids to smoothly interpolate.
    ///
    void set_selected(std::vector<AttributeId> smooth, std::vector<AttributeId> linear = {})
    {
        selection_type = SelectionType::Selected;
        smooth_attributes = std::move(smooth);
        linear_attributes = std::move(linear);
    }

    // Has to be kept public to use aggregate initialization...
public:
    /// Selection type.
    SelectionType selection_type = SelectionType::All;

    /// List of per-vertex or indexed attributes ids to smoothly interpolate (in OpenSubdiv terms,
    /// this corresponds to "vertex" weights for per-vertex attributes, and "face-varying" weights
    /// for indexed attributes). If selection_type is All, all attributes not specifically present
    /// in linear_attributes are considered "smooth".
    std::vector<AttributeId> smooth_attributes;

    /// List of per-vertex attributes ids to linearly interpolate (in OpenSubdiv terms, this
    /// corresponds to "varying" weights).
    std::vector<AttributeId> linear_attributes;
};

/// Mesh subdivision options.
struct SubdivisionOptions
{
    ///
    /// @name General Options
    /// @{

    /// Subvision scheme. If not provided, will use Loop for triangle meshes, and CatmullCark for
    /// quad-dominant meshes.
    std::optional<SchemeType> scheme;

    /// Number of subdivision level requested.
    unsigned num_levels = 1;

    /// How to refine the mesh topology.
    RefinementType refinement = RefinementType::Uniform;

    /// @}
    /// @name Adaptive tessellation options
    /// @{

    /// Maximum edge length for adaptive tessellation. If not specified, it is set to the longest
    /// edge length divided by num_levels.
    std::optional<float> max_edge_length;

    /// @}
    /// @name Interpolation Rules
    /// @{

    /// Vertex boundary interpolation rule.
    VertexBoundaryInterpolation vertex_boundary_interpolation =
        VertexBoundaryInterpolation::EdgeOnly;

    /// Face-varying interpolation rule.
    FaceVaryingInterpolation face_varying_interpolation = FaceVaryingInterpolation::None;

    /// Interpolate all data to the limit surface.
    bool use_limit_surface = false;

    /// @}
    /// @name Input Attributes To Interpolate
    /// @{

    ///
    /// List of attributes to interpolate. The selection can be either all (default), none, or
    /// specify a list of attribute ids to interpolate.
    ///
    /// @see        @ref InterpolatedAttributes
    ///
    InterpolatedAttributes interpolated_attributes;

    // TODO: Add face-uniform attributes (i.e. per-facet attributes), e.g. material_id.

    /// @}
    /// @name Input Element Tags
    /// @{

    /// Per-edge scalar attribute denoting edge sharpness. Sharpness values must be in [0, 1] (0 means
    /// smooth, 1 means sharp).
    std::optional<AttributeId> edge_sharpness_attr;

    /// Per-vertex scalar attribute denoting vertex sharpness (e.g. for boundary corners). Sharpness
    /// values must be in [0, 1] (0 means smooth, 1 means sharp).
    std::optional<AttributeId> vertex_sharpness_attr;

    /// Per-face integer attribute denoting face holes. A non-zero value means the facet is a hole.
    /// If a face is tagged as a hole, the limit surface will not be generated for that face.
    std::optional<AttributeId> face_hole_attr;

    /// @}
    /// @name Output Attributes
    /// @{

    ///
    /// [Adaptive subdivision only] Whether to preserve shared indices when interpolating indexed
    /// attributes. Turn this off if your input UVs are overlapping, or the output UVs will not be
    /// correctly interpolated.
    ///
    bool preserve_shared_indices = false;

    ///
    /// A newly computed per-vertex attribute containing the normals to the limit surface. Skipped
    /// if left empty.
    ///
    /// @note       It is strongly recommended to use limit normals only when interpolating
    ///             positions to the limit surface. Otherwise this can lead to visual artifacts if
    ///             the positions and the normals don't match.
    ///
    std::string_view output_limit_normals;

    ///
    /// A newly computed per-vertex attribute containing the tangents (first derivatives) to the
    /// limit surface. Skipped if left empty.
    ///
    /// @note       It is strongly recommended to use limit tangents only when interpolating
    ///             positions to the limit surface. Otherwise this can lead to visual artifacts if
    ///             the positions and the normals don't match.
    ///
    std::string_view output_limit_tangents;

    ///
    /// A newly computed per-vertex attribute containing the bitangents (second derivative) to the
    /// limit surface. Skipped if left empty.
    ///
    /// @note       It is strongly recommended to use limit bitangents only when interpolating
    ///             positions to the limit surface. Otherwise this can lead to visual artifacts if
    ///             the positions and the normals don't match.
    ///
    std::string_view output_limit_bitangents;

    /// @}
    /// @name Debugging options
    /// @{

    ///
    /// Validate topology of the subdivision surface. For debugging only.
    ///
    ///
    bool validate_topology = false;

    /// @}
};

///
/// Evaluates the subdivision surface of a polygonal mesh.
///
/// @param[in]  mesh     Input mesh to subdivide.
/// @param[in]  options  Subdivision options.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     Subdivided mesh.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> subdivide_mesh(
    const SurfaceMesh<Scalar, Index>& mesh,
    const SubdivisionOptions& options = {});

/// @}

} // namespace lagrange::subdivision
