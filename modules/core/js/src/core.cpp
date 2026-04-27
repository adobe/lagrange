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

#include <lagrange/Attribute.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/span.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>
#include <vector>

namespace lagrange::js::bind {
namespace {

// Splits vertices at attribute discontinuities so all indexed attributes become per-vertex.
// The returned mesh has a unified index buffer suitable for WebGL / zero-copy views.
MeshType unify_index_buffer_js(const MeshType& mesh)
{
    return lagrange::unify_index_buffer<Scalar, Index>(mesh);
}

void mesh_add_vertex(MeshType& mesh, const emscripten::val& vertex_coords)
{
    const Index dim = mesh.get_dimension();
    const unsigned len = vertex_coords["length"].as<unsigned>();
    if (len != static_cast<unsigned>(dim)) {
        throw std::runtime_error("addVertex: array length must equal mesh dimension");
    }
    std::vector<Scalar> coords(static_cast<size_t>(dim));
    for (Index i = 0; i < dim; ++i) {
        coords[static_cast<size_t>(i)] = vertex_coords[i].as<Scalar>();
    }
    mesh.add_vertex(span<const Scalar>(coords.data(), coords.size()));
}

void mesh_add_triangles(MeshType& mesh, const emscripten::val& triangle_indices)
{
    const unsigned len = triangle_indices["length"].as<unsigned>();
    if (len % 3 != 0) {
        throw std::runtime_error("addTriangles: array length must be divisible by 3");
    }
    const Index num_facets = static_cast<Index>(len / 3);
    std::vector<Index> buf(static_cast<size_t>(len));
    for (unsigned i = 0; i < len; ++i) {
        buf[i] = triangle_indices[i].as<Index>();
    }
    mesh.add_triangles(num_facets, span<const Index>(buf.data(), buf.size()));
}

emscripten::val copy_span_f32(span<const Scalar> s)
{
    if (s.empty()) {
        return emscripten::val::global("Float32Array").new_(0);
    }
    std::vector<Scalar> buf(s.begin(), s.end());
    return emscripten::val::global("Float32Array")
        .new_(emscripten::typed_memory_view(buf.size(), buf.data()));
}

emscripten::val copy_span_u32(span<const Index> s)
{
    if (s.empty()) {
        return emscripten::val::global("Uint32Array").new_(0);
    }
    std::vector<Index> buf(s.begin(), s.end());
    return emscripten::val::global("Uint32Array")
        .new_(emscripten::typed_memory_view(buf.size(), buf.data()));
}

emscripten::val mesh_get_position(const MeshType& mesh, Index vertex_id)
{
    return copy_span_f32(mesh.get_position(vertex_id));
}

emscripten::val mesh_get_facet_vertices(const MeshType& mesh, Index facet_id)
{
    return copy_span_u32(mesh.get_facet_vertices(facet_id));
}

bool mesh_has_attribute(const MeshType& mesh, const std::string& name)
{
    return mesh.has_attribute(name);
}

} // namespace
} // namespace lagrange::js::bind

EMSCRIPTEN_BINDINGS(lagrange_core)
{
    using namespace emscripten;
    using namespace lagrange::js::bind;

    class_<MeshType>("SurfaceMesh")
        .constructor<Index>()
        .function("getNumVertices", &MeshType::get_num_vertices)
        .function("getNumFacets", &MeshType::get_num_facets)
        .function("getNumCorners", &MeshType::get_num_corners)
        .function("getDimension", &MeshType::get_dimension)
        .function("isTriangleMesh", &MeshType::is_triangle_mesh)
        .function(
            "addVertex",
            +[](MeshType& mesh, const emscripten::val& vertex_coords) {
                mesh_add_vertex(mesh, vertex_coords);
            })
        .function("addTriangle", &MeshType::add_triangle)
        .function(
            "addTriangles",
            +[](MeshType& mesh, const emscripten::val& triangle_indices) {
                mesh_add_triangles(mesh, triangle_indices);
            })
        .function("getNumEdges", &MeshType::get_num_edges)
        .function("isQuadMesh", &MeshType::is_quad_mesh)
        .function("isRegular", &MeshType::is_regular)
        .function("isHybrid", &MeshType::is_hybrid)
        .function("getVertexPerFacet", &MeshType::get_vertex_per_facet)
        .function("getFacetSize", &MeshType::get_facet_size)
        .function("getFacetVertex", &MeshType::get_facet_vertex)
        .function("getFacetCornerBegin", &MeshType::get_facet_corner_begin)
        .function("getFacetCornerEnd", &MeshType::get_facet_corner_end)
        .function("getCornerVertex", &MeshType::get_corner_vertex)
        .function("getCornerFacet", &MeshType::get_corner_facet)
        .function("getPosition", &mesh_get_position)
        .function("getFacetVertices", &mesh_get_facet_vertices)
        .function("shrinkToFit", &MeshType::shrink_to_fit)
        .function("clearFacets", &MeshType::clear_facets)
        .function("clearVertices", &MeshType::clear_vertices)
        .function("hasAttribute", &mesh_has_attribute)
        .function(
            "addVertices",
            +[](MeshType& mesh, const emscripten::val& coords) {
                const unsigned len = coords["length"].as<unsigned>();
                const Index dim = mesh.get_dimension();
                if (len % dim != 0) {
                    throw std::runtime_error(
                        "addVertices: array length must be divisible by dimension");
                }
                const Index num_verts = static_cast<Index>(len / dim);
                std::vector<Scalar> buf(len);
                for (unsigned i = 0; i < len; ++i) {
                    buf[i] = coords[i].as<Scalar>();
                }
                mesh.add_vertices(num_verts, buf);
            })
        .function(
            "removeVertices",
            +[](MeshType& mesh, const emscripten::val& indices) {
                const unsigned len = indices["length"].as<unsigned>();
                std::vector<Index> buf(len);
                for (unsigned i = 0; i < len; ++i) {
                    buf[i] = indices[i].as<Index>();
                }
                mesh.remove_vertices(buf);
            })
        .function(
            "removeFacets",
            +[](MeshType& mesh, const emscripten::val& indices) {
                const unsigned len = indices["length"].as<unsigned>();
                std::vector<Index> buf(len);
                for (unsigned i = 0; i < len; ++i) {
                    buf[i] = indices[i].as<Index>();
                }
                mesh.remove_facets(buf);
            })
        .function(
            "clone",
            +[](const MeshType& self) -> MeshType { return MeshType(self); })
        .function(
            "clone",
            +[](const MeshType& self, emscripten::val opts) -> MeshType {
                bool strip = false;
                apply_opt(opts, "strip", strip);
                return strip ? MeshType::stripped_copy(self) : MeshType(self);
            })
        .function(
            "flipFacets",
            +[](MeshType& mesh) { mesh.flip_facets([](Index) { return true; }); })
        .function(
            "flipFacets",
            +[](MeshType& mesh, const emscripten::val& indices) {
                if (indices.isUndefined() || indices.isNull()) {
                    mesh.flip_facets([](Index) { return true; });
                } else {
                    const unsigned len = indices["length"].as<unsigned>();
                    std::vector<Index> buf(len);
                    for (unsigned i = 0; i < len; ++i) {
                        buf[i] = indices[i].as<Index>();
                    }
                    mesh.flip_facets(lagrange::span<const Index>(buf.data(), buf.size()));
                }
            });

    function("unifyIndexBuffer", &unify_index_buffer_js);
}
