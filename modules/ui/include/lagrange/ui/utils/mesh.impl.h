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

#include <igl/ray_mesh_intersect.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/select_facets_in_frustum.h>
#include <lagrange/ui/Entity.h>
#include <lagrange/ui/types/Camera.h>
#include <lagrange/ui/types/Color.h>
#include <lagrange/ui/types/Frustum.h>
#include <lagrange/ui/types/RayFacetHit.h>
#include <lagrange/ui/types/VertexBuffer.h>
#include <lagrange/ui/utils/math.h>
#include <lagrange/ui/utils/objectid_viewport.h>
#include <lagrange/utils/tbb.h>


namespace lagrange {
namespace ui {

namespace detail {

//////////////////////////////////////////////////////////////////////////////////////
// Getters
//////////////////////////////////////////////////////////////////////////////////////

template <typename T>
RowMajorMatrixXf eigen_convert_to_float(const T& input)
{
    return input.template cast<float>().eval();
}

template <typename T>
RowMajorMatrixXi eigen_convert_to_int(const T& input)
{
    return input.template cast<int>().eval();
}

template <typename MeshType>
size_t get_num_vertices(const MeshBase* mesh_base)
{
    return reinterpret_cast<const MeshType&>(*mesh_base).get_num_vertices();
}
template <typename MeshType>
size_t get_num_facets(const MeshBase* mesh_base)
{
    return reinterpret_cast<const MeshType&>(*mesh_base).get_num_facets();
}
template <typename MeshType>
size_t get_num_edges(const MeshBase* mesh_base)
{
    return reinterpret_cast<const MeshType&>(*mesh_base).get_num_edges_new();
}

template <typename MeshType>
const typename MeshType::VertexArray& get_mesh_vertices(const MeshBase* mesh_base)
{
    return reinterpret_cast<const MeshType&>(*mesh_base).get_vertices();
}


template <typename MeshType>
RowMajorMatrixXi get_mesh_facets(const MeshBase* mesh_base)
{
    return reinterpret_cast<const MeshType&>(*mesh_base).get_facets().template cast<int>();
}

template <typename MeshType>
RowMajorMatrixXf get_mesh_vertex_attribute(const MeshBase* mesh_base, const std::string& name)
{
    return reinterpret_cast<const MeshType&>(*mesh_base)
        .get_vertex_attribute(name)
        .template cast<float>();
}

template <typename MeshType>
RowMajorMatrixXf get_mesh_corner_attribute(const MeshBase* mesh_base, const std::string& name)
{
    return reinterpret_cast<const MeshType&>(*mesh_base)
        .get_corner_attribute(name)
        .template cast<float>();
}

template <typename MeshType>
RowMajorMatrixXf get_mesh_facet_attribute(const MeshBase* mesh_base, const std::string& name)
{
    return reinterpret_cast<const MeshType&>(*mesh_base)
        .get_facet_attribute(name)
        .template cast<float>();
}

template <typename MeshType>
RowMajorMatrixXf get_mesh_edge_attribute(const MeshBase* mesh_base, const std::string& name)
{
    return reinterpret_cast<const MeshType&>(*mesh_base)
        .get_edge_attribute_new(name)
        .template cast<float>();
}


template <typename MeshType>
RowMajorMatrixXf
get_mesh_attribute(const MeshBase* mesh_base, IndexingMode mode, const std::string& name)
{
    LA_ASSERT(
        mode != lagrange::ui::IndexingMode::INDEXED,
        "Indexed attribute not supported. Map to corner first.");

    const auto& mesh = reinterpret_cast<const MeshType&>(*mesh_base);
    switch (mode) {
    case lagrange::ui::IndexingMode::VERTEX:
        return mesh.get_vertex_attribute(name).template cast<float>();
    case lagrange::ui::IndexingMode::EDGE:
        return mesh.get_edge_attribute_new(name).template cast<float>();
    case lagrange::ui::IndexingMode::FACE:
        return mesh.get_facet_attribute(name).template cast<float>();
    case lagrange::ui::IndexingMode::CORNER:
        return mesh.get_corner_attribute(name).template cast<float>();
    default: break;
    }
    return RowMajorMatrixXf();
}


template <typename MeshType>
std::pair<Eigen::VectorXf, Eigen::VectorXf>
get_mesh_attribute_range(const MeshBase* mesh_base, IndexingMode mode, const std::string& name)
{
    using AttributeArray = typename MeshType::AttributeArray;

    const AttributeArray* aa = nullptr;
    const auto& mesh = reinterpret_cast<const MeshType&>(*mesh_base);

    switch (mode) {
    case lagrange::ui::IndexingMode::VERTEX: aa = &mesh.get_vertex_attribute(name); break;
    case lagrange::ui::IndexingMode::EDGE: aa = &mesh.get_edge_attribute_new(name); break;
    case lagrange::ui::IndexingMode::FACE: aa = &mesh.get_facet_attribute(name); break;
    case lagrange::ui::IndexingMode::CORNER: aa = &mesh.get_corner_attribute(name); break;
    default: break;
    }

    LA_ASSERT(aa != nullptr);

    return {
        aa->colwise().minCoeff().transpose().template cast<float>().eval(),
        aa->colwise().maxCoeff().transpose().template cast<float>().eval()};
}

template <typename MeshType>
AABB get_mesh_bounds(const MeshBase* mesh_base)
{
    const auto& mesh = reinterpret_cast<const MeshType&>(*mesh_base);
    if (mesh.get_num_vertices() == 0) return AABB();

    const auto& V = mesh.get_vertices();
    return AABB(
        V.colwise().minCoeff().template cast<float>(),
        V.colwise().maxCoeff().template cast<float>());
}

//////////////////////////////////////////////////////////////////////////////////////
// Ensure existence of mesh attributes for rendering
//////////////////////////////////////////////////////////////////////////////////////

template <typename MeshType>
void ensure_uv(MeshBase* d)
{
    auto& mesh = reinterpret_cast<MeshType&>(*d);
    if (!mesh.has_corner_attribute("uv")) {
        if (mesh.has_vertex_attribute("uv")) {
            map_vertex_attribute_to_corner_attribute(mesh, "uv");
        } else if (mesh.has_indexed_attribute("uv")) {
            map_indexed_attribute_to_corner_attribute(mesh, "uv");
        } else {
            // No UV
        }
    }
}

template <typename MeshType>
void ensure_normal(MeshBase* d)
{
    auto& mesh = reinterpret_cast<MeshType&>(*d);
    if (!mesh.has_corner_attribute("normal")) {
        if (mesh.has_vertex_attribute("normal")) {
            map_vertex_attribute_to_corner_attribute(mesh, "normal");
        } else if (mesh.has_indexed_attribute("normal")) {
            map_indexed_attribute_to_corner_attribute(mesh, "normal");
        } else {
            lagrange::compute_vertex_normal(mesh, igl::PER_VERTEX_NORMALS_WEIGHTING_TYPE_UNIFORM);
            map_vertex_attribute_to_corner_attribute(mesh, "normal");
        }
    }
}

template <typename MeshType>
void ensure_tangent_bitangent(MeshBase* d)
{
    auto& mesh = reinterpret_cast<MeshType&>(*d);

    if (!mesh.has_corner_attribute("tangent") || !mesh.has_corner_attribute("bitangent")) {
        if (mesh.has_vertex_attribute("tangent")) {
            map_vertex_attribute_to_corner_attribute(mesh, "tangent");
        }
        if (mesh.has_vertex_attribute("bitangent")) {
            map_vertex_attribute_to_corner_attribute(mesh, "bitangent");
        }

        // Check again
        if (!mesh.has_corner_attribute("tangent") || !mesh.has_corner_attribute("bitangent")) {
            if (mesh.has_corner_attribute("uv")) {
                compute_corner_tangent_bitangent(mesh);
            } else {
                // No uvs -> no tangent nor bitangent
            }
        }
    }
}

template <typename MeshType>
void ensure_is_selected_attribute(MeshBase* d)
{
    auto& mesh = reinterpret_cast<MeshType&>(*d);

    const auto attrib_name = "is_selected";
    using AttributeArray = typename MeshType::AttributeArray;

    if (!mesh.has_corner_attribute(attrib_name)) {
        mesh.add_corner_attribute(attrib_name);
        mesh.import_corner_attribute(
            attrib_name,
            AttributeArray::Zero(mesh.get_num_facets() * mesh.get_vertex_per_facet(), 1));
    }

    if (!mesh.has_facet_attribute(attrib_name)) {
        mesh.add_facet_attribute(attrib_name);
        mesh.import_facet_attribute(attrib_name, AttributeArray::Zero(mesh.get_num_facets(), 1));
    }

    if (mesh.is_edge_data_initialized_new() && !mesh.has_edge_attribute_new(attrib_name)) {
        mesh.add_edge_attribute_new(attrib_name);
        mesh.import_edge_attribute_new(
            attrib_name,
            AttributeArray::Zero(mesh.get_num_edges_new(), 1));
    }

    if (!mesh.has_vertex_attribute(attrib_name)) {
        mesh.add_vertex_attribute(attrib_name);
        mesh.import_vertex_attribute(attrib_name, AttributeArray::Zero(mesh.get_num_vertices(), 1));
    }
}

template <typename MeshType>
void map_indexed_attribute_to_corner_attribute(MeshBase* d, const std::string& name)
{
    auto& mesh = reinterpret_cast<MeshType&>(*d);
    lagrange::map_indexed_attribute_to_corner_attribute(mesh, name);
}

//////////////////////////////////////////////////////////////////////////////////////
// Mesh to GPU upload
//////////////////////////////////////////////////////////////////////////////////////


template <typename FacetArray>
void upload_facets(const FacetArray& facets, GPUBuffer* gpu)
{
    static_assert(sizeof(typename FacetArray::Scalar) == sizeof(unsigned int), "Must be u/int32");
    gpu->vbo().upload(
        GLuint(facets.size() * sizeof(unsigned int)),
        (const uint8_t*)facets.data(),
        GLsizei(facets.rows()),
        true,
        GL_UNSIGNED_INT);
}

template <typename MeshType>
void upload_mesh_triangles(const MeshBase* mesh_base, GPUBuffer* gpu)
{
    const auto& mesh = reinterpret_cast<const MeshType&>(*mesh_base);
    LA_ASSERT(mesh.get_facets().cols() == 3, "Triangulate the mesh first");

    auto& facets = mesh.get_facets();
    upload_facets(facets, gpu);
}


template <typename T>
std::shared_ptr<GPUBuffer> create_gpubuffer_and_upload(T&& data)
{
    auto gpubuf = std::make_shared<GPUBuffer>();
    gpubuf->vbo().upload(std::move(data));
    return gpubuf;
}

template <typename T>
std::shared_ptr<GPUBuffer> create_gpubuffer_and_upload(const T& data)
{
    auto gpubuf = std::make_shared<GPUBuffer>();
    gpubuf->vbo().upload(data);
    return gpubuf;
}

template <typename MeshType>
void upload_mesh_vertex_attribute(const MeshBase* d, const RowMajorMatrixXf* data, GPUBuffer* gpu)
{
    auto& m = reinterpret_cast<const MeshType&>(*d);
    LA_ASSERT(data->rows() == m.get_num_vertices());
    RowMajorMatrixXf flattened = RowMajorMatrixXf(m.get_num_facets() * 3, data->cols());

    const auto& F = m.get_facets();
    for (auto fi = 0; fi < m.get_num_facets(); fi++) {
        flattened.row(3 * fi + 0) = data->row(F(fi, 0));
        flattened.row(3 * fi + 1) = data->row(F(fi, 1));
        flattened.row(3 * fi + 2) = data->row(F(fi, 2));
    }

    (*gpu).vbo().upload(std::move(flattened));
}

template <typename MeshType>
void upload_mesh_corner_attribute(const MeshBase* d, const RowMajorMatrixXf* data, GPUBuffer* gpu)
{
    auto& m = reinterpret_cast<const MeshType&>(*d);
    LA_ASSERT(data->rows() == m.get_num_facets() * 3);
    (*gpu).vbo().upload(*data);
}

template <typename MeshType>
void upload_mesh_facet_attribute(const MeshBase* d, const RowMajorMatrixXf* data, GPUBuffer* gpu)
{
    auto& m = reinterpret_cast<const MeshType&>(*d);
    LA_ASSERT(data->rows() == m.get_num_facets());

    RowMajorMatrixXf flattened = RowMajorMatrixXf(m.get_num_facets() * 3, data->cols());

    for (auto i = 0; i < data->rows(); i++) {
        flattened.row(3 * i + 0) = data->row(i);
        flattened.row(3 * i + 1) = data->row(i);
        flattened.row(3 * i + 2) = data->row(i);
    }

    (*gpu).vbo().upload(std::move(flattened));
}

template <typename MeshType>
void upload_mesh_edge_attribute(const MeshBase* d, const RowMajorMatrixXf* data, GPUBuffer* gpu)
{
    const auto& m = reinterpret_cast<const MeshType&>(*d);
    LA_ASSERT(m.is_edge_data_initialized_new(), "Edge data (new) not initialized");
    LA_ASSERT(data->rows() == m.get_num_edges_new());

    const auto& F = m.get_facets();
    const auto per_facet = m.get_vertex_per_facet();

    RowMajorMatrixXf flattened = RowMajorMatrixXf(m.get_num_facets() * per_facet, data->cols());

    // For each flattened triangle
    for (auto i = 0; i < F.rows(); i++) {
        // For each of its edges
        for (auto k = 0; k < per_facet; k++) {
            const auto ei = m.get_edge_new(i, k);
            flattened.row(per_facet * i + k) = data->row(ei);
        }
    }

    (*gpu).vbo().upload(std::move(flattened));
}

template <typename MeshType>
void upload_mesh_vertices(const MeshBase* mesh_base, GPUBuffer* gpu)
{
    // TODO template for 2D
    auto& m = reinterpret_cast<const MeshType&>(*mesh_base);
    auto& data = m.get_vertices();

    RowMajorMatrixXf flattened = RowMajorMatrixXf(m.get_num_facets() * 3, data.cols());

    const auto& F = m.get_facets();
    for (auto fi = 0; fi < m.get_num_facets(); fi++) {
        flattened.row(3 * fi + 0) = data.row(F(fi, 0)).template cast<float>();
        flattened.row(3 * fi + 1) = data.row(F(fi, 1)).template cast<float>();
        flattened.row(3 * fi + 2) = data.row(F(fi, 2)).template cast<float>();
    }

    (*gpu).vbo().upload(std::move(flattened));
}

template <typename MeshType>
std::unordered_map<entt::id_type, std::shared_ptr<GPUBuffer>> upload_submesh_indices(
    const MeshBase* mesh_base,
    const std::string& facet_attrib_name)
{
    auto& m = reinterpret_cast<const MeshType&>(*mesh_base);

    const auto& sub_ids = m.get_facet_attribute(facet_attrib_name);

    using FacetArray = typename MeshType::FacetArray;

    std::unordered_map<entt::id_type, std::shared_ptr<GPUBuffer>> result;
    std::unordered_map<entt::id_type, size_t> sub_counts;
    std::unordered_map<entt::id_type, FacetArray> submesh_triangles;

    // Count submeshes and number of triangles per submesh
    for (auto fi = 0; fi < m.get_num_facets(); fi++) {
        auto id = entt::id_type(sub_ids(fi, 0));
        auto it = sub_counts.find(id);
        if (it == sub_counts.end())
            sub_counts.insert({id, 1});
        else
            it->second++;
    }

    // Allocate submesh index arrays
    for (auto& it : sub_counts) {
        submesh_triangles.insert({it.first, FacetArray(it.second, 3)});
    }

    // Go in reverse, decrementing the counter
    for (auto fi = m.get_num_facets() - 1; fi >= 0; fi--) {
        auto id = entt::id_type(sub_ids(fi, 0));
        auto& counter = sub_counts.at(id);
        auto& triangles = submesh_triangles.at(id);
        triangles(counter - 1, 0) = 3 * fi + 0;
        triangles(counter - 1, 1) = 3 * fi + 1;
        triangles(counter - 1, 2) = 3 * fi + 2;
        counter--;
    }
#ifdef _DEBUG
    for (auto& it : sub_counts) {
        assert(it.second == 0);
    }
#endif

    for (auto& it : submesh_triangles) {
        auto buf = std::make_shared<GPUBuffer>(GL_ELEMENT_ARRAY_BUFFER);
        upload_facets(it.second, buf.get());
        result.insert({it.first, std::move(buf)});
    }


    return result;
}


//////////////////////////////////////////////////////////////////////////////////////
// Has attribute
//////////////////////////////////////////////////////////////////////////////////////

template <typename MeshType>
bool has_mesh_vertex_attribute(const MeshBase* d, const std::string& name)
{
    return reinterpret_cast<const MeshType&>(*d).has_vertex_attribute(name);
}
template <typename MeshType>
bool has_mesh_corner_attribute(const MeshBase* d, const std::string& name)
{
    return reinterpret_cast<const MeshType&>(*d).has_corner_attribute(name);
}
template <typename MeshType>
bool has_mesh_facet_attribute(const MeshBase* d, const std::string& name)
{
    return reinterpret_cast<const MeshType&>(*d).has_facet_attribute(name);
}
template <typename MeshType>
bool has_mesh_edge_attribute(const MeshBase* d, const std::string& name)
{
    return reinterpret_cast<const MeshType&>(*d).has_edge_attribute_new(name);
}
template <typename MeshType>
bool has_mesh_indexed_attribute(const MeshBase* d, const std::string& name)
{
    return reinterpret_cast<const MeshType&>(*d).has_indexed_attribute(name);
}

//////////////////////////////////////////////////////////////////////////////////////
// Picking
//////////////////////////////////////////////////////////////////////////////////////

template <typename MeshType>
bool intersect_ray(
    const MeshBase* mesh_base,
    const Eigen::Vector3f& origin,
    const Eigen::Vector3f& dir,
    RayFacetHit* out)
{
    auto& m = reinterpret_cast<const MeshType&>(*mesh_base);
    using Scalar = typename MeshType::Scalar;


    igl::Hit ihit;
    bool res = igl::ray_mesh_intersect(
        origin.cast<Scalar>(),
        dir.cast<Scalar>(),
        m.get_vertices(),
        m.get_facets(),
        ihit);

    if (!res) return false;

    out->facet_id = ihit.id;
    out->t = ihit.t;
    out->barycentric = Eigen::Vector3f(1.0f - ihit.u - ihit.v, ihit.u, ihit.v);
    return true;
}


template <typename MeshType>
bool select_facets_in_frustum(
    MeshBase* mesh_base,
    SelectionBehavior sel_behavior,
    const Frustum* frustum_ptr)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);
    const auto& frustum = *frustum_ptr;
    using Scalar = typename MeshType::Scalar;
    using Point = Eigen::Matrix<Scalar, 3, 1>;


