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
#include <lagrange/texproc/extract_mesh_with_alpha_mask.h>

#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/utils/StackVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>

#include <Eigen/Geometry>

#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>

namespace lagrange::texproc {

template <typename Scalar, typename Callback>
void rasterize_triangle_data(
    const Eigen::Matrix<Scalar, 3, 2>& texcoords,
    const size_t width,
    const size_t height,
    Callback callback)
{
    const auto to_coordinate = [&](Eigen::Vector<Scalar, 2> uv) -> Eigen::Vector<size_t, 2> {
        auto ii = static_cast<size_t>(uv(0) * static_cast<Scalar>(width));
        auto jj = static_cast<size_t>(uv(1) * static_cast<Scalar>(height));
        ii = std::min(ii, width - 1);
        jj = std::min(jj, height - 1);
        la_debug_assert(ii < width);
        la_debug_assert(jj < height);
        return {ii, jj};
    };

    const auto from_coordinate =
        [&](const Eigen::Vector<size_t, 2>& ij) -> Eigen::Vector<Scalar, 2> {
        la_debug_assert(ij(0) < width);
        la_debug_assert(ij(1) < height);
        const auto uu = (static_cast<Scalar>(ij(0)) + 0.5) / static_cast<Scalar>(width);
        const auto vv = (static_cast<Scalar>(ij(1)) + 0.5) / static_cast<Scalar>(height);
        return {uu, vv};
    };

    const auto wedge = [](const Eigen::Vector<Scalar, 2>& xx, const Eigen::Vector<Scalar, 2>& yy) {
        return xx(0) * yy(1) - xx(1) * yy(0);
    };

    Eigen::AlignedBox<Scalar, 2> bbox_uv;
    for (auto uv : texcoords.rowwise()) {
        bbox_uv.extend(uv.transpose());
    }
    const auto coordinate_min = to_coordinate(bbox_uv.min());
    const auto coordinate_max = to_coordinate(bbox_uv.max());

    const Eigen::Vector<Scalar, 2> q0 = texcoords.row(0).transpose();
    const Eigen::Vector<Scalar, 2> q1 = texcoords.row(1).transpose();
    const Eigen::Vector<Scalar, 2> q2 = texcoords.row(2).transpose();
    const Scalar area = wedge(q1 - q0, q2 - q0);
    if (std::fabs(area) <= 1e-7) return;

    const auto barycentric_weights =
        [&](const Eigen::Vector<size_t, 2>& ij) -> Eigen::Vector<Scalar, 3> {
        const auto uv = from_coordinate(ij);
        la_debug_assert(uv(0) >= 0.0);
        la_debug_assert(uv(1) >= 0.0);
        la_debug_assert(uv(0) < 1.0);
        la_debug_assert(uv(1) < 1.0);
        Scalar w0 = wedge(q1 - uv, q2 - uv);
        Scalar w1 = wedge(q2 - uv, q0 - uv);
        Scalar w2 = wedge(q0 - uv, q1 - uv);
        const Scalar area_ = w0 + w1 + w2;
        la_debug_assert(std::fabs(area_ - area) < 1e-6);
        w0 /= area_;
        w1 /= area_;
        w2 /= area_;
        [[maybe_unused]] const auto ww = Eigen::Vector<Scalar, 3>(w0, w1, w2);
        la_debug_assert(std::fabs(ww.sum() - 1.0) < 1e-6);
        return {w0, w1, w2};
    };

    for (const auto jj : lagrange::range(coordinate_min(1), coordinate_max(1))) {
        la_debug_assert(jj < height - 1);
        for (const auto ii : lagrange::range(coordinate_min(0), coordinate_max(0))) {
            la_debug_assert(ii < width - 1);
            const auto ij_bottom_left = Eigen::Vector<size_t, 2>(ii, jj);
            const auto ij_bottom_right = Eigen::Vector<size_t, 2>(ii + 1, jj);
            const auto ij_top_left = Eigen::Vector<size_t, 2>(ii, jj + 1);
            const auto ij_top_right = Eigen::Vector<size_t, 2>(ii + 1, jj + 1);
            const auto ww_bottom_left = barycentric_weights(ij_bottom_left);
            const auto ww_bottom_right = barycentric_weights(ij_bottom_right);
            const auto ww_top_left = barycentric_weights(ij_top_left);
            const auto ww_top_right = barycentric_weights(ij_top_right);
            const auto ijs = std::array<Eigen::Vector<size_t, 2>, 4>{
                ij_bottom_left,
                ij_bottom_right,
                ij_top_right,
                ij_top_left,
            };
            const auto wws = std::array<Eigen::Vector<Scalar, 3>, 4>{
                ww_bottom_left,
                ww_bottom_right,
                ww_top_right,
                ww_top_left,
            };
            callback(ijs, wws);
        }
    }
}


template <typename Scalar, typename Index>
auto extract_mesh_with_alpha_mask(
    const SurfaceMesh<Scalar, Index>& mesh,
    const image::experimental::View3D<const float> image,
    const ExtractMeshWithAlphaMaskOptions& options) -> SurfaceMesh<Scalar, Index>
{
    AttributeId texcoord_id = options.texcoord_id;
    if (texcoord_id == invalid_attribute_id()) {
        texcoord_id =
            find_matching_attribute(mesh, AttributeUsage::UV).value_or(invalid_attribute_id());
    }

    la_runtime_assert(texcoord_id != invalid_attribute_id());
    la_runtime_assert(options.alpha_threshold >= 0.0);
    la_runtime_assert(options.alpha_threshold <= 1.0);

    la_runtime_assert(mesh.get_dimension() == 3);
    la_runtime_assert(mesh.is_triangle_mesh());
    la_runtime_assert(mesh.is_attribute_indexed(texcoord_id));

    const auto width = image.extent(0);
    const auto height = image.extent(1);
    la_runtime_assert(width > 0);
    la_runtime_assert(height > 0);
    la_runtime_assert(image.extent(2) == 4, "expected rgba image");

    logger().debug(
        "mesh {}v{}e{}f using {}",
        mesh.get_num_vertices(),
        mesh.get_num_edges(),
        mesh.get_num_facets(),
        mesh.get_attribute_name(texcoord_id));
    logger().debug("texture {}x{}x{}", image.extent(0), image.extent(1), image.extent(2));

    const auto& texcoord_attr = mesh.template get_indexed_attribute<Scalar>(texcoord_id);
    const auto texcoord_indices = reshaped_view(texcoord_attr.indices(), 3);
    const auto texcoord_values = matrix_view(texcoord_attr.values());
    const auto vertices = vertex_view(mesh);
    const auto facets = facet_view(mesh);
    const auto get_triangle_data = [&](Index ff) {
        Eigen::Matrix<Scalar, 3, 2> triangle_texcoords;
        Eigen::Matrix<Scalar, 3, 3> triangle_vertices;
        for (const Eigen::Index ii : lagrange::range(3)) {
            triangle_texcoords.row(ii) = texcoord_values.row(texcoord_indices(ff, ii));
            triangle_vertices.row(ii) = vertices.row(facets(ff, ii));
        }
        la_runtime_assert(triangle_texcoords.minCoeff() >= 0.0);
        la_runtime_assert(triangle_texcoords.maxCoeff() <= 1.0);
        return std::make_pair(triangle_texcoords, triangle_vertices);
    };

    tbb::concurrent_vector<Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor>> triangles;
    tbb::concurrent_vector<Eigen::Matrix<Scalar, 4, 3, Eigen::RowMajor>> quads;
    const auto loop = [&](const Index ff) {
        const auto triangle_data = get_triangle_data(ff);
        const auto& triangle_texcoords = std::get<0>(triangle_data);
        const auto& triangle_vertices = std::get<1>(triangle_data);
        rasterize_triangle_data(
            triangle_texcoords,
            width,
            height,
            [&](const std::array<Eigen::Vector<size_t, 2>, 4>& ijs,
                const std::array<Eigen::Vector<Scalar, 3>, 4>& wws) {
                la_debug_assert(ijs.size() == 4);
                la_debug_assert(wws.size() == 4);
                Eigen::Vector<Scalar, 3> mean_wws = Eigen::Vector<Scalar, 3>::Zero();
                for (const auto& ww : wws) mean_wws += ww;
                mean_wws.array() /= static_cast<Scalar>(wws.size());
                const bool center_outside_triangle = mean_wws.minCoeff() < 0.0;
                StackVector<Eigen::Vector<Scalar, 3>, 4> pps;
                for (const auto kk : lagrange::range(4)) {
                    const auto& ww = wws[kk];
                    if (center_outside_triangle && ww.minCoeff() < 0.0) continue;
                    const auto& ij = ijs[kk];
                    la_debug_assert(ij(0) < width);
                    la_debug_assert(ij(1) < height);
                    // Flip v because (0, 0) is assumed to be the bottom-left corner.
                    const bool is_opaque =
                        image(ij(0), height - 1 - ij(1), 3) > options.alpha_threshold;
                    if (!is_opaque) continue;
                    const Eigen::Vector<Scalar, 3> pp = triangle_vertices.transpose() * ww;
                    pps.emplace_back(pp);
                }
                switch (pps.size()) {
                case 0:
                case 1:
                case 2: break;
                case 3: {
                    Eigen::Matrix<Scalar, 3, 3> triangle;
                    triangle.row(0) = pps[0].transpose();
                    triangle.row(1) = pps[1].transpose();
                    triangle.row(2) = pps[2].transpose();
                    triangles.emplace_back(triangle);
                } break;
                case 4: {
                    Eigen::Matrix<Scalar, 4, 3> quad;
                    quad.row(0) = pps[0].transpose();
                    quad.row(1) = pps[1].transpose();
                    quad.row(2) = pps[2].transpose();
                    quad.row(3) = pps[3].transpose();
                    quads.emplace_back(quad);
                } break;
                default: la_runtime_assert(false); break;
                }
            });
    };
    tbb::parallel_for(static_cast<Index>(0), mesh.get_num_facets(), loop);
    logger().debug("num_triangles {}", triangles.size());
    logger().debug("num_quads {}", quads.size());

    SurfaceMesh<Scalar, Index> mesh_;
    for (const auto& triangle : triangles) {
        const auto ii = mesh_.get_num_vertices();
        mesh_.add_vertices(3, [&](const Index vv, span<Scalar> pp) {
            la_debug_assert(vv < 3);
            la_debug_assert(pp.size() == 3);
            const auto pp_ = triangle.row(vv);
            std::copy(pp_.data(), pp_.data() + pp.size(), pp.begin());
        });
        la_debug_assert(mesh_.get_num_vertices() == ii + 3);
        mesh_.add_triangle(ii + 2, ii + 1, ii + 0);
    }
    for (const auto& quad : quads) {
        const auto ii = mesh_.get_num_vertices();
        mesh_.add_vertices(4, [&](const Index vv, span<Scalar> pp) {
            la_debug_assert(vv < 4);
            la_debug_assert(pp.size() == 3);
            const auto pp_ = quad.row(vv);
            std::copy(pp_.data(), pp_.data() + pp.size(), pp.begin());
        });
        la_debug_assert(mesh_.get_num_vertices() == ii + 4);
        mesh_.add_quad(ii + 3, ii + 2, ii + 1, ii + 0);
    }

    return mesh_;
}

#define LA_X_extract_mesh_with_alpha_mask(_, Scalar, Index)   \
    template auto extract_mesh_with_alpha_mask(               \
        const SurfaceMesh<Scalar, Index>& mesh,               \
        const image::experimental::View3D<const float> image, \
        const ExtractMeshWithAlphaMaskOptions& options) -> SurfaceMesh<Scalar, Index>;

LA_SURFACE_MESH_X(extract_mesh_with_alpha_mask, 0)

} // namespace lagrange::texproc
