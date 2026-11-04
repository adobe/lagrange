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
////////////////////////////////////////////////////////////////////////////////
#include <lagrange/compute_facet_area.h>
#include <lagrange/sampling/random_sample_density_blue_noise.h>
#include <lagrange/sampling/random_sample_uniform.h>

#include <lagrange/common.h>
#include <lagrange/create_mesh.h>

#include <catch2/catch_test_macros.hpp>
////////////////////////////////////////////////////////////////////////////////

template <typename T>
using WrapVertexArray =
    Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, 3, Eigen::RowMajor>, Eigen::Unaligned>;

template <typename U>
using WrapFacetArray =
    Eigen::Map<const Eigen::Matrix<U, Eigen::Dynamic, 3, Eigen::RowMajor>, Eigen::Unaligned>;


template <typename T, typename U>
using WrapMesh = lagrange::Mesh<const WrapVertexArray<T>, const WrapFacetArray<U>>;


template <typename T, typename U>
std::unique_ptr<WrapMesh<T, U>>
wrap_with_mesh_map(const T* V, int num_vertices, const U* F, int num_triangles)
{
    using VertexMatrixType = Eigen::Matrix<T, Eigen::Dynamic, 3, Eigen::RowMajor>;
    auto V_map = Eigen::Map<const VertexMatrixType, Eigen::Unaligned>(
        reinterpret_cast<const T*>(&V[0]),
        Eigen::Index(num_vertices),
        Eigen::Index(3));

    using FacetMatrixType = Eigen::Matrix<U, Eigen::Dynamic, 3, Eigen::RowMajor>;
    auto F_map = Eigen::Map<const FacetMatrixType, Eigen::Unaligned>(
        reinterpret_cast<const U*>(&F[0]),
        Eigen::Index(num_triangles),
        Eigen::Index(3));

    auto geometry = std::make_unique<
        lagrange::GenuineMeshGeometry<const WrapVertexArray<T>, const WrapFacetArray<U>>>(
        std::move(V_map),
        std::move(F_map));

    return std::make_unique<WrapMesh<T, U>>(std::move(geometry));
}

namespace {

template <typename MeshType>
void test_sampling(MeshType& input)
{
    lagrange::compute_facet_area(input);

    REQUIRE(input.has_facet_attribute("area"));

    using Index = typename MeshType::Index;
    using Cloud = typename MeshType::VertexArray::PlainObject;
    using Indices = Eigen::Matrix<typename MeshType::Index, Eigen::Dynamic, 1>;
    using Densities = typename MeshType::AttributeArray;

    {
        using VertexArray = typename MeshType::VertexArray;
        using Plain = typename VertexArray::PlainObject;
        lagrange::logger().trace("MeshType {}", typeid(MeshType).name());
        lagrange::logger().trace("VertexArray {}", typeid(VertexArray).name());
        lagrange::logger().trace("Plain {}", typeid(Plain).name());
    }

    SECTION("Uniform, With facet indices")
    {
        std::mt19937 rng;
        const auto ret = lagrange::sampling::random_sample_uniform(input, 256, rng, true);

        const auto& cloud = std::get<0>(ret);
        REQUIRE(cloud.cols() == 3);
        REQUIRE(cloud.rows() == 256);
        for (Index kk = 0; kk < cloud.rows(); kk++) {
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 1) >= -1);
            REQUIRE(cloud(kk, 1) <= 1);
            REQUIRE(cloud(kk, 2) == 0);
        }

        const auto& indices = std::get<1>(ret);
        REQUIRE(indices.cols() == 1);
        REQUIRE(indices.rows() == 256);
        for (Index kk = 0; kk < indices.rows(); kk++) {
            REQUIRE(indices(kk) >= 0);
            REQUIRE(indices(kk) < input.get_num_facets());
        }
    }

    SECTION("Uniform blue noise")
    {
        std::mt19937 rng;
        const auto ret = lagrange::sampling::random_sample_density_blue_noise(
            input,
            1024,
            [](const Cloud& cloud, const Indices& /*indices*/) -> Densities {
                return Densities::Constant(cloud.rows(), 1, 1);
            },
            rng);

        const auto& status = std::get<0>(ret);
        const auto& cloud = std::get<1>(ret);
        const auto& densities = std::get<2>(ret);
        REQUIRE(status == lagrange::sampling::DensityStatus::Valid);
        REQUIRE(cloud.rows() == 1024);
        REQUIRE(cloud.cols() == 3);
        REQUIRE(densities.rows() == 1024);
        REQUIRE(densities.cols() == 1);

        for (Index kk = 0; kk < cloud.rows(); kk++) {
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 1) >= -1);
            REQUIRE(cloud(kk, 1) <= 1);
            REQUIRE(cloud(kk, 2) == 0);
        }
    }
}

} // namespace


TEST_CASE("Uniform sampling on wrap_with_mesh", "[sampling][wrap]")
{
    auto quad = lagrange::create_quad(false);
    auto input = lagrange::wrap_with_mesh(quad->get_vertices(), quad->get_facets());
    test_sampling(*input);
}

TEST_CASE("Uniform sampling on Mesh<Eigen::Map, ...>", "[sampling][wrap]")
{
    auto quad = lagrange::create_quad(false);
    auto input = wrap_with_mesh_map(
        quad->get_vertices().data(),
        quad->get_num_vertices(),
        quad->get_facets().data(),
        quad->get_num_facets());


    test_sampling(*input);
}
