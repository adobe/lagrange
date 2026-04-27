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

#include <lagrange/subdivision/mesh_subdivision.h>
#include <lagrange/subdivision/midpoint_subdivision.h>
#include <lagrange/subdivision/sqrt_subdivision.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

namespace {

using namespace lagrange;
using namespace lagrange::js::bind;
using val = emscripten::val;

subdivision::SchemeType parse_scheme(const std::string& s)
{
    if (s == "bilinear") return subdivision::SchemeType::Bilinear;
    if (s == "loop") return subdivision::SchemeType::Loop;
    return subdivision::SchemeType::CatmullClark;
}

subdivision::VertexBoundaryInterpolation parse_vertex_boundary(const std::string& s)
{
    if (s == "none") return subdivision::VertexBoundaryInterpolation::None;
    if (s == "edgeAndCorner") return subdivision::VertexBoundaryInterpolation::EdgeAndCorner;
    return subdivision::VertexBoundaryInterpolation::EdgeOnly;
}

subdivision::FaceVaryingInterpolation parse_fv_interpolation(const std::string& s)
{
    if (s == "cornersOnly") return subdivision::FaceVaryingInterpolation::CornersOnly;
    if (s == "cornersPlus1") return subdivision::FaceVaryingInterpolation::CornersPlus1;
    if (s == "cornersPlus2") return subdivision::FaceVaryingInterpolation::CornersPlus2;
    if (s == "boundaries") return subdivision::FaceVaryingInterpolation::Boundaries;
    if (s == "all") return subdivision::FaceVaryingInterpolation::All;
    return subdivision::FaceVaryingInterpolation::None;
}

} // namespace

EMSCRIPTEN_BINDINGS(lagrange_subdivision)
{
    using namespace emscripten;

    function(
        "subdivideMesh",
        +[](const MeshType& mesh, val opts) -> MeshType {
            subdivision::SubdivisionOptions o;
            if (!opts.isUndefined()) {
                auto scheme = opts["scheme"];
                if (!scheme.isUndefined()) o.scheme = parse_scheme(scheme.as<std::string>());
                apply_opt(opts, "numLevels", o.num_levels);
                auto vbi = opts["vertexBoundaryInterpolation"];
                if (!vbi.isUndefined()) {
                    o.vertex_boundary_interpolation = parse_vertex_boundary(vbi.as<std::string>());
                }
                auto fvi = opts["faceVaryingInterpolation"];
                if (!fvi.isUndefined()) {
                    o.face_varying_interpolation = parse_fv_interpolation(fvi.as<std::string>());
                }
                apply_opt(opts, "useLimitSurface", o.use_limit_surface);
                apply_opt(opts, "validateTopology", o.validate_topology);
                apply_opt(opts, "preserveSharedIndices", o.preserve_shared_indices);

                // Adaptive refinement
                auto ref = opts["refinement"];
                if (!ref.isUndefined()) {
                    auto s = ref.as<std::string>();
                    if (s == "edgeAdaptive") {
                        o.refinement = subdivision::RefinementType::EdgeAdaptive;
                    }
                }
                auto mel = opts["maxEdgeLength"];
                if (!mel.isUndefined()) o.max_edge_length = mel.as<float>();
                auto mcd = opts["maxChordalDeviation"];
                if (!mcd.isUndefined()) o.max_chordal_deviation = mcd.as<float>();
            }
            return subdivision::subdivide_mesh(mesh, o);
        });

    function(
        "midpointSubdivision",
        +[](const MeshType& mesh) -> MeshType { return subdivision::midpoint_subdivision(mesh); });

    function(
        "sqrtSubdivision",
        +[](const MeshType& mesh) -> MeshType { return subdivision::sqrt_subdivision(mesh); });
}
