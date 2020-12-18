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
#include <lagrange/create_mesh.h>

#include <lagrange/Edge.h>
#include <lagrange/common.h>
#include <lagrange/compute_facet_area.h>
#include <lagrange/utils/safe_cast.h>

std::unique_ptr<lagrange::Mesh<lagrange::Vertices3D, lagrange::Triangles>> lagrange::create_cube()
{
    lagrange::Vertices3D vertices(8, 3);
    vertices.row(0) << -1, -1, -1;
    vertices.row(1) << 1, -1, -1;
    vertices.row(2) << 1, 1, -1;
    vertices.row(3) << -1, 1, -1;
    vertices.row(4) << -1, -1, 1;
    vertices.row(5) << 1, -1, 1;
    vertices.row(6) << 1, 1, 1;
    vertices.row(7) << -1, 1, 1;

    lagrange::Triangles facets(12, 3);
    facets << 0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 1, 2, 6, 1, 6, 5, 3, 0, 7, 7, 0, 4, 2, 3, 7, 2, 7,
        6, 0, 1, 4, 4, 1, 5;

    lagrange::Vertices2D uvs(14, 2);
    uvs << 0.25, 0.0, 0.5, 0.0, 0.0, 0.25, 0.25, 0.25, 0.5, 0.25, 0.75, 0.25, 0.0, 0.5, 0.25, 0.5,
        0.5, 0.5, 0.75, 0.5, 0.25, 0.75, 0.5, 0.75, 0.25, 1.0, 0.5, 1.0;
    lagrange::Triangles uv_indices(12, 3);
    uv_indices << 10, 13, 11, 10, 12, 13, 7, 8, 4, 7, 4, 3, 9, 5, 4, 9, 4, 8, 2, 6, 3, 3, 6, 7, 1,
        0, 3, 1, 3, 4, 10, 11, 7, 7, 11, 8;
    auto mesh = lagrange::create_mesh(vertices, facets);
    mesh->initialize_uv(uvs, uv_indices);
    return mesh;
}

std::unique_ptr<lagrange::Mesh<lagrange::Vertices3D, lagrange::Triangles>> lagrange::create_quad(
    const bool with_center_vertex)
{
    std::unique_ptr<lagrange::Mesh<lagrange::Vertices3D, lagrange::Triangles>> mesh = nullptr;

    if (with_center_vertex) {
        lagrange::Vertices3D vertices(5, 3);
        vertices << -1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0, 0, 0, 0;

        lagrange::Triangles facets(4, 3);
        facets << 0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4;

        mesh = lagrange::create_mesh(vertices, facets);
    } else {
        lagrange::Vertices3D vertices(4, 3);
        vertices << -1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0;

        lagrange::Triangles facets(2, 3);
        facets << 0, 1, 2, 0, 2, 3;

        mesh = lagrange::create_mesh(vertices, facets);
    }

    assert(mesh);
    lagrange::compute_facet_area(*mesh);

    return mesh;
}

std::unique_ptr<lagrange::Mesh<lagrange::Vertices3D, lagrange::Triangles>> lagrange::create_sphere(
    typename lagrange::Triangles::Scalar refine_order)
{
    using MeshType = TriangleMesh3D;
    using Index = typename MeshType::Index;
    lagrange::Vertices3D vertices(12, 3);
    double t = (1.0 + sqrt(5.0)) / 2.0;
    vertices << -1, t, 0, 1, t, 0, -1, -t, 0, 1, -t, 0, 0, -1, t, 0, 1, t, 0, -1, -t, 0, 1, -t, t,
        0, -1, t, 0, 1, -t, 0, -1, -t, 0, 1;
    lagrange::Triangles facets(20, 3);
    facets << 0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7,
        6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6,
        7, 9, 8, 1;

    auto refine = [](MeshType& mesh) {
        const auto& coarse_vertices = mesh.get_vertices();
        const auto& coarse_facets = mesh.get_facets();
        EdgeMap<Index, Index> edge_map;
        const Index num_vertices = mesh.get_num_vertices();
        const Index num_facets = mesh.get_num_facets();
        Index count = 0;
        for (Index i = 0; i < num_facets; i++) {
            Eigen::Matrix<Index, 3, 1> f = coarse_facets.row(i);
            EdgeType<Index> e01{{f[0], f[1]}};
            EdgeType<Index> e12{{f[1], f[2]}};
            EdgeType<Index> e20{{f[2], f[0]}};
            if (edge_map.find(e01) == edge_map.end()) {
                edge_map.insert({e01, count});
                count++;
            }
            if (edge_map.find(e12) == edge_map.end()) {
                edge_map.insert({e12, count});
                count++;
            }
            if (edge_map.find(e20) == edge_map.end()) {
                edge_map.insert({e20, count});
                count++;
            }
        }

        const Index num_edges = safe_cast<Index>(edge_map.size());
        Vertices3D new_vertices(num_vertices + num_edges, 3);
        new_vertices.topRows(num_vertices) = coarse_vertices;
        for (const auto& entry : edge_map) {
            const Index v0 = safe_cast<Index>(entry.first[0]);
            const Index v1 = safe_cast<Index>(entry.first[1]);
            const Index vid = entry.second;
            new_vertices.row(vid + num_vertices) =
                0.5 * (coarse_vertices.row(v0) + coarse_vertices.row(v1));
        }

        Triangles new_facets(4 * num_facets, 3);
        for (Index i = 0; i < num_facets; i++) {
            Eigen::Matrix<Index, 3, 1> f = coarse_facets.row(i);
            Index v01 = safe_cast<Index>(num_vertices + edge_map.find({f[0], f[1]})->second);
            Index v12 = safe_cast<Index>(num_vertices + edge_map.find({f[1], f[2]})->second);
            Index v20 = safe_cast<Index>(num_vertices + edge_map.find({f[2], f[0]})->second);
            new_facets.row(i * 4) << f[0], v01, v20;
            new_facets.row(i * 4 + 1) << f[1], v12, v01;
            new_facets.row(i * 4 + 2) << f[2], v20, v12;
            new_facets.row(i * 4 + 3) << v01, v12, v20;
        }

        return create_mesh(new_vertices, new_facets);
    };

    auto normalize = [](MeshType& mesh) {
        const Index num_vertices = mesh.get_num_vertices();
        typename MeshType::VertexArray tmp_vertices;
        mesh.export_vertices(tmp_vertices);
        for (Index i = 0; i < num_vertices; i++) {
            tmp_vertices.row(i) /= tmp_vertices.row(i).norm();
        }
        mesh.import_vertices(tmp_vertices);
    };

    auto mesh = lagrange::create_mesh(vertices, facets);

    for (Index i = 0; i < refine_order; i++) {
        mesh = refine(*mesh);
    }
    normalize(*mesh);
    return mesh;
}