    // Facet attribute
    return lagrange::select_facets_in_frustum<MeshType, Point>(
        mesh,
        frustum.planes[FRUSTUM_LEFT].cast<Scalar>().normal().eval(),
        frustum.planes[FRUSTUM_LEFT].cast<Scalar>().projection(Point::Zero()),
        frustum.planes[FRUSTUM_BOTTOM].cast<Scalar>().normal().eval(),
        frustum.planes[FRUSTUM_BOTTOM].cast<Scalar>().projection(Point::Zero()),
        frustum.planes[FRUSTUM_RIGHT].cast<Scalar>().normal().eval(),
        frustum.planes[FRUSTUM_RIGHT].cast<Scalar>().projection(Point::Zero()),
        frustum.planes[FRUSTUM_TOP].cast<Scalar>().normal().eval(),
        frustum.planes[FRUSTUM_TOP].cast<Scalar>().projection(Point::Zero()));
}

template <typename MeshType>
void select_vertices_in_frustum(
    MeshBase* mesh_base,
    SelectionBehavior sel_behavior,
    const Frustum* frustum_ptr)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);
    const auto& frustum = *frustum_ptr;

    static_assert(MeshTrait<MeshType>::is_mesh(), "Input is not a mesh.");
    using AttributeArray = typename MeshType::AttributeArray;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    const auto num_vertices = mesh.get_num_vertices();
    const auto& vertices = mesh.get_vertices();

    AttributeArray attr;

    if (!mesh.has_vertex_attribute("is_selected")) {
        mesh.add_vertex_attribute("is_selected");
        attr = AttributeArray(num_vertices, 1);
    } else {
        mesh.export_vertex_attribute("is_selected", attr);
        LA_ASSERT(attr.rows() == num_vertices);
    }


    if (sel_behavior == SelectionBehavior::SET) {
        attr.setZero();
    }
    using Scalar = typename MeshType::Scalar;
    const Scalar value = (sel_behavior != SelectionBehavior::ERASE) ? Scalar(1) : Scalar(0);


    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_vertices),
        [&value, &attr, &frustum, &vertices](const tbb::blocked_range<Index>& tbb_range) {
            for (auto vi = tbb_range.begin(); vi != tbb_range.end(); vi++) {
                if (tbb_utils::is_cancelled()) break;
                if (frustum.contains(vertices.row(vi).template cast<float>())) {
                    attr(vi, 0) = value;
                }
            }
        });

    mesh.import_vertex_attribute("is_selected", attr);
}

