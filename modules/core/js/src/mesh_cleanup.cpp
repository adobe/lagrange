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

#include <lagrange/mesh_cleanup/close_small_holes.h>
#include <lagrange/mesh_cleanup/remove_degenerate_facets.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>

#include <emscripten/bind.h>

namespace lagrange::js::bind {
namespace {

void close_small_holes_js(MeshType& mesh, size_t max_hole_size, emscripten::val triangulate_holes)
{
    CloseSmallHolesOptions options;
    options.max_hole_size = max_hole_size;
    if (!triangulate_holes.isUndefined()) options.triangulate_holes = triangulate_holes.as<bool>();
    close_small_holes(mesh, std::move(options));
}

void remove_degenerate_facets_js(MeshType& mesh)
{
    remove_degenerate_facets(mesh);
}

void remove_duplicate_vertices_js(MeshType& mesh)
{
    remove_duplicate_vertices(mesh, {});
}

void split_long_edges_js(MeshType& mesh, float max_edge_length, bool recursive)
{
    SplitLongEdgesOptions options;
    options.max_edge_length = max_edge_length;
    options.recursive = recursive;
    split_long_edges(mesh, std::move(options));
}

} // namespace
} // namespace lagrange::js::bind

EMSCRIPTEN_BINDINGS(lagrange_mesh_cleanup)
{
    using namespace emscripten;
    using namespace lagrange::js::bind;

    function("closeSmallHoles", &close_small_holes_js);
    function("removeDegenerateFacets", &remove_degenerate_facets_js);
    function("removeDuplicateVertices", &remove_duplicate_vertices_js);
    function("splitLongEdges", &split_long_edges_js);
}
