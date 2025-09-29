/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/compute_mesh_covariance.h>

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/tbb.h>
#include <lagrange/views.h>

#include <Eigen/Core>

#include <optional>

namespace lagrange {

namespace {

// had to declare this class to comply with tbb::parallel_reduce
template <typename Scalar, typename Index>
class SumCovariancePerTriangleT
{
private:
    using SumCovariancePerTriangle = SumCovariancePerTriangleT<Scalar, Index>;

    using ConstEigenMapScalar = const Eigen::Map<
        const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;
    using ConstEigenMapIndex = const Eigen::Map<
        const Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;
    using Vector3RowMajor = Eigen::Matrix<Scalar, 1, 3, Eigen::RowMajor>;

    ConstEigenMapScalar ref_vertices;
    ConstEigenMapIndex ref_facets;
    const lagrange::span<const uint8_t> ref_active_facets_view;
    const Eigen::Matrix<Scalar, 3, 3>& ref_factors;
    const Vector3RowMajor& ref_center;

public:
    Eigen::Matrix<Scalar, 3, 3> summed_covariance = Eigen::Matrix<Scalar, 3, 3>::Zero();
    void operator()(const tbb::blocked_range<Index>& range)
    {
        for (Index facet_id = range.begin(); facet_id < range.end(); ++facet_id) {
            if (ref_active_facets_view.empty() || ref_active_facets_view[facet_id]) {
                // compute triangle covariance for facet_id
                const Vector3RowMajor v0 = ref_vertices.row(ref_facets(facet_id, 0));
                const Vector3RowMajor v1 = ref_vertices.row(ref_facets(facet_id, 1));
                const Vector3RowMajor v2 = ref_vertices.row(ref_facets(facet_id, 2));
                const Scalar a = (v1 - v0).cross(v2 - v0).norm();
                Eigen::Matrix<Scalar, 3, 3> p;
                p.row(0) = v0 - ref_center;
                p.row(1) = v2 - v0;
                p.row(2) = v1 - v2;
                summed_covariance += a * p.transpose() * ref_factors * p;
            }
        }
    }

    void join(const SumCovariancePerTriangle& other)
    {
        summed_covariance += other.summed_covariance;
    }

    SumCovariancePerTriangleT(
        ConstEigenMapScalar vertices_in,
        ConstEigenMapIndex facets_in,
        const lagrange::span<const uint8_t> active_facets_view_in,
        const Eigen::Matrix<Scalar, 3, 3>& factors_in,
        const Vector3RowMajor& center_in)
        : ref_vertices(vertices_in)
        , ref_facets(facets_in)
        , ref_active_facets_view(active_facets_view_in)
        , ref_factors(factors_in)
        , ref_center(center_in)
    {}

    SumCovariancePerTriangleT(const SumCovariancePerTriangle& other, tbb::split)
        : ref_vertices(other.ref_vertices)
        , ref_facets(other.ref_facets)
        , ref_active_facets_view(other.ref_active_facets_view)
        , ref_factors(other.ref_factors)
        , ref_center(other.ref_center)
    {}
};

} // namespace

template <typename Scalar, typename Index>
std::array<std::array<Scalar, 3>, 3> compute_mesh_covariance(
    const SurfaceMesh<Scalar, Index>& mesh,
    const MeshCovarianceOptions& options)
{
    using ConstEigenMapScalar = const Eigen::Map<
        const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;
    using ConstEigenMapIndex = const Eigen::Map<
        const Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;
    la_runtime_assert(mesh.get_dimension() == 3, "Currently, only 3 dimensions are supported");
    la_runtime_assert(mesh.is_triangle_mesh(), "Currently, only triangles are supported");

    const Eigen::RowVector<Scalar, 3> center(
        static_cast<Scalar>(options.center[0]),
        static_cast<Scalar>(options.center[1]),
        static_cast<Scalar>(options.center[2]));
    using Matrix3 = Eigen::Matrix<Scalar, 3, 3>; // This one is by default ColMajor

    ConstEigenMapScalar vertices = vertex_view(mesh);
    ConstEigenMapIndex facets = facet_view(mesh);

    lagrange::span<const uint8_t> active_facets_view; // default to empty
    if (options.active_facets_attribute_name.has_value()) {
        active_facets_view =
            mesh.template get_attribute<uint8_t>(options.active_facets_attribute_name.value())
                .get_all();
    }

    Matrix3 factors;
    factors.row(0) << Scalar(1. / 2), Scalar(1. / 3), Scalar(1. / 6);
    factors.row(1) << Scalar(1. / 3), Scalar(1. / 4), Scalar(1. / 8);
    factors.row(2) << Scalar(1. / 6), Scalar(1. / 8), Scalar(1. / 12);

    using SumCovariancePerTriangle = SumCovariancePerTriangleT<Scalar, Index>;
    SumCovariancePerTriangle sum_covariance(vertices, facets, active_facets_view, factors, center);
    tbb::parallel_reduce(tbb::blocked_range<Index>(0, mesh.get_num_facets()), sum_covariance);

    std::array<std::array<Scalar, 3>, 3> return_value;
    std::copy(
        sum_covariance.summed_covariance.data(),
        sum_covariance.summed_covariance.data() + 3,
        return_value[0].data());
    std::copy(
        sum_covariance.summed_covariance.data() + 3,
        sum_covariance.summed_covariance.data() + 6,
        return_value[1].data());
    std::copy(
        sum_covariance.summed_covariance.data() + 6,
        sum_covariance.summed_covariance.data() + 9,
        return_value[2].data());
    return return_value;
}

#define LA_X_compute_mesh_covariance(_, Scalar, Index)                                 \
    template LA_CORE_API std::array<std::array<Scalar, 3>, 3> compute_mesh_covariance( \
        const SurfaceMesh<Scalar, Index>& mesh,                                        \
        const MeshCovarianceOptions& options);
LA_SURFACE_MESH_X(compute_mesh_covariance, 0)


} // namespace lagrange