template <typename MeshType>
void select_edges_in_frustum(
    MeshBase* mesh_base,
    SelectionBehavior sel_behavior,
    const Frustum* frustum_ptr)
{
    LA_ASSERT(false, "not implemented yet");
}


template <typename MeshType>
void propagate_corner_selection(MeshBase* mesh_base, const std::string& attrib_name)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);

    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using Index = typename MeshType::Index;
    LA_ASSERT(mesh.has_corner_attribute(attrib_name));

    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();

    const auto& facets = mesh.get_facets();

    {
        typename MeshType::AttributeArray corner_attrib;
        mesh.export_corner_attribute(attrib_name, corner_attrib);

        typename MeshType::AttributeArray vertex_attrib;
        mesh.export_vertex_attribute(attrib_name, vertex_attrib);

        vertex_attrib.setZero();

        // TODO: TBB Parallelize

        // Corner to vertex
        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                if (corner_attrib(i * vertex_per_facet + j) != 0.0f) {
                    vertex_attrib(facets(i, j)) = 1.0f;
                }
            }
        }

        // Vertex to corner
        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                if (vertex_attrib(facets(i, j)) != 0.0f) {
                    corner_attrib(i * vertex_per_facet + j) = 1.0f;
                }
            }
        }

        mesh.import_vertex_attribute(attrib_name, vertex_attrib);
        mesh.import_corner_attribute(attrib_name, corner_attrib);
    }
}

