/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/testing/common.h>
#include <lagrange/views.h>
#include <lagrange/winding/FastWindingNumber.h>

#include <UT_SolidAngle.h>
#include <Eigen/Geometry>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <random>

namespace {

// Direct wrapper, no PIMPL. We do this to measure the impact of the PIMPL idiom on the query times.
class FastWindingNumberDirect
{
public:
    template <typename DerivedV, typename DerivedF>
    void initialize(
        const Eigen::MatrixBase<DerivedV>& vertices,
        const Eigen::MatrixBase<DerivedF>& facets)
    {
        la_runtime_assert(vertices.cols() == 3);
        la_runtime_assert(facets.cols() == 3);

        // TODO: Avoid copy if possible. For now we just copy stuff around.
        m_vertices.resize(vertices.rows());
        for (Eigen::Index v = 0; v < vertices.rows(); ++v) {
            m_vertices[v][0] = static_cast<float>(vertices(v, 0));
            m_vertices[v][1] = static_cast<float>(vertices(v, 1));
            m_vertices[v][2] = static_cast<float>(vertices(v, 2));
        }

        m_triangles.resize(facets.rows());
        for (Eigen::Index f = 0; f < facets.rows(); ++f) {
            m_triangles[f][0] = static_cast<int>(facets(f, 0));
            m_triangles[f][1] = static_cast<int>(facets(f, 1));
            m_triangles[f][2] = static_cast<int>(facets(f, 2));
        }

        const int num_vertices = static_cast<int>(m_vertices.size());
        const int num_triangles = static_cast<int>(m_triangles.size());
        const int* const triangles_ptr = reinterpret_cast<const int*>(m_triangles.data());
        m_engine.init(num_triangles, triangles_ptr, num_vertices, m_vertices.data());
    }

    bool is_inside(const Eigen::Vector3f& pos) const
    {
        Vector q;
        q[0] = pos.x();
        q[1] = pos.y();
        q[2] = pos.z();
        return m_engine.computeSolidAngle(q) / (4.f * M_PI) > 0.5f;
    }

protected:
    using Vector = HDK_Sample::UT_Vector3T<float>;
    using Engine = HDK_Sample::UT_SolidAngle<float, float>;

    std::vector<Vector> m_vertices;
    std::vector<std::array<int, 3>> m_triangles;
    Engine m_engine;
};

} // namespace

// Make metabuild happy
TEST_CASE("winding number noop", "[winding]")
{
    SUCCEED();
}

TEST_CASE("fast winding number", "[winding][!benchmark]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    Eigen::AlignedBox<Scalar, 3> bbox;
    for (auto p : vertex_view(mesh).rowwise()) {
        bbox.extend(p.transpose());
    }
    std::uniform_real_distribution<Scalar> px(bbox.min().x(), bbox.max().x());
    std::uniform_real_distribution<Scalar> py(bbox.min().y(), bbox.max().y());
    std::uniform_real_distribution<Scalar> pz(bbox.min().z(), bbox.max().z());

    size_t num_samples = 10000;

    BENCHMARK_ADVANCED("pimpl wrapper")(Catch::Benchmark::Chronometer meter)
    {
        lagrange::winding::FastWindingNumber engine(mesh);
        std::mt19937 gen;
        meter.measure([&]() {
            size_t num_insides = 0;
            for (size_t k = 0; k < num_samples; ++k) {
                num_insides += engine.is_inside({px(gen), py(gen), pz(gen)});
            }
            return num_insides;
        });
    };

    BENCHMARK_ADVANCED("direct wrapper")(Catch::Benchmark::Chronometer meter)
    {
        FastWindingNumberDirect engine;
        engine.initialize(vertex_view(mesh), facet_view(mesh));
        std::mt19937 gen;
        meter.measure([&]() {
            size_t num_insides = 0;
            for (size_t k = 0; k < num_samples; ++k) {
                num_insides += engine.is_inside({px(gen), py(gen), pz(gen)});
            }
            return num_insides;
        });
    };
}
