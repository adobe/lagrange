/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include "bind_types.h"

#include <lagrange/combine_meshes.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_components.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_greedy_coloring.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_seam_edges.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/compute_vertex_valence.h>
#include <lagrange/mesh_cleanup/remove_duplicate_facets.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/mesh_cleanup/remove_null_area_facets.h>
#include <lagrange/mesh_cleanup/remove_short_edges.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/mesh_cleanup/resolve_nonmanifoldness.h>
#include <lagrange/mesh_cleanup/resolve_vertex_nonmanifoldness.h>
#include <lagrange/normalize_meshes.h>
#include <lagrange/orient_outward.h>
#include <lagrange/topology.h>
#include <lagrange/triangulate_polygonal_facets.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>
#include <vector>

namespace {

using namespace lagrange;
using namespace lagrange::js::bind;
using val = emscripten::val;

NormalWeightingType parse_weight_type(const std::string& s)
{
    if (s == "uniform") return NormalWeightingType::Uniform;
    if (s == "cornerTriangleArea") return NormalWeightingType::CornerTriangleArea;
    return NormalWeightingType::Angle;
}

} // namespace

EMSCRIPTEN_BINDINGS(lagrange_core_utilities)
{
    using namespace emscripten;

    // --- Normals ---

    function(
        "computeVertexNormal",
        +[](MeshType& mesh, val opts) {
            VertexNormalOptions o;
            if (!opts.isUndefined()) {
                auto wt = opts["weightType"];
                if (!wt.isUndefined()) o.weight_type = parse_weight_type(wt.as<std::string>());
                apply_opt(
                    opts,
                    "recomputeWeightedCornerNormals",
                    o.recompute_weighted_corner_normals);
                apply_opt(opts, "keepWeightedCornerNormals", o.keep_weighted_corner_normals);
                apply_opt(opts, "distanceTolerance", o.distance_tolerance);
            }
            compute_vertex_normal(mesh, std::move(o));
        });

    function("computeFacetNormal", +[](MeshType& m) { compute_facet_normal(m); });

    function(
        "computeNormal",
        +[](MeshType& mesh, Scalar feature_angle_threshold, val opts) {
            NormalOptions o;
            std::vector<Index> cone_verts;
            if (!opts.isUndefined()) {
                auto wt = opts["weightType"];
                if (!wt.isUndefined()) o.weight_type = parse_weight_type(wt.as<std::string>());
                apply_opt(opts, "recomputeFacetNormals", o.recompute_facet_normals);
                apply_opt(opts, "keepFacetNormals", o.keep_facet_normals);
                apply_opt(opts, "distanceTolerance", o.distance_tolerance);
                auto cv = opts["coneVertices"];
                if (!cv.isUndefined()) {
                    unsigned len = cv["length"].as<unsigned>();
                    cone_verts.reserve(len);
                    for (unsigned i = 0; i < len; ++i) {
                        cone_verts.push_back(cv[i].as<Index>());
                    }
                }
            }
            compute_normal(
                mesh,
                feature_angle_threshold,
                span<const Index>(cone_verts.data(), cone_verts.size()),
                std::move(o));
        });

    function(
        "computeTangentBitangent",
        +[](MeshType& mesh, val opts) {
            TangentBitangentOptions o;
            if (!opts.isUndefined()) {
                apply_opt(opts, "padWithSign", o.pad_with_sign);
                apply_opt(opts, "orthogonalizeBitangent", o.orthogonalize_bitangent);
                apply_opt(opts, "keepExistingTangent", o.keep_existing_tangent);
            }
            compute_tangent_bitangent(mesh, std::move(o));
        });

    // --- Mesh operations ---

    function("triangulatePolygonalFacets", +[](MeshType& m) { triangulate_polygonal_facets(m); });

    function(
        "combineMeshes",
        +[](val js_array, val opts) -> MeshType {
            unsigned len = js_array["length"].as<unsigned>();
            std::vector<MeshType> meshes;
            meshes.reserve(len);
            for (unsigned i = 0; i < len; ++i)
                meshes.push_back(js_array[i].as<MeshType>(emscripten::allow_raw_pointers()));
            bool preserve_attributes = true;
            apply_opt(opts, "preserveAttributes", preserve_attributes);
            return combine_meshes<Scalar, Index>(
                span<const MeshType>(meshes.data(), meshes.size()),
                preserve_attributes);
        });

    function(
        "computeComponents",
        +[](MeshType& mesh, val opts) -> size_t {
            ComponentOptions o;
            std::vector<Index> blockers;
            if (!opts.isUndefined()) {
                auto ct = opts["connectivityType"];
                if (!ct.isUndefined()) {
                    o.connectivity_type = ct.as<std::string>() == "vertex"
                                              ? ConnectivityType::Vertex
                                              : ConnectivityType::Edge;
                }
                auto bl = opts["blockerElements"];
                if (!bl.isUndefined()) {
                    unsigned len = bl["length"].as<unsigned>();
                    blockers.reserve(len);
                    for (unsigned i = 0; i < len; ++i) {
                        blockers.push_back(bl[i].as<Index>());
                    }
                }
            }
            if (blockers.empty()) {
                return compute_components(mesh, std::move(o));
            } else {
                return compute_components(
                    mesh,
                    span<const Index>(blockers.data(), blockers.size()),
                    std::move(o));
            }
        });

    function(
        "orientOutward",
        +[](MeshType& mesh, val opts) {
            OrientOptions o;
            apply_opt(opts, "positive", o.positive);
            orient_outward(mesh, o);
        });

    function("normalizeMesh", +[](MeshType& m) { normalize_mesh(m); });
    function("computeFacetArea", +[](MeshType& m) { compute_facet_area(m); });

    // --- Topology queries ---

    function("isManifold", +[](const MeshType& m) -> bool { return is_manifold(m); });
    function("isVertexManifold", +[](const MeshType& m) -> bool { return is_vertex_manifold(m); });
    function("isEdgeManifold", +[](const MeshType& m) -> bool { return is_edge_manifold(m); });
    function("isClosed", +[](const MeshType& m) -> bool { return is_closed(m); });
    function("computeEuler", +[](const MeshType& m) -> int { return compute_euler(m); });

    // --- Seam edges, valence, coloring ---

    function(
        "computeSeamEdges",
        +[](MeshType& mesh, unsigned indexed_attribute_id) {
            compute_seam_edges(mesh, static_cast<AttributeId>(indexed_attribute_id));
        });

    function("computeVertexValence", +[](MeshType& mesh) { compute_vertex_valence(mesh); });

    function(
        "computeGreedyColoring",
        +[](MeshType& mesh, val opts) {
            GreedyColoringOptions o;
            if (!opts.isUndefined()) {
                auto et = opts["elementType"];
                if (!et.isUndefined()) {
                    o.element_type = et.as<std::string>() == "vertex" ? AttributeElement::Vertex
                                                                      : AttributeElement::Facet;
                }
                apply_opt(opts, "numColorUsed", o.num_color_used);
            }
            compute_greedy_coloring(mesh, o);
        });

    // --- Additional mesh cleanup ---

    function(
        "removeDuplicateFacets",
        +[](MeshType& mesh, val opts) {
            RemoveDuplicateFacetOptions o;
            apply_opt(opts, "considerOrientation", o.consider_orientation);
            remove_duplicate_facets(mesh, o);
        });

    function("removeIsolatedVertices", +[](MeshType& m) { remove_isolated_vertices(m); });

    function(
        "removeNullAreaFacets",
        +[](MeshType& mesh, val opts) {
            RemoveNullAreaFacetsOptions o;
            apply_opt(opts, "nullAreaThreshold", o.null_area_threshold);
            apply_opt(opts, "removeIsolatedVertices", o.remove_isolated_vertices);
            remove_null_area_facets(mesh, o);
        });

    function(
        "removeShortEdges",
        +[](MeshType& mesh, Scalar threshold) { remove_short_edges(mesh, threshold); });

    function(
        "removeTopologicallyDegenerateFacets",
        +[](MeshType& m) { remove_topologically_degenerate_facets(m); });

    function("resolveNonmanifoldness", +[](MeshType& m) { resolve_nonmanifoldness(m); });

    function(
        "resolveVertexNonmanifoldness",
        +[](MeshType& m) { resolve_vertex_nonmanifoldness(m); });
}