template <typename MeshType>
void propagate_vertex_selection(MeshBase* mesh_base, const std::string& attrib_name)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");

    using Index = typename MeshType::Index;
    LA_ASSERT(mesh.has_corner_attribute(attrib_name));
    LA_ASSERT(mesh.has_vertex_attribute(attrib_name));

    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();

    const auto& facets = mesh.get_facets();

    typename MeshType::AttributeArray corner_attrib;
    mesh.export_corner_attribute(attrib_name, corner_attrib);

    const typename MeshType::AttributeArray& vertex_attrib = mesh.get_vertex_attribute(attrib_name);

    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_facets),
        [&](const tbb::blocked_range<Index>& tbb_range) {
            for (auto fi = tbb_range.begin(); fi != tbb_range.end(); fi++) {
                if (tbb_utils::is_cancelled()) break;

                for (Index j = 0; j < vertex_per_facet; j++) {
                    if (vertex_attrib(facets(fi, j)) != 0.0f) {
                        corner_attrib(fi * vertex_per_facet + j) = 1.0f;
                    } else {
                        corner_attrib(fi * vertex_per_facet + j) = 0.0f;
                    }
                }
            }
        });

    mesh.import_corner_attribute(attrib_name, corner_attrib);
}

template <typename MeshType>
void propagate_facet_selection(MeshBase* mesh_base, const std::string& attrib_name)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);

    // Convert to vertex attribute
    const auto per_facet = mesh.get_vertex_per_facet();
    const auto& facet_attr = mesh.get_facet_attribute(attrib_name);

    using AttributeArray = typename MeshType::AttributeArray;

    AttributeArray vertex_attr;
    mesh.export_vertex_attribute(attrib_name, vertex_attr);
    vertex_attr.setZero();

    const auto& F = mesh.get_facets();

    // TODO: TBB paralellize
    for (auto fi = 0; fi < F.rows(); fi++) {
        for (auto j = 0; j < per_facet; j++) {
            auto val = facet_attr(fi);
            if (val > 0.0f) {
                vertex_attr(F(fi, j)) = val;
            }
        }
    }
    mesh.import_vertex_attribute(attrib_name, vertex_attr);

    // Map to corner for visualization
    lagrange::map_vertex_attribute_to_corner_attribute(mesh, attrib_name);
}


