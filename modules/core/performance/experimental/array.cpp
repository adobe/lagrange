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
#include <lagrange/Mesh.h>
#include <lagrange/experimental/Array.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/timing.h>
#include <iostream>

#ifndef EIGEN_VECTORIZE
    #warning "Vectorization is disabled"
#endif

template <typename DerivedV, typename DerivedF>
auto compute_centroids(
    const Eigen::MatrixBase<DerivedV>& vertices,
    const Eigen::MatrixBase<DerivedF>& facets)
{
    using Scalar = typename DerivedV::Scalar;
    constexpr int N = 10;

    const auto num_facets = facets.rows();
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> centroids(num_facets, 3);

    auto start = lagrange::get_timestamp();
    for (int j = 0; j < N; j++) {
        for (auto i : lagrange::range(num_facets)) {
            centroids.row(i) = (vertices.row(facets(i, 0)) + vertices.row(facets(i, 1)) +
                                vertices.row(facets(i, 2))) /
                               3;
        }
    }
    auto end = lagrange::get_timestamp();

    return lagrange::timestamp_diff_in_seconds(start, end) / N;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " input_mesh" << std::endl;
        return 1;
    }

    void* mem = malloc(1);
    std::cout << mem << std::endl;

    Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic> test(1, 1);
    std::cout << test.data() << std::endl;

    using VertexArray = Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using FacetArray = Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using MeshType = lagrange::Mesh<VertexArray, FacetArray>;
    auto mesh = lagrange::io::load_mesh<MeshType>(argv[1]);
    std::cout << mesh->get_num_vertices() << std::endl;
    std::cout << mesh->get_num_facets() << std::endl;

    Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor | Eigen::AutoAlign> vertices =
        mesh->get_vertices();
    Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor | Eigen::AutoAlign> facets =
        mesh->get_facets();

    auto t1 = compute_centroids(vertices, facets);
    std::cout << vertices.data() << "\t" << facets.data() << std::endl;
    std::cout << "Average duration (Eigen Matrix, aligned): " << t1 << "s" << std::endl;

    auto vertex_array = lagrange::experimental::create_array(mesh->get_vertices());
    auto facet_array = lagrange::experimental::create_array(mesh->get_facets());
    auto vertices_view = vertex_array->view<VertexArray>();
    auto facets_view = facet_array->view<FacetArray>();

    auto t2 = compute_centroids(vertices_view, facets_view);
    std::cout << vertices_view.data() << "\t" << facets_view.data() << std::endl;
    std::cout << "Average duration (Array, unaligned): " << t2 << "s" << std::endl;

    free(mem);
    return 0;
}
