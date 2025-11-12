/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/geodesic/GeodesicEngineDGPC.h>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/common.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_vertex_valence.h>
#include <lagrange/geodesic/api.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/views.h>

#include <Eigen/Core>

#include <cmath>
#include <exception>
#include <limits>
#include <list>
#include <map>
#include <queue>
#include <vector>

namespace lagrange::geodesic {

template <typename Scalar, typename Index>
GeodesicEngineDGPC<Scalar, Index>::GeodesicEngineDGPC(Mesh& mesh)
    : Super(mesh)
{
    if (mesh.get_dimension() != 3) {
        throw std::runtime_error("Input mesh must be 3D mesh.");
    }
    if (!mesh.is_triangle_mesh()) {
        throw std::runtime_error("Input mesh must be triangle mesh.");
    }
    mesh.initialize_edges();
    m_facet_normal_attr_id = compute_facet_normal(mesh);
}

template <typename Scalar, typename Index>
SingleSourceGeodesicResult GeodesicEngineDGPC<Scalar, Index>::single_source_geodesic(
    const SingleSourceGeodesicOptions& options)
{
    auto normals = attribute_matrix_view<Scalar>(this->mesh(), m_facet_normal_attr_id);
    Scalar radius = static_cast<Scalar>(options.radius);
    if (radius <= 0.0) {
        radius = std::numeric_limits<Scalar>::max();
    }

    using HighResScalar = double;
    using HighResArray = Eigen::Matrix<HighResScalar, Eigen::Dynamic, 1, Eigen::ColMajor>;
    using HighResVector = Eigen::Matrix<HighResScalar, 3, 1>;
    const HighResScalar EPS = 1e-12;
    const Scalar INVALID = -1;
    Eigen::Matrix<HighResScalar, 3, 1> bc(
        1.0 - options.source_facet_bc[0] - options.source_facet_bc[1],
        options.source_facet_bc[0],
        options.source_facet_bc[1]); // A non-const copy.

    const auto vertices = vertex_view(this->mesh());
    const auto facets = facet_view(this->mesh());
    const Index num_vertices = this->mesh().get_num_vertices();
    HighResArray geodesic_distance(num_vertices, 1);
    HighResArray theta(num_vertices, 1);
    geodesic_distance.setConstant(INVALID);
    theta.setZero();
    HighResVector ref_dir_hr(options.ref_dir[0], options.ref_dir[1], options.ref_dir[2]);

    using Entry = std::pair<Index, HighResScalar>;
    auto comp = [](const Entry& e1, const Entry& e2) { return e1.second > e2.second; };
    std::priority_queue<Entry, std::vector<Entry>, decltype(comp)> Q(comp);

    auto initialize_from_facet = [&]() {
        const HighResVector seed_normal =
            normals.row(options.source_facet_id).template cast<HighResScalar>();
        ref_dir_hr = ref_dir_hr - ref_dir_hr.dot(seed_normal) * seed_normal;
        if (ref_dir_hr.norm() < EPS) {
            // The user specifies a ref direction that is parallel to the
            // normal.  Using the secondary reference direction.
            logger().warn("ref_dir is parallel to the seed facet normal. Using second_ref_dir.");
            ref_dir_hr << options.second_ref_dir[0], options.second_ref_dir[1],
                options.second_ref_dir[2];
            ref_dir_hr = ref_dir_hr - ref_dir_hr.dot(seed_normal) * seed_normal;
            la_runtime_assert(
                ref_dir_hr.norm() > EPS,
                "Both ref_dir and second_ref_dir are parallel to the seed facet normal.");
        }
        ref_dir_hr.stableNormalize();

        const Index i0 = facets(options.source_facet_id, 0);
        const Index i1 = facets(options.source_facet_id, 1);
        const Index i2 = facets(options.source_facet_id, 2);
        const HighResVector v0 = vertices.row(i0).template cast<HighResScalar>();
        const HighResVector v1 = vertices.row(i1).template cast<HighResScalar>();
        const HighResVector v2 = vertices.row(i2).template cast<HighResScalar>();
        HighResVector p = v0 * bc[0] + v1 * bc[1] + v2 * bc[2];

        geodesic_distance(i0, 0) = (p - v0).norm();
        geodesic_distance(i1, 0) = (p - v1).norm();
        geodesic_distance(i2, 0) = (p - v2).norm();
        theta(i0, 0) = atan2(ref_dir_hr.cross(v0 - p).dot(seed_normal), ref_dir_hr.dot(v0 - p));
        theta(i1, 0) = atan2(ref_dir_hr.cross(v1 - p).dot(seed_normal), ref_dir_hr.dot(v1 - p));
        theta(i2, 0) = atan2(ref_dir_hr.cross(v2 - p).dot(seed_normal), ref_dir_hr.dot(v2 - p));
        Q.push({i0, geodesic_distance(i0, 0)});
        Q.push({i1, geodesic_distance(i1, 0)});
        Q.push({i2, geodesic_distance(i2, 0)});
    };
    auto initialize_from_vertex = [&](Index seed_vertex_id) {
        std::vector<Index> one_ring;
        this->mesh().foreach_facet_around_vertex(seed_vertex_id, [&](Index fi) {
            one_ring.push_back(fi);
        });
        std::map<Index, Index> next_vertex;
        std::map<Index, Index> prev_vertex;
        for (const auto fi : one_ring) {
            Index f_vi = 0;
            for (f_vi = 0; f_vi < 3; f_vi++) {
                if (facets(fi, f_vi) == seed_vertex_id) {
                    break;
                }
            }
            la_runtime_assert(f_vi < 3);
            Index next_v = facets(fi, (f_vi + 1) % 3);
            Index prev_v = facets(fi, (f_vi + 2) % 3);
            la_runtime_assert(next_vertex.find(next_v) == next_vertex.end());
            next_vertex[next_v] = prev_v;
            prev_vertex[prev_v] = next_v;
        }
        // Chain one ring vertices together.
        // TODO: handle non-manifold vertices.
        // TODO: Can we use get_clockwise_corner_around_vertex() here?
        std::list<Index> one_ring_chain;
        const auto seed_facet = facets.row(options.source_facet_id);
        Index start_vertex_id = seed_facet[0];
        if (seed_vertex_id == seed_facet[0]) {
            start_vertex_id = seed_facet[1];
        }
        la_runtime_assert(start_vertex_id != seed_vertex_id);

        one_ring_chain.push_back(start_vertex_id);
        auto itr = next_vertex.find(start_vertex_id);
        while (itr != next_vertex.end() && itr->second != one_ring_chain.front()) {
            one_ring_chain.push_back(itr->second);
            itr = next_vertex.find(one_ring_chain.back());
        }
        itr = prev_vertex.find(start_vertex_id);
        while (itr != prev_vertex.end() && itr->second != one_ring_chain.back()) {
            one_ring_chain.push_front(itr->second);
            itr = prev_vertex.find(one_ring_chain.front());
        }
        la_runtime_assert(one_ring_chain.size() >= 2);
        bool on_boundary = next_vertex[one_ring_chain.back()] != one_ring_chain.front();
        const Index one_ring_size = safe_cast<Index>(one_ring_chain.size());
        Index start_vertex_local_id = 0;
        std::vector<HighResVector> one_ring_vertices;
        std::vector<Index> one_ring_indices;
        one_ring_vertices.reserve(one_ring_size);
        one_ring_indices.reserve(one_ring_size);
        for (const auto v : one_ring_chain) {
            one_ring_vertices.push_back(vertices.row(v).template cast<HighResScalar>());
            one_ring_indices.push_back(v);
            if (v == start_vertex_id) {
                start_vertex_local_id = safe_cast<Index>(one_ring_vertices.size()) - 1;
            }
        }

        const HighResVector start_vertex = one_ring_vertices[start_vertex_local_id];
        const HighResVector seed_normal =
            normals.row(options.source_facet_id).template cast<HighResScalar>();

        ref_dir_hr = ref_dir_hr - ref_dir_hr.dot(seed_normal) * seed_normal;
        if (ref_dir_hr.norm() < EPS) {
            // The user specifies a ref direction that is parallel to the
            // normal.  Sigh...  Guess a good tangent direction instead...
            if (std::abs(seed_normal[0]) < std::abs(seed_normal[1])) {
                ref_dir_hr = seed_normal.cross(HighResVector({1.0, 0.0, 0.0}));
            } else {
                ref_dir_hr = seed_normal.cross(HighResVector({0.0, 1.0, 0.0}));
            }
        } else {
            ref_dir_hr.normalize();
        }

        const HighResVector p = vertices.row(seed_vertex_id).template cast<HighResScalar>();
        std::vector<HighResScalar> angles(one_ring_size, 0);
        HighResScalar total_angle = 0.0;
        for (Index i = 0; i < one_ring_size; i++) {
            const HighResVector e_curr = one_ring_vertices[i] - p;
            const HighResVector e_next = one_ring_vertices[(i + 1) % one_ring_size] - p;
            if (i != one_ring_size - 1 || !on_boundary) {
                angles[i] = atan2(e_curr.cross(e_next).norm(), e_curr.dot(e_next));
                la_runtime_assert(angles[i] >= 0.0);
                total_angle += angles[i];
            }
        }
        la_runtime_assert(total_angle > 0.0);
        if (!on_boundary) {
            for (Index i = 0; i < one_ring_size; i++) {
                angles[i] = angles[i] / total_angle * 2 * M_PI;
            }
        }

        HighResScalar start_vertex_theta = atan2(
            ref_dir_hr.cross(start_vertex - p).dot(seed_normal),
            ref_dir_hr.dot(start_vertex - p));
        la_runtime_assert(std::isfinite(start_vertex_theta));
        HighResScalar angle_cumu = start_vertex_theta;
        for (Index i = 0; i < start_vertex_local_id; i++) {
            angle_cumu -= angles[i];
        }
        for (Index i = 0; i < one_ring_size; i++) {
            if (angle_cumu > M_PI) {
                angle_cumu -= 2 * M_PI;
            } else if (angle_cumu < -M_PI) {
                angle_cumu += 2 * M_PI;
            }

            geodesic_distance(one_ring_indices[i], 0) = (one_ring_vertices[i] - p).norm();
            la_runtime_assert(std::isfinite(angle_cumu));
            theta(one_ring_indices[i], 0) = angle_cumu;
            angle_cumu += angles[i];
            if (i != one_ring_size - 1 || !on_boundary) {
                Q.push({one_ring_indices[i], geodesic_distance(one_ring_indices[i], 0)});
            }
        }
        geodesic_distance(seed_vertex_id, 0) = 0.0;
        theta(seed_vertex_id, 0) = 0.0;
    };

    auto heron_area_stable =
        [](HighResScalar e0, HighResScalar e1, HighResScalar e2) -> HighResScalar {
        HighResScalar a, b, c;
        if (e0 > e2) {
            a = e0;
            b = e1;
        } else {
            a = e1;
            b = e0;
        }
        c = e2;
        HighResScalar r = (a + (b + c)) * (c - (a - b)) * (c + (a - b)) * (a + (b - c));
        if (r < 0.0) {
            return 0;
        }
        return sqrt(r);
    };

    auto try_compute_dgpc =
        [&vertices, &geodesic_distance, radius, EPS, INVALID, &theta, &heron_area_stable](
            Index vi,
            Index vj,
            Index vk) -> bool {
        const HighResScalar Uj = geodesic_distance(vj, 0);
        const HighResScalar Uk = geodesic_distance(vk, 0);
        if (Uj == INVALID || Uk == INVALID) {
            return false;
        }

        const HighResVector pi = vertices.row(vi).template cast<HighResScalar>();
        const HighResVector pj = vertices.row(vj).template cast<HighResScalar>();
        const HighResVector pk = vertices.row(vk).template cast<HighResScalar>();

        const HighResVector ekj = pk - pj;
        const HighResScalar lkj = ekj.norm();
        const HighResScalar H = heron_area_stable(Uj, Uk, lkj);
        la_runtime_assert(std::isfinite(H));

        const HighResVector ej = pj - pi;
        const HighResVector ek = pk - pi;
        const HighResScalar A = ej.cross(ek).norm();
        const HighResScalar xj = A * (lkj * lkj + Uk * Uk - Uj * Uj) + ek.dot(ekj) * H;
        const HighResScalar xk = A * (lkj * lkj + Uj * Uj - Uk * Uk) - ej.dot(ekj) * H;
        const HighResScalar dijk_through_j = Uj + ej.norm();
        const HighResScalar dijk_through_k = Uk + ek.norm();
        HighResScalar alpha = -1;
        HighResScalar Uijk = 0.0;
        if (xj > 0.0 && xk > 0.0) {
            Uijk = (xj * ej + xk * ek).norm() / (2 * A * lkj * lkj);
        } else {
            if (dijk_through_j < dijk_through_k) {
                alpha = 0.0;
                Uijk = dijk_through_j;
            } else {
                alpha = 1.0;
                Uijk = dijk_through_k;
            }
        }
        HighResScalar height = H * 0.25 / lkj;
        la_runtime_assert(Uijk > height);
        if (Uijk > radius && (height > radius || alpha >= 0)) {
            return false;
        }

        const HighResScalar curr_dist = geodesic_distance(vi, 0);
        if (curr_dist < 0.0 || (curr_dist / Uijk > 1.0 + EPS && curr_dist > Uijk + EPS)) {
            geodesic_distance(vi, 0) = Uijk;
            if (alpha == -1) {
                HighResScalar phi_ij_cos =
                    (Uj * Uj + Uijk * Uijk - ej.squaredNorm()) / (2 * Uj * Uijk);
                if (phi_ij_cos < -1.0)
                    phi_ij_cos = -1.0;
                else if (phi_ij_cos > 1.0)
                    phi_ij_cos = 1.0;
                const HighResScalar phi_ij = acos(phi_ij_cos);
                la_runtime_assert(std::isfinite(phi_ij));

                HighResScalar phi_ki_cos =
                    (Uk * Uk + Uijk * Uijk - ek.squaredNorm()) / (2 * Uk * Uijk);
                if (phi_ki_cos < -1.0)
                    phi_ki_cos = -1.0;
                else if (phi_ki_cos > 1.0)
                    phi_ki_cos = 1.0;
                const HighResScalar phi_ki = acos(phi_ki_cos);
                la_runtime_assert(std::isfinite(phi_ki));

                if (phi_ij < 1e-12 && phi_ki < 1e-12) {
                    alpha = 0.5;
                } else {
                    alpha = phi_ij / (phi_ij + phi_ki);
                }
                la_runtime_assert(std::isfinite(alpha));
            }

            const HighResScalar& tj = theta(vj, 0);
            const HighResScalar& tk = theta(vk, 0);
            if (std::abs(tk - tj) > M_PI) {
                theta(vi, 0) = (1.0 - alpha) * tj + alpha * tk +
                               ((tj < tk) ? (1.0 - alpha) : alpha) * 2 * M_PI;
                if (theta(vi, 0) > M_PI) theta(vi, 0) -= 2 * M_PI;
            } else {
                theta(vi, 0) = (1.0 - alpha) * tj + alpha * tk;
            }
            return true;
        }
        return false;
    };

    int min_index, max_index;
    if (bc.maxCoeff(&max_index) > 1.0 - EPS) {
        Index seed_vertex_id = facets(options.source_facet_id, max_index);
        initialize_from_vertex(seed_vertex_id);
    } else if (bc.minCoeff(&min_index) < EPS) {
        // Seed point is either very close to an edge or outside of the
        // triangle.  Modify the point so it is inside of the face.
        bc[min_index] = EPS;
        bc[(min_index + 1) % 3] = 1.0f - bc[(min_index + 2) % 3] - bc[min_index];
        la_runtime_assert(bc.minCoeff() >= 0.0);
        initialize_from_facet();
    } else {
        initialize_from_facet();
    }

    Index counter = 0;
    std::vector<Index> counters(num_vertices, 0);
    auto valence_attr_id = compute_vertex_valence(this->mesh());
    const auto& valence = attribute_matrix_view<Index>(this->mesh(), valence_attr_id);
    la_runtime_assert(!Q.empty());
    while (!Q.empty()) {
        const auto entry = Q.top();
        const auto v = entry.first;
        la_runtime_assert(entry.second != -1);
        la_runtime_assert(geodesic_distance(v, 0) != -1);
        Q.pop();
        counter++;
        if (entry.second > geodesic_distance(v, 0)) {
            continue;
        }
        counters[v]++;
        if (counters[v] > valence(v, 0)) continue;

        std::vector<Index> adj_facets;
        this->mesh().foreach_facet_around_vertex(v, [&](Index fi) { adj_facets.push_back(fi); });
        for (const auto fi : adj_facets) {
            const Eigen::Matrix<Index, 3, 1> f = facets.row(fi);
            if (f[0] == v) {
                if (try_compute_dgpc(f[1], v, f[2])) {
                    Q.push({f[1], geodesic_distance(f[1], 0)});
                }
                if (try_compute_dgpc(f[2], v, f[1])) {
                    Q.push({f[2], geodesic_distance(f[2], 0)});
                }
            } else if (f[1] == v) {
                if (try_compute_dgpc(f[0], v, f[2])) {
                    Q.push({f[0], geodesic_distance(f[0], 0)});
                }
                if (try_compute_dgpc(f[2], v, f[0])) {
                    Q.push({f[2], geodesic_distance(f[2], 0)});
                }
            } else {
                la_runtime_assert(f[2] == v);
                if (try_compute_dgpc(f[0], v, f[1])) {
                    Q.push({f[0], geodesic_distance(f[0], 0)});
                }
                if (try_compute_dgpc(f[1], v, f[0])) {
                    Q.push({f[1], geodesic_distance(f[1], 0)});
                }
            }
        }
    }

    auto geodesic_attr_id = internal::find_or_create_attribute<Scalar>(
        this->mesh(),
        options.output_geodesic_attribute_name,
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);
    attribute_vector_ref<Scalar>(this->mesh(), options.output_geodesic_attribute_name) =
        geodesic_distance.template cast<Scalar>();
    auto polar_angle_attr_id = internal::find_or_create_attribute<Scalar>(
        this->mesh(),
        options.output_polar_angle_attribute_name,
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);
    attribute_vector_ref<Scalar>(this->mesh(), options.output_polar_angle_attribute_name) =
        theta.template cast<Scalar>();

    return {geodesic_attr_id, polar_angle_attr_id};
}

#define LA_X_GeodesicEngineDGPC(_, Scalar, Index) \
    template class LA_GEODESIC_API GeodesicEngineDGPC<Scalar, Index>;
LA_SURFACE_MESH_X(GeodesicEngineDGPC, 0)

} // namespace lagrange::geodesic