/// @brief Sets corner value to 1.0 only if vertex AND corner values are not zero
/// @tparam MeshType
/// @param mesh
/// @param attrib_name
template <typename MeshType>
void combine_vertex_and_corner_selection(MeshBase* mesh_base, const std::string& attrib_name)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);

    using AttribArray = typename MeshType::AttributeArray;

    const auto num_facets = mesh.get_num_facets();
    const auto vertex_per_facet = mesh.get_vertex_per_facet();
    const auto& facets = mesh.get_facets();

    const AttribArray& vertex_attrib = mesh.get_vertex_attribute(attrib_name);

    AttribArray corner_attrib;
    mesh.export_corner_attribute(attrib_name, corner_attrib);

    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;

    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_facets),
        [&](const tbb::blocked_range<Index>& tbb_range) {
            for (auto fi = tbb_range.begin(); fi != tbb_range.end(); fi++) {
                if (tbb_utils::is_cancelled()) break;
                for (Index j = 0; j < vertex_per_facet; j++) {
                    auto& corner_val = corner_attrib(fi * vertex_per_facet + j);
                    if (vertex_attrib(facets(fi, j)) != Scalar(0) && corner_val != Scalar(0)) {
                        corner_val = Scalar(1);
                    } else {
                        corner_val = Scalar(0);
                    }
                }
            }
        });

    mesh.import_corner_attribute(attrib_name, corner_attrib);
}

