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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/AABB.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/Mesh.h>
#include <lagrange/ui/ProxyMesh.h>
#include <lagrange/ui/Selection.h>

#include <lagrange/compute_normal.h>
#include <lagrange/attributes/attribute_utils.h>

namespace lagrange {
namespace ui {

namespace util {

template <typename MeshType>
void compute_corner_tangent_bitangent(MeshType& mesh)
{
    using Scalar = typename MeshType::Scalar;
    // Compute tangent and bitangent
    mesh.add_corner_attribute("tangent");
    mesh.add_corner_attribute("bitangent");

    LA_ASSERT(MeshType::VertexArray::ColsAtCompileTime == 3, "Only 3D mesh supported");
    LA_ASSERT(mesh.has_corner_attribute("uv"), "No corner UV attribute defined");

    const auto& F = mesh.get_facets();
    const auto& V = mesh.get_vertices();
    const auto& UV = mesh.get_corner_attribute("uv");
    const auto n_faces = mesh.get_num_facets();
    const auto per_facet = mesh.get_vertex_per_facet();

    typename MeshType::AttributeArray T(n_faces * per_facet, 3);
    typename MeshType::AttributeArray BT(n_faces * per_facet, 3);

    for (auto i = 0; i < F.rows(); i++) {
        const auto indices = F.row(i).eval();
        const auto v0 = V.row(indices(0)).eval();
        const auto v1 = V.row(indices(1)).eval();
        const auto v2 = V.row(indices(2)).eval();

        const auto uv0 = UV.row(i * per_facet + 0).eval();
        const auto uv1 = UV.row(i * per_facet + 1).eval();
        const auto uv2 = UV.row(i * per_facet + 2).eval();

        const auto edge0 = (v1 - v0).eval();
        const auto edge1 = (v2 - v0).eval();

        const auto edge_uv0 = (uv1 - uv0).eval();
        const auto edge_uv1 = (uv2 - uv0).eval();

        const Scalar r = Scalar(1) / (edge_uv0.x() * edge_uv1.y() - edge_uv0.y() * edge_uv1.x());

        const auto tangent =
            ((edge0 * edge_uv1.y() - edge1 * edge_uv0.y()) * r).normalized().eval();
        const auto bitangent =
            ((edge1 * edge_uv0.y() - edge0 * edge_uv1.y()) * r).normalized().eval();

        for (auto k = 0; k < per_facet; k++) {
            if (indices(k) == INVALID<typename MeshType::Index>()) break;
            T.row(i * per_facet + k) = tangent;
            BT.row(i * per_facet + k) = bitangent;
        }
    }

    mesh.import_corner_attribute("tangent", T);
    mesh.import_corner_attribute("bitangent", BT);
}


/// Compute number of new triangles needed to represent
/// a triangulated mesh
template <typename MeshType>
size_t get_new_triangle_num(const MeshType& mesh)
{
    using Index = typename MeshType::Index;
    const auto& F = mesh.get_facets();
    if (F.cols() <= 3) return 0;

    size_t new_triangle_count = 0;

    // Scan through facets, see if any has more than 3 indices
    for (auto i : lagrange::range(mesh.get_num_facets())) {
        for (auto k = 3; k < F.cols(); k++) {
            if (F(i, k) != lagrange::INVALID<Index>())
                new_triangle_count++;
            else
                break;
        }
    }
    return new_triangle_count;
}


/// Triangulates polygonal mesh into proxy mesh facet array
template <typename MeshType>
ProxyMesh::FacetArray triangulate(
    const MeshType& mesh,
    std::vector<ProxyMesh::Index>& triangulation_prefix_sum,
    std::vector<ProxyMesh::Index>& new_triangle_to_orig)
{
    using Index = typename MeshType::Index;
    const auto& F = mesh.get_facets();

    // Get number of new triangles
    const size_t new_triangle_count = get_new_triangle_num(mesh);
    const size_t orig_triangles = F.rows();

    // Allocate space for original + new triangles
    ProxyMesh::FacetArray new_F(F.rows() + new_triangle_count, 3);

    // Reset triangulation storage
    triangulation_prefix_sum.clear();
    triangulation_prefix_sum.resize(orig_triangles, 0);
    new_triangle_to_orig.clear();
    new_triangle_to_orig.resize(new_triangle_count, 0);


    // Running pointer to newest triangle
    size_t new_triangle_index = orig_triangles;


    for (auto i : lagrange::range(mesh.get_num_facets())) {
        for (auto k = 0; k < 3; k++) {
            // Copy first three
            new_F(i, k) = F(i, k);
        }

        for (auto k = 3; k < F.cols(); k++) {
            if (F(i, k) == lagrange::INVALID<Index>()) break;


            triangulation_prefix_sum[i]++;
            new_triangle_to_orig[new_triangle_index - orig_triangles] = i;

            int new_index = k - 3 + 1;
            new_F(new_triangle_index, 0) = F(i, 0);
            new_F(new_triangle_index, 1) = F(i, new_index + 1);
            new_F(new_triangle_index, 2) = F(i, new_index + 2);
            new_triangle_index++;
        }
    }

    // Compute the prefix sum
    for (size_t i = 1; i < triangulation_prefix_sum.size(); i++) {
        triangulation_prefix_sum[i] = triangulation_prefix_sum[i] + triangulation_prefix_sum[i - 1];
    }

    return new_F;
}

/// Converts input vertex type to proxy mesh vertex type
/// If input is 2D, output is padded with z=0
template <typename MeshType>
ProxyMesh::VertexArray process_vertices(const MeshType& mesh)
{
    using TargetScalar = typename ProxyMesh::VertexArray::Scalar;

    const auto& V = mesh.get_vertices();
    ProxyMesh::VertexArray new_V(V.rows(), 3);

    for (auto i : lagrange::range(V.rows())) {
        for (auto k = 0; k < V.cols(); k++) {
            new_V(i, k) = static_cast<TargetScalar>(V(i, k));
        }

        // Padding
        for (auto k = V.cols(); k < new_V.cols(); k++) {
            new_V(i, k) = TargetScalar(0);
        }
    }

    return new_V;
}

/// Converts lagrange mesh uv to an corner attribute array
template <typename MeshType>
ProxyMesh::AttributeArray process_uvs(const ProxyMesh& initialized_proxy, const MeshType& mesh)
{
    using TargetScalar = typename ProxyMesh::VertexArray::Scalar;

    const auto& proxy = initialized_proxy;

    if (!mesh.is_uv_initialized()) {
        return ProxyMesh::AttributeArray();
    }

    const size_t num_corners = proxy.get_facets().rows() * 3;

    ProxyMesh::AttributeArray target_uvs(num_corners, 2);

    const auto& source_uvs_V = mesh.get_uv();
    const auto& source_uvs_F = mesh.get_uv_indices();

    for (ProxyMesh::Index i = 0; i < proxy.original_facet_num(); i++) {
        auto new_tris = proxy.polygon_triangles(i);

        for (size_t k = 0; k < new_tris.size(); k++) {
            // Which row
            const auto original_row = i;
            const auto target_row = new_tris[k];
            const auto target_stride = 3;

            target_uvs.row(target_row * target_stride + 0) =
                source_uvs_V.row(source_uvs_F(original_row, 0)).template cast<TargetScalar>();

            target_uvs.row(target_row * target_stride + 1) =
                source_uvs_V.row(source_uvs_F(original_row, k + 1)).template cast<TargetScalar>();

            target_uvs.row(target_row * target_stride + 2) =
                source_uvs_V.row(source_uvs_F(original_row, k + 2)).template cast<TargetScalar>();
        }
    }


    return target_uvs;
}

} // namespace util


template <typename MeshType>
ProxyMesh::ProxyMesh(const MeshType& orig_mesh)
{
    {
        auto F = util::triangulate(orig_mesh, m_triangulation_prefix_sum, m_new_triangle_to_orig);
        auto V = util::process_vertices(orig_mesh);

        auto geometry =
            std::make_unique<GenuineMeshGeometry<ProxyMesh::VertexArray, ProxyMesh::FacetArray>>(
                V,
                F);

        m_mesh = std::make_unique<TriangleMesh3Df>(std::move(geometry));
    }

    // Alias for clarity
    auto& proxy_mesh = *m_mesh;

    m_orig_facet_dim = safe_cast<ProxyMesh::Index>(orig_mesh.get_vertex_per_facet());
    m_orig_vertex_dim = safe_cast<ProxyMesh::Index>(orig_mesh.get_vertices().cols());
    m_orig_facet_num = safe_cast<ProxyMesh::Index>(orig_mesh.get_facets().rows());
    m_triangulated = (m_orig_facet_dim > 3);

    // Edges
    // If the edges are initialize, copy it
    if (orig_mesh.is_edge_data_initialized()) {
        m_original_edge_index_map = orig_mesh.get_edge_index_map();
        m_original_edges = orig_mesh.get_edges();
    }
    // Otherwise initialize our own edge data that can handle polygons
    else {
        const auto& orig_F = orig_mesh.get_facets();
        for (auto fi : lagrange::range(orig_F.rows())) {
            // Find number of vertices in the polygon
            int polygon_size = int(orig_F.cols());
            for (int k = polygon_size - 1; k > 0; k--) {
                if (orig_F(fi, k) == lagrange::INVALID<typename MeshType::Index>()) {
                    polygon_size--;
                } else {
                    break;
                }
            }

            // Go through edges of the polygon
            for (auto k = 0; k < polygon_size; k++) {
                const auto v0 = orig_F(fi, k);
                const auto v1 = orig_F(fi, (k + 1) % polygon_size);

                // Create an unique edge
                const auto edge = TriangleMesh3Df::Edge(v0, v1);
                auto it = m_original_edge_index_map.find(edge);


                if (it == m_original_edge_index_map.end()) {
                    m_original_edges.push_back(edge);
                    auto edge_index = int(m_original_edges.size() - 1);
                    m_original_edge_index_map[edge] = edge_index;
                }
            }
        }
    }

    // Compute vertex->vertex attribute mapping
    {
        m_vertex_to_vertex_attrib_mapping.resize(
            proxy_mesh.get_num_vertices(),
            lagrange::INVALID<unsigned int>());

        auto& F = proxy_mesh.get_facets();
        for (auto fi = 0; fi < proxy_mesh.get_num_facets(); fi++) {
            for (auto k = 0; k < 3; k++) {
                unsigned int vertex_index = F(fi, k);
                unsigned int flattened_index = 3 * fi + k;
                // Use the lowest index
                m_vertex_to_vertex_attrib_mapping[vertex_index] =
                    std::min(flattened_index, m_vertex_to_vertex_attrib_mapping[vertex_index]);
            }
        }
    }

    // Compute edge->vertex index array
    // Each edge will have a unique flattened vertex indices assigned to it
    {
        m_edge_to_vertices.resize(m_original_edges.size() * 2, lagrange::INVALID<unsigned int>());

        const auto& orig_F = orig_mesh.get_facets();
        const auto& new_F = get_facets();

        // Go through original facets and their triangulations
        for (auto fi = 0; fi < orig_F.rows(); fi++) {
            auto triangles = polygon_triangles(int(fi));

            // Single triangle, use all its edges
            if (triangles.size() == 1) {
                auto t = triangles.front();

                // All three triangle edges exist in original edges
                for (auto k = 0; k < 3; k++) {
                    auto edge_index =
                        m_original_edge_index_map.at(Edge(new_F(t, k), new_F(t, (k + 1) % 3)));
                    m_edge_to_vertices[2 * edge_index + 0] = static_cast<unsigned int>(3 * t + k);
                    m_edge_to_vertices[2 * edge_index + 1] =
                        static_cast<unsigned int>(3 * t + (k + 1) % 3);
                }
            }
            // Multiple triangles, use only original facet edges
            else {
                // First triangle - first two edges
                {
                    auto t = triangles.front();
                    auto e0i = m_original_edge_index_map.at(Edge(new_F(t, 0), new_F(t, 1)));
                    auto e1i = m_original_edge_index_map.at(Edge(new_F(t, 1), new_F(t, 2)));
                    m_edge_to_vertices[2 * e0i + 0] = static_cast<unsigned int>(3 * t + 0);
                    m_edge_to_vertices[2 * e0i + 1] = static_cast<unsigned int>(3 * t + 1);

                    m_edge_to_vertices[2 * e1i + 0] = static_cast<unsigned int>(3 * t + 1);
                    m_edge_to_vertices[2 * e1i + 1] = static_cast<unsigned int>(3 * t + 2);
                }

                // Middle triangles - single edge (second one of the triangle)
                for (size_t ti = 1; ti < triangles.size() - 1; ti++) {
                    auto t = triangles[ti];
                    auto e0i =
                        m_original_edge_index_map.at(Edge(new_F(t, ti + 1), new_F(t, ti + 2)));

                    m_edge_to_vertices[2 * e0i + 0] = static_cast<unsigned int>(3 * t + 1);
                    m_edge_to_vertices[2 * e0i + 1] = static_cast<unsigned int>(3 * t + 2);
                }

                // Last triangle - last two edges
                {
                    auto t = triangles.back();
                    auto e0i = m_original_edge_index_map.at(Edge(new_F(t, 1), new_F(t, 2)));
                    auto e1i = m_original_edge_index_map.at(Edge(new_F(t, 2), new_F(t, 0)));
                    m_edge_to_vertices[2 * e0i + 0] = static_cast<unsigned int>(3 * t + 1);
                    m_edge_to_vertices[2 * e0i + 1] = static_cast<unsigned int>(3 * t + 2);

                    m_edge_to_vertices[2 * e1i + 0] = static_cast<unsigned int>(3 * t + 2);
                    m_edge_to_vertices[2 * e1i + 1] = static_cast<unsigned int>(3 * t + 0);
                }
            }
        }
    }


    // Normals
    // Prefer corner normals if defined
    if (orig_mesh.has_corner_attribute("normal")) {
        proxy_mesh.add_corner_attribute("normal");

        // If original was not triangular mesh
        if (is_triangulated()) {
            // Flatten based on triangulation first
            proxy_mesh.set_corner_attribute(
                "normal",
                flatten_corner_attribute(orig_mesh.get_corner_attribute("normal"))
                    .template cast<TriangleMesh3Df::Scalar>());
        } else {
            // Straight copy and cast
            proxy_mesh.set_corner_attribute(
                "normal",
                orig_mesh.get_corner_attribute("normal").template cast<TriangleMesh3Df::Scalar>());
        }
    }
    // Otherwise try per vertex normals
    else if (orig_mesh.has_vertex_attribute("normal")) {
        proxy_mesh.add_vertex_attribute("normal");
        proxy_mesh.set_vertex_attribute(
            "normal",
            orig_mesh.get_vertex_attribute("normal").template cast<TriangleMesh3Df::Scalar>());
    }
    // If none defined, compute corner normals
    else {
        lagrange::compute_normal(proxy_mesh, pi() * 0.25f);
        lagrange::map_indexed_attribute_to_corner_attribute(proxy_mesh, "normal");
    }

    auto uvs = util::process_uvs(*this, orig_mesh);
    if (uvs.rows() > 0) {
        proxy_mesh.add_corner_attribute("uv");
        proxy_mesh.import_corner_attribute("uv", uvs);
        util::compute_corner_tangent_bitangent(proxy_mesh);
    }

    // Material IDs
    if (orig_mesh.has_facet_attribute("material_id")) {
        auto& orig_mat_ids = orig_mesh.get_facet_attribute("material_id");

        // Find unique ids
        for (auto fi = 0; fi < orig_mat_ids.size(); fi++) {
            auto& indices = m_material_indices[static_cast<int>(orig_mat_ids(fi))];
            auto triangles = polygon_triangles(int(fi));
            for (auto t : triangles) {
                indices.push_back(t);
            }
        }

        // Only one unique material, release the memory and use implicit indexing
        if (m_material_indices.size() == 1) {
            m_material_indices.clear();
        }
    }


    //Compute bounds
    m_bounds = AABB(m_mesh->get_vertices().colwise().minCoeff(), m_mesh->get_vertices().colwise().maxCoeff());

}


template <typename T>
using Dynamic = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

template <typename T>
using DynamicMesh = Mesh<Dynamic<T>, Dynamic<Triangles::Scalar>>;

/// Specializations for common lagrange types
template ProxyMesh::ProxyMesh(const Mesh<Vertices3Df, Triangles>& mesh);
template ProxyMesh::ProxyMesh(const Mesh<Vertices3D, Triangles>& mesh);
template ProxyMesh::ProxyMesh(const Mesh<Vertices3Df, Quads>& mesh);
template ProxyMesh::ProxyMesh(const Mesh<Vertices3D, Quads>& mesh);
template ProxyMesh::ProxyMesh(const Mesh<Vertices2Df, Triangles>& mesh);
template ProxyMesh::ProxyMesh(const Mesh<Vertices2D, Triangles>& mesh);
template ProxyMesh::ProxyMesh(const Mesh<Vertices2Df, Quads>& mesh);
template ProxyMesh::ProxyMesh(const Mesh<Vertices2D, Quads>& mesh);
template ProxyMesh::ProxyMesh(const Mesh<Eigen::MatrixXf, Eigen::MatrixXi>& mesh);
template ProxyMesh::ProxyMesh(const DynamicMesh<float>& mesh);
template ProxyMesh::ProxyMesh(const DynamicMesh<double>& mesh);


struct ProxyMeshAccelImpl
{
    using TreeNode = igl::AABB<typename ProxyMesh::VertexArray, 3>;
    igl::AABB<typename ProxyMesh::VertexArray, 3> tree;
    void init(const ProxyMesh& pm) { tree.init(pm.mesh().get_vertices(), pm.mesh().get_facets()); }
};


ProxyMesh::~ProxyMesh() {}

AABB ProxyMesh::get_bounds() const
{
    return m_bounds;
}

ProxyMesh::Index ProxyMesh::proxy_to_orig_facet(Index i) const
{
    if (static_cast<size_t>(i) < m_triangulation_prefix_sum.size()) {
        return i;
    }

    return m_new_triangle_to_orig[i - m_triangulation_prefix_sum.size()];
}


std::vector<ProxyMesh::Index> ProxyMesh::polygon_triangles(Index i) const
{
    std::vector<Index> indices;
    indices.push_back(i);

    // Extract number of extra triangles from prefix sum
    Index num_t = (i > 0) ? (m_triangulation_prefix_sum[i] - m_triangulation_prefix_sum[i - 1])
                          : m_triangulation_prefix_sum[i];

    // Get index of extra triangles
    auto start_index = m_triangulation_prefix_sum.size() + m_triangulation_prefix_sum[i] - 1;
    for (Index k = 0; k < num_t; k++) {
        indices.push_back(safe_cast<Index>(start_index + k));
    }

    return indices;
}


void ProxyMesh::init_acceleration() const
{
    m_accel_impl = std::make_unique<ProxyMeshAccelImpl>();
    m_accel_impl->init(*this);
}

bool ProxyMesh::get_proxy_facet_at(
    Eigen::Vector3f origin,
    Eigen::Vector3f dir,
    int& facet_id_out,
    float& t_out,
    Eigen::Vector3f& barycentric_out) const
{
    if (!m_picking_enabled) {
        return false;
    }

    // Lazily initialize acceleration structure for picking
    if (!m_accel_impl) {
        init_acceleration();
    }

    igl::Hit hit;
    bool res = m_accel_impl->tree.intersect_ray(
        mesh().get_vertices(),
        mesh().get_facets(),
        origin,
        dir,
        std::numeric_limits<Scalar>::infinity(),
        hit);

    if (!res) return false;

    facet_id_out = hit.id;

    barycentric_out = Eigen::Vector3f(1.0f - hit.u - hit.v, hit.u, hit.v);
    t_out = hit.t;

    return true;
}


bool ProxyMesh::get_original_facet_at(
    Eigen::Vector3f origin,
    Eigen::Vector3f dir,
    int& facet_id_out,
    float& t_out,
    Eigen::Vector3f& barycentric_out) const
{
    bool res = get_proxy_facet_at(origin, dir, facet_id_out, t_out, barycentric_out);
    if (res) facet_id_out = proxy_to_orig_facet(facet_id_out);
    return res;
}


std::unordered_set<int>
ProxyMesh::get_facets_in_frustum(const Frustum& f, bool ignore_backfacing, bool proxy_indices) const
{
    std::unordered_set<int> result;
    std::function<void(ProxyMeshAccelImpl::TreeNode*, bool)> traverse;
    std::function<void(int)> inserter;

    const auto& V = get_vertices();
    const auto& F = get_facets();

    if (!m_picking_enabled) {
        return {};
    }

    // Lazily initialize acceleration structure for picking
    if (!m_accel_impl) {
        init_acceleration();
    }

    if (proxy_indices) {
        inserter = [&](int x) -> void {
            if (ignore_backfacing &&
                f.is_backfacing(V.row(F(x, 0)), V.row(F(x, 1)), V.row(F(x, 2)))) {
                return;
            }
            result.insert(x);
        };
    } else {
        inserter = [&](int x) -> void {
            if (ignore_backfacing &&
                f.is_backfacing(V.row(F(x, 0)), V.row(F(x, 1)), V.row(F(x, 2)))) {
                return;
            }
            result.insert(proxy_to_orig_facet(x));
        };
    }

    auto near_phase = [&](int f_index) {
        return f.intersects(V.row(F(f_index, 0)), V.row(F(f_index, 1)), V.row(F(f_index, 2)));
    };

    traverse = [&](ProxyMeshAccelImpl::TreeNode* node, bool fully_inside) {
        if (!node) return;

        if (!fully_inside) {
            if (!f.intersects(node->m_box, fully_inside)) return;
        }

        if (node->is_leaf()) {
            if (fully_inside || near_phase(node->m_primitive)) inserter(node->m_primitive);
        } else {
            traverse(node->m_left, fully_inside);
            traverse(node->m_right, fully_inside);
        }
    };

    traverse(&m_accel_impl->tree, false);

    return result;
}


std::unordered_set<int> ProxyMesh::get_vertices_in_frustum(const Frustum& f, bool ignore_backfacing)
    const
{
    std::unordered_set<int> result;
    std::function<void(ProxyMeshAccelImpl::TreeNode*, bool)> traverse;
    std::function<void(int, bool)> inserter;

    const auto& V = get_vertices();
    const auto& F = get_facets();

    if (!m_picking_enabled) {
        return {};
    }

    // Lazily initialize acceleration structure for picking
    if (!m_accel_impl) {
        init_acceleration();
    }

    inserter = [&](int fi, bool inside) -> void {
        if (ignore_backfacing &&
            f.is_backfacing(V.row(F(fi, 0)), V.row(F(fi, 1)), V.row(F(fi, 2)))) {
            return;
        }

        if (inside) {
            for (auto k = 0; k < 3; k++) {
                result.insert(F(fi, k));
            }
        } else {
            for (auto k = 0; k < 3; k++) {
                if (!f.contains(V.row(F(fi, k)))) continue;
                result.insert(F(fi, k));
            }
        }
    };


    traverse = [&](ProxyMeshAccelImpl::TreeNode* node, bool fully_inside) {
        if (!node) return;

        if (!fully_inside) {
            if (!f.intersects(node->m_box, fully_inside)) return;
        }

        if (node->is_leaf()) {
            inserter(node->m_primitive, fully_inside);
        } else {
            traverse(node->m_left, fully_inside);
            traverse(node->m_right, fully_inside);
        }
    };

    traverse(&m_accel_impl->tree, false);


    return result;
}

std::unordered_set<int> ProxyMesh::get_edges_in_frustum(const Frustum& f, bool ignore_backfacing)
    const
{
    if (m_original_edges.size() == 0) return {};

    std::unordered_set<int> result;
    std::function<void(ProxyMeshAccelImpl::TreeNode*, bool)> traverse;
    std::function<void(int, bool)> inserter;

    const auto& V = get_vertices();
    const auto& F = get_facets();

    if (!m_picking_enabled) {
        return {};
    }

    // Lazily initialize acceleration structure for picking
    if (!m_accel_impl) {
        init_acceleration();
    }

    inserter = [&](int fi, bool inside) -> void {
        if (ignore_backfacing &&
            f.is_backfacing(V.row(F(fi, 0)), V.row(F(fi, 1)), V.row(F(fi, 2)))) {
            return;
        }

        if (inside) {
            for (auto k = 0; k < 3; k++) {
                auto it = m_original_edge_index_map.find(Edge(F(fi, k), F(fi, (k + 1) % 3)));
                if (it != m_original_edge_index_map.end()) result.insert(it->second);
            }
        } else {
            for (auto k = 0; k < 3; k++) {
                auto e = Edge(F(fi, k), F(fi, (k + 1) % 3));
                if (!f.intersects(V.row(e.v1()), V.row(e.v2()))) continue;
                auto it = m_original_edge_index_map.find(e);
                if (it != m_original_edge_index_map.end()) result.insert(it->second);
            }
        }
    };


    traverse = [&](ProxyMeshAccelImpl::TreeNode* node, bool fully_inside) {
        if (!node) return;

        if (!fully_inside) {
            if (!f.intersects(node->m_box, fully_inside)) return;
        }

        if (node->is_leaf()) {
            inserter(node->m_primitive, fully_inside);
        } else {
            traverse(node->m_left, fully_inside);
            traverse(node->m_right, fully_inside);
        }
    };

    traverse(&m_accel_impl->tree, false);


    return result;
}

bool ProxyMesh::intersects(const Frustum& f) const
{
    std::unordered_set<int> result;
    std::function<bool(ProxyMeshAccelImpl::TreeNode*, bool)> traverse;


    const auto& V = get_vertices();
    const auto& F = get_facets();

    if (!m_picking_enabled) {
        return false;
    }

    // Lazily initialize acceleration structure for picking
    if (!m_accel_impl) {
        init_acceleration();
    }

    auto near_phase = [&](int f_index) {
        return f.intersects(V.row(F(f_index, 0)), V.row(F(f_index, 1)), V.row(F(f_index, 2)));
    };

    traverse = [&](ProxyMeshAccelImpl::TreeNode* node, bool fully_inside) -> bool {
        if (!node) return false;

        if (!fully_inside) {
            if (!f.intersects(node->m_box, fully_inside)) return false;
        }

        if (node->is_leaf()) {
            if (fully_inside || near_phase(node->m_primitive)) return true;
        } else {
            return traverse(node->m_left, fully_inside) || traverse(node->m_right, fully_inside);
        }
        return false;
    };

    return traverse(&m_accel_impl->tree, false);
}

AABB ProxyMesh::get_selection_bounds(const ElementSelection& sel) const
{
    AABB bb;

    if (!m_picking_enabled) {
        return bb;
    }

    const auto& V = get_vertices();

    if (sel.get_type() == SelectionElementType::VERTEX) {
        for (auto i : sel.get_persistent().get_selection()) {
            bb.extend(V.row(i).transpose());
        }
    } else if (sel.get_type() == SelectionElementType::EDGE) {
        if (m_original_edges.size() == 0) return {};

        for (auto i : sel.get_persistent().get_selection()) {
            auto& e = m_original_edges[i];
            bb.extend(V.row(e.v1()).transpose());
            bb.extend(V.row(e.v2()).transpose());
        }
    } else if (sel.get_type() == SelectionElementType::FACE) {
        const auto& F = get_facets();
        for (auto i : sel.get_persistent().get_selection()) {
            auto tris = polygon_triangles(i);
            for (auto& t : tris) {
                bb.extend(V.row(F(t, 0)).transpose());
                bb.extend(V.row(F(t, 1)).transpose());
                bb.extend(V.row(F(t, 2)).transpose());
            }
        }
    }
    return bb;
}

void ProxyMesh::set_picking_enabled(bool value)
{
    m_picking_enabled = value;
}

bool ProxyMesh::is_picking_enabled() const
{
    return m_picking_enabled;
}

} // namespace ui
} // namespace lagrange