template <typename MeshType>
void select_facets_by_color(
    MeshBase* mesh_base,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior,
    const unsigned char* color_bytes,
    size_t colors_byte_size)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);

    using Attr = typename MeshType::AttributeArray;
    Attr attrib;
    mesh.export_facet_attribute(attrib_name, attrib);

    if (sel_behavior == SelectionBehavior::SET) {
        attrib.setZero();
    }
    using Scalar = typename MeshType::Scalar;
    const auto value = (sel_behavior != SelectionBehavior::ERASE) ? Scalar(1) : Scalar(0);

    const size_t pixel_size = 4;
    for (auto i = 0; i < colors_byte_size / pixel_size; i++) {
        const auto id = color_to_id(
            color_bytes[pixel_size * i + 0],
            color_bytes[pixel_size * i + 1],
            color_bytes[pixel_size * i + 2]);
        if (is_id_background(id)) continue;
        attrib(id) = value;
    }

    mesh.import_facet_attribute(attrib_name, attrib);
}

template <typename MeshType>
void select_edges_by_color(
    MeshBase* mesh_base,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior,
    const unsigned char* color_bytes,
    size_t colors_byte_size)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);

    using Attr = typename MeshType::AttributeArray;
    Attr attrib;
    mesh.export_corner_attribute(attrib_name, attrib);


    if (sel_behavior == SelectionBehavior::SET) {
        attrib.setZero();
    }
    using Scalar = typename MeshType::Scalar;
    const auto value = (sel_behavior != SelectionBehavior::ERASE) ? Scalar(1) : Scalar(0);

    const size_t pixel_size = 4;
    for (auto i = 0; i < colors_byte_size / pixel_size; i++) {
        const auto id = color_to_id(
            color_bytes[pixel_size * i + 0],
            color_bytes[pixel_size * i + 1],
            color_bytes[pixel_size * i + 2]);
        if (is_id_background(id)) continue;

        const auto face_id = id / 3;
        const auto edge_id = id % 3;
        attrib(face_id * 3 + (edge_id + 1) % 3) = value;
        attrib(face_id * 3 + (edge_id + 2) % 3) = value;
    }
    mesh.import_corner_attribute(attrib_name, attrib);
}

template <typename MeshType>
void select_vertices_by_color(
    MeshBase* mesh_base,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior,
    const unsigned char* color_bytes,
    size_t colors_byte_size)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);

    using Attr = typename MeshType::AttributeArray;
    Attr attrib;
    mesh.export_corner_attribute(attrib_name, attrib);

    if (sel_behavior == SelectionBehavior::SET) {
        attrib.setZero();
    }
    using Scalar = typename MeshType::Scalar;
    const auto value = (sel_behavior != SelectionBehavior::ERASE) ? Scalar(1) : Scalar(0);


    const size_t pixel_size = 4;
    for (auto i = 0; i < colors_byte_size / pixel_size; i++) {
        const auto id = color_to_id(
            color_bytes[pixel_size * i + 0],
            color_bytes[pixel_size * i + 1],
            color_bytes[pixel_size * i + 2]);
        if (is_id_background(id)) continue;
        attrib(id) = value;
    }
    mesh.import_corner_attribute(attrib_name, attrib);
}


template <typename MeshType>
void select_facets(
    MeshBase* mesh_base,
    SelectionBehavior sel_behavior,
    const std::vector<int>* facet_indices)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);

    const auto attrib_name = "is_selected";
    using Attr = typename MeshType::AttributeArray;
    Attr attrib;
    mesh.export_facet_attribute(attrib_name, attrib);

    if (sel_behavior == SelectionBehavior::SET) {
        attrib.setZero();
    }

    using Scalar = typename MeshType::Scalar;
    const auto value = (sel_behavior != SelectionBehavior::ERASE) ? Scalar(1) : Scalar(0);

    for (auto i : (*facet_indices)) {
        attrib(i, 0) = value;
    }

    mesh.import_facet_attribute(attrib_name, attrib);
}


template <typename MeshType>
void filter_closest_vertex(
    MeshBase* mesh_base,
    const std::string& attrib_name,
    SelectionBehavior sel_behavior,
    const Camera& camera,
    const Eigen::Vector2i& viewport_pos)
{
    auto& mesh = reinterpret_cast<MeshType&>(*mesh_base);
    using AttribArray = typename MeshType::AttributeArray;

    const auto num_vertices = mesh.get_num_vertices();
    const auto& vertices = mesh.get_vertices();

    AttribArray vertex_attrib;
    mesh.export_vertex_attribute(attrib_name, vertex_attrib);

    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;

    struct LocalBuffer
    {
        int screen_diff = std::numeric_limits<int>::max();
        float min_depth = std::numeric_limits<float>::max();
        Index min_vi = lagrange::INVALID<Index>();
        std::vector<Index> equivalence = {};
    };

    tbb::enumerable_thread_specific<LocalBuffer> temp_vars;

    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_vertices),
        [&](const tbb::blocked_range<Index>& tbb_range) {
            auto& min_screen_diff = temp_vars.local().screen_diff;
            auto& min_depth = temp_vars.local().min_depth;
            auto& min_vi = temp_vars.local().min_vi;
            auto& equiv = temp_vars.local().equivalence;

            for (auto vi = tbb_range.begin(); vi != tbb_range.end(); vi++) {
                if (tbb_utils::is_cancelled()) break;

                if (vertex_attrib(vi) == 0.0f) continue;

                const auto proj =
                    camera.project_with_depth(vertices.row(vi).template cast<float>());
                const auto diff =
                    (Eigen::Vector2i(int(proj.x()), int(camera.get_window_height() - proj.y())) -
                     viewport_pos)
                        .squaredNorm();

                if (diff == min_screen_diff && proj.z() == min_depth) {
                    equiv.push_back(vi);
                } else if (
                    proj.z() < min_depth || (proj.z() == min_depth && diff < min_screen_diff)) {
                    min_screen_diff = diff;
                    min_depth = proj.z();
                    min_vi = vi;
                    equiv.clear();
                }
            }
        });


    {
        LocalBuffer final_buffer;
        for (auto& var : temp_vars) {
            if (var.min_vi == lagrange::INVALID<Index>()) continue;

            if (var.screen_diff == final_buffer.screen_diff &&
                var.min_depth == final_buffer.min_depth) {
                final_buffer.equivalence.push_back(var.min_vi);
                for (auto e : var.equivalence) {
                    final_buffer.equivalence.push_back(e);
                }
            } else if (
                var.min_depth < final_buffer.min_depth ||
                (var.screen_diff < final_buffer.screen_diff &&
                 var.min_depth == final_buffer.min_depth)) {
                final_buffer.screen_diff = var.screen_diff;
                final_buffer.min_depth = var.min_depth;
                final_buffer.min_vi = var.min_vi;
                final_buffer.equivalence.clear();
            }
        }

        if (sel_behavior == SelectionBehavior::SET) {
            vertex_attrib.setZero();
        }

        const auto value = (sel_behavior != SelectionBehavior::ERASE) ? Scalar(1) : Scalar(0);

        if (final_buffer.min_vi != lagrange::INVALID<Index>()) {
            vertex_attrib(final_buffer.min_vi, 0) = value;
            for (auto equiv_vi : final_buffer.equivalence) {
                vertex_attrib(equiv_vi, 0) = value;
            }
        }
    }


    mesh.import_vertex_attribute(attrib_name, vertex_attrib);
}


} // namespace detail


template <typename MeshType>
void register_mesh_type(const std::string& display_name = entt::type_id<MeshType>().name().data())
{
    using namespace entt::literals;

    // Register MeshType
    entt::meta<MeshType>().type();
    entt::meta<MeshType>().template base<lagrange::MeshBase>();

    // Register conversion functions
    using VertexArray = typename MeshType::VertexArray;

    entt::meta<RowMajorMatrixXf>().template ctor<&detail::eigen_convert_to_float<VertexArray>>();

    // Getters
    entt::meta<MeshType>().template func<&detail::get_num_vertices<MeshType>>(
        "get_num_vertices"_hs);
    entt::meta<MeshType>().template func<&detail::get_num_edges<MeshType>>("get_num_edges"_hs);
    entt::meta<MeshType>().template func<&detail::get_num_facets<MeshType>>("get_num_facets"_hs);

    entt::meta<MeshType>().template func<&detail::get_mesh_vertices<MeshType>>(
        "get_mesh_vertices"_hs);

    entt::meta<MeshType>().template func<&detail::get_mesh_facets<MeshType>>("get_mesh_facets"_hs);
    entt::meta<MeshType>().template func<&detail::get_mesh_vertex_attribute<MeshType>>(
        "get_mesh_vertex_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::get_mesh_corner_attribute<MeshType>>(
        "get_mesh_corner_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::get_mesh_facet_attribute<MeshType>>(
        "get_mesh_facet_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::get_mesh_edge_attribute<MeshType>>(
        "get_mesh_edge_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::get_mesh_attribute<MeshType>>(
        "get_mesh_attribute"_hs);

    entt::meta<MeshType>().template func<&detail::get_mesh_attribute_range<MeshType>>(
        "get_mesh_attribute_range"_hs);

    entt::meta<MeshType>().template func<&detail::get_mesh_bounds<MeshType>>("get_mesh_bounds"_hs);


    // Ensure attribs
    entt::meta<MeshType>().template func<&detail::ensure_uv<MeshType>>("ensure_uv"_hs);
    entt::meta<MeshType>().template func<&detail::ensure_normal<MeshType>>("ensure_normal"_hs);
    entt::meta<MeshType>().template func<&detail::ensure_tangent_bitangent<MeshType>>(
        "ensure_tangent_bitangent"_hs);
    entt::meta<MeshType>().template func<&detail::ensure_is_selected_attribute<MeshType>>(
        "ensure_is_selected_attribute"_hs);
    entt::meta<MeshType>()
        .template func<&detail::map_indexed_attribute_to_corner_attribute<MeshType>>(
            "map_indexed_attribute_to_corner_attribute"_hs);


    // Mesh to GPU
    entt::meta<MeshType>().template func<&detail::upload_mesh_vertices<MeshType>>(
        "upload_mesh_vertices"_hs);
    entt::meta<MeshType>().template func<&detail::upload_mesh_triangles<MeshType>>(
        "upload_mesh_triangles"_hs);
    entt::meta<MeshType>().template func<&detail::upload_mesh_vertex_attribute<MeshType>>(
        "upload_mesh_vertex_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::upload_mesh_corner_attribute<MeshType>>(
        "upload_mesh_corner_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::upload_mesh_facet_attribute<MeshType>>(
        "upload_mesh_facet_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::upload_mesh_edge_attribute<MeshType>>(
        "upload_mesh_edge_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::upload_submesh_indices<MeshType>>(
        "upload_submesh_indices"_hs);


    // Has attribute
    entt::meta<MeshType>().template func<&detail::has_mesh_vertex_attribute<MeshType>>(
        "has_mesh_vertex_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::has_mesh_corner_attribute<MeshType>>(
        "has_mesh_corner_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::has_mesh_facet_attribute<MeshType>>(
        "has_mesh_facet_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::has_mesh_edge_attribute<MeshType>>(
        "has_mesh_edge_attribute"_hs);
    entt::meta<MeshType>().template func<&detail::has_mesh_indexed_attribute<MeshType>>(
        "has_mesh_indexed_attribute"_hs);


    // Picking
    entt::meta<MeshType>().template func<&detail::intersect_ray<MeshType>>("intersect_ray"_hs);

    entt::meta<MeshType>().template func<&detail::select_vertices_in_frustum<MeshType>>(
        "select_vertices_in_frustum"_hs);
    entt::meta<MeshType>().template func<&detail::select_facets_in_frustum<MeshType>>(
        "select_facets_in_frustum"_hs);
    entt::meta<MeshType>().template func<&detail::select_edges_in_frustum<MeshType>>(
        "select_edges_in_frustum"_hs);


    entt::meta<MeshType>().template func<&detail::propagate_corner_selection<MeshType>>(
        "propagate_corner_selection"_hs);
    entt::meta<MeshType>().template func<&detail::propagate_vertex_selection<MeshType>>(
        "propagate_vertex_selection"_hs);
    entt::meta<MeshType>().template func<&detail::propagate_facet_selection<MeshType>>(
        "propagate_facet_selection"_hs);
    entt::meta<MeshType>().template func<&detail::combine_vertex_and_corner_selection<MeshType>>(
        "combine_vertex_and_corner_selection"_hs);


    entt::meta<MeshType>().template func<&detail::select_facets_by_color<MeshType>>(
        "select_facets_by_color"_hs);
    entt::meta<MeshType>().template func<&detail::select_edges_by_color<MeshType>>(
        "select_edges_by_color"_hs);
    entt::meta<MeshType>().template func<&detail::select_vertices_by_color<MeshType>>(
        "select_vertices_by_color"_hs);

    entt::meta<MeshType>().template func<&detail::select_facets<MeshType>>("select_facets"_hs);
    entt::meta<MeshType>().template func<&detail::filter_closest_vertex<MeshType>>(
        "filter_closest_vertex"_hs);
}


} // namespace ui
} // namespace lagrange
