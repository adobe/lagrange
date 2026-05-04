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

#include "clip_triangle_by_bbox.h"

#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/utils/StackVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>

#include <Eigen/Geometry>

#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <cmath>

namespace lagrange::texproc {

namespace {

template <typename Scalar>
auto uv_to_coordinate(const Eigen::Vector<Scalar, 2>& uv, const size_t width, const size_t height)
    -> Eigen::Vector<size_t, 2>
{
    auto ii = static_cast<size_t>(uv(0) * static_cast<Scalar>(width));
    auto jj = static_cast<size_t>(uv(1) * static_cast<Scalar>(height));
    ii = std::min(ii, width - 1);
    jj = std::min(jj, height - 1);
    la_debug_assert(ii < width);
    la_debug_assert(jj < height);
    return {ii, jj};
}

template <typename Scalar>
auto uv_from_coordinate(const Eigen::Vector<size_t, 2>& ij, const size_t width, const size_t height)
    -> Eigen::Vector<Scalar, 2>
{
    la_debug_assert(ij(0) < width);
    la_debug_assert(ij(1) < height);
    const auto uu = (static_cast<Scalar>(ij(0)) + 0.5) / static_cast<Scalar>(width);
    const auto vv = (static_cast<Scalar>(ij(1)) + 0.5) / static_cast<Scalar>(height);
    return {uu, vv};
}

template <typename Scalar>
auto wedge(const Eigen::Vector<Scalar, 2>& xx, const Eigen::Vector<Scalar, 2>& yy) -> Scalar
{
    return xx(0) * yy(1) - xx(1) * yy(0);
}

template <typename Scalar>
auto barycentric_weights(
    const Eigen::Matrix<Scalar, 3, 2, Eigen::RowMajor>& texcoords,
    const Eigen::Vector<Scalar, 2>& uv) -> Eigen::Vector<Scalar, 3>
{
    const Eigen::Vector<Scalar, 2> q0 = texcoords.row(0).transpose();
    const Eigen::Vector<Scalar, 2> q1 = texcoords.row(1).transpose();
    const Eigen::Vector<Scalar, 2> q2 = texcoords.row(2).transpose();
    [[maybe_unused]] const Scalar area = wedge<Scalar>(q1 - q0, q2 - q0);
    la_debug_assert(std::fabs(area) > 1e-7);
    la_debug_assert(uv(0) >= 0.0);
    la_debug_assert(uv(1) >= 0.0);
    la_debug_assert(uv(0) <= 1.0);
    la_debug_assert(uv(1) <= 1.0);
    Scalar w0 = wedge<Scalar>(q1 - uv, q2 - uv);
    Scalar w1 = wedge<Scalar>(q2 - uv, q0 - uv);
    Scalar w2 = wedge<Scalar>(q0 - uv, q1 - uv);
    const Scalar area_sum = w0 + w1 + w2;
    la_debug_assert(std::fabs(area_sum - area) < 1e-5);
    w0 /= area_sum;
    w1 /= area_sum;
    w2 /= area_sum;
    const auto ww = Eigen::Vector<Scalar, 3>(w0, w1, w2);
    la_debug_assert(std::fabs(ww.sum() - 1.0) < 1e-5);
    return ww;
}

template <typename Scalar, typename Callback>
void rasterize_triangle_data(
    const Eigen::Matrix<Scalar, 3, 2, Eigen::RowMajor>& texcoords,
    const size_t width,
    const size_t height,
    Callback callback)
{
    Eigen::AlignedBox<Scalar, 2> bbox_triangle;
    for (auto uv : texcoords.rowwise()) {
        bbox_triangle.extend(uv.transpose());
    }
    auto coordinate_min = uv_to_coordinate(bbox_triangle.min(), width, height);
    auto coordinate_max = uv_to_coordinate(bbox_triangle.max(), width, height);

    // Extend bbox to cover uv triangle.
    if (coordinate_min(0) > 0) coordinate_min(0) -= 1;
    if (coordinate_min(1) > 0) coordinate_min(1) -= 1;
    if (coordinate_max(0) < width - 1) coordinate_max(0) += 1;
    if (coordinate_max(1) < height - 1) coordinate_max(1) += 1;

    {
        const Eigen::Vector<Scalar, 2> q0 = texcoords.row(0).transpose();
        const Eigen::Vector<Scalar, 2> q1 = texcoords.row(1).transpose();
        const Eigen::Vector<Scalar, 2> q2 = texcoords.row(2).transpose();
        const Scalar area = wedge<Scalar>(q1 - q0, q2 - q0);
        if (std::fabs(area) <= 1e-7) return;
    }

    for (const auto jj : lagrange::range(coordinate_min(1), coordinate_max(1))) {
        la_debug_assert(jj < height - 1);
        for (const auto ii : lagrange::range(coordinate_min(0), coordinate_max(0))) {
            la_debug_assert(ii < width - 1);
            const auto ij_bottom_left = Eigen::Vector<size_t, 2>(ii, jj);
            const auto ij_bottom_right = Eigen::Vector<size_t, 2>(ii + 1, jj);
            const auto ij_top_left = Eigen::Vector<size_t, 2>(ii, jj + 1);
            const auto ij_top_right = Eigen::Vector<size_t, 2>(ii + 1, jj + 1);
            // Negative orientation because v is flipped.
            const auto ijs = std::array<Eigen::Vector<size_t, 2>, 4>{
                ij_bottom_left,
                ij_top_left,
                ij_top_right,
                ij_bottom_right,
            };
            callback(ijs);
        }
    }
}

template <typename Scalar>
Scalar sample_alpha(
    const Eigen::Vector<Scalar, 2>& uv,
    const image::experimental::View3D<const float>& image)
{
    const auto w = static_cast<Scalar>(image.extent(0));
    const auto h = static_cast<Scalar>(image.extent(1));

    // Convert UV to continuous pixel coordinates. Texel centers are at integer indices,
    // so uv_from_coordinate maps texel (i,j) to UV ((i+0.5)/w, (j+0.5)/h).
    // The inverse is: px = u*w - 0.5, py = v*h - 0.5.
    const Scalar px = uv(0) * w - Scalar(0.5);
    const Scalar py = uv(1) * h - Scalar(0.5);

    const int ix = static_cast<int>(std::floor(px));
    const int iy = static_cast<int>(std::floor(py));
    const int iw = static_cast<int>(image.extent(0));
    const int ih = static_cast<int>(image.extent(1));

    const auto x0 = static_cast<size_t>(std::clamp(ix, 0, iw - 1));
    const auto x1 = static_cast<size_t>(std::clamp(ix + 1, 0, iw - 1));
    const auto y0 = static_cast<size_t>(std::clamp(iy, 0, ih - 1));
    const auto y1 = static_cast<size_t>(std::clamp(iy + 1, 0, ih - 1));

    const Scalar fx = std::clamp(px - static_cast<Scalar>(ix), Scalar(0), Scalar(1));
    const Scalar fy = std::clamp(py - static_cast<Scalar>(iy), Scalar(0), Scalar(1));

    // Read 4 alpha values. Flip v because (0,0) is the bottom-left corner in UV space.
    const auto yflip = [&](size_t y) { return image.extent(1) - 1 - y; };
    const Scalar a00 = static_cast<Scalar>(image(x0, yflip(y0), 3));
    const Scalar a10 = static_cast<Scalar>(image(x1, yflip(y0), 3));
    const Scalar a01 = static_cast<Scalar>(image(x0, yflip(y1), 3));
    const Scalar a11 = static_cast<Scalar>(image(x1, yflip(y1), 3));

    // Bilinear interpolation.
    return (Scalar(1) - fy) * ((Scalar(1) - fx) * a00 + fx * a10) +
           fy * ((Scalar(1) - fx) * a01 + fx * a11);
}

template <typename Scalar>
Eigen::Vector<Scalar, 3> lift_uv_to_position(
    const Eigen::Vector<Scalar, 2>& uv,
    const Eigen::Matrix<Scalar, 3, 2, Eigen::RowMajor>& triangle_texcoords,
    const Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor>& triangle_vertices)
{
    const auto ww = barycentric_weights<Scalar>(triangle_texcoords, uv);
    return triangle_vertices.transpose() * ww;
}

template <typename Scalar, typename EmitPolygon>
bool predicate_impl(
    EmitPolygon emit_polygon,
    const image::experimental::View3D<const float>& image,
    const Scalar& alpha_threshold,
    const Eigen::Matrix<Scalar, 3, 2, Eigen::RowMajor>& triangle_texcoords,
    const Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor>& triangle_vertices,
    const std::array<Eigen::Vector<size_t, 2>, 4>& ijs)
{
    la_debug_assert(ijs.size() == 4);

    // Clip uv triangle with current 2-cell.
    // Ijs are axis aligned by construction, so are uvs.
    Eigen::AlignedBox<Scalar, 2> bbox_cell;
    for (const auto& ij : ijs) {
        la_debug_assert(ij(0) < image.extent(0));
        la_debug_assert(ij(1) < image.extent(1));
        const auto uv = uv_from_coordinate<Scalar>(ij, image.extent(0), image.extent(1));
        la_debug_assert(ij == uv_to_coordinate(uv, image.extent(0), image.extent(1)));
        bbox_cell.extend(uv);
    }
    const internal::SmallPolygon2<Scalar, 3> triangle_poly = triangle_texcoords;
    const auto uvs_cell = internal::clip_triangle_by_bbox(triangle_poly, bbox_cell);
    la_debug_assert(uvs_cell.rows() <= 7);

    if (uvs_cell.rows() == 0) return true;

    // Sample alpha at each clipped polygon vertex.
    const auto n = static_cast<size_t>(uvs_cell.rows());
    StackVector<Scalar, 7> alphas;
    StackVector<Eigen::Vector<Scalar, 2>, 7> uvs;
    for (Eigen::Index i = 0; i < uvs_cell.rows(); ++i) {
        const Eigen::Vector<Scalar, 2> uv = uvs_cell.row(i).transpose();
        uvs.emplace_back(uv);
        alphas.emplace_back(sample_alpha<Scalar>(uv, image));
    }

    // Count opaque-transparent crossings to detect the ambiguous saddle case.
    size_t num_crossings = 0;
    bool all_opaque = true;
    for (size_t i = 0; i < n; ++i) {
        if (!(alphas[i] > alpha_threshold)) all_opaque = false;
        const size_t j = (i + 1) % n;
        if ((alphas[i] > alpha_threshold) != (alphas[j] > alpha_threshold)) {
            num_crossings++;
        }
    }

    // Helper: interpolate position at the alpha threshold crossing between vertices i and j.
    auto make_crossing = [&](size_t i, size_t j) {
        const Scalar ai = alphas[i];
        const Scalar aj = alphas[j];
        const Scalar t = std::clamp((alpha_threshold - ai) / (aj - ai), Scalar(0), Scalar(1));
        const Eigen::Vector<Scalar, 2> uv_crossing = (Scalar(1) - t) * uvs[i] + t * uvs[j];
        return lift_uv_to_position(uv_crossing, triangle_texcoords, triangle_vertices);
    };

    // Determine whether ambiguous saddle runs should be merged or kept separate.
    // Sample alpha at the cell center using bilinear interpolation.
    bool split_runs = false;
    if (num_crossings >= 4) {
        const Eigen::Vector<Scalar, 2> center_uv = bbox_cell.center();
        const Scalar center_alpha = sample_alpha<Scalar>(center_uv, image);
        split_runs = !(center_alpha > alpha_threshold);
    }

    if (!split_runs) {
        // Simple case (0 or 2 crossings), or connected saddle: emit a single polygon.
        StackVector<Eigen::Vector<Scalar, 3>, 14> pps;
        for (size_t i = 0; i < n; ++i) {
            const size_t j = (i + 1) % n;
            if (alphas[i] > alpha_threshold) {
                pps.emplace_back(
                    lift_uv_to_position(uvs[i], triangle_texcoords, triangle_vertices));
            }
            if ((alphas[i] > alpha_threshold) != (alphas[j] > alpha_threshold)) {
                pps.emplace_back(make_crossing(i, j));
            }
        }
        if (!pps.empty()) emit_polygon(pps);
    } else {
        // Disconnected saddle: emit separate polygons for each opaque run.
        // Start walking from a transparent vertex so runs are cleanly delimited.
        size_t start = 0;
        for (size_t i = 0; i < n; ++i) {
            if (!(alphas[i] > alpha_threshold)) {
                start = i;
                break;
            }
        }

        StackVector<Eigen::Vector<Scalar, 3>, 14> pps;
        for (size_t k = 0; k < n; ++k) {
            const size_t i = (start + k) % n;
            const size_t j = (i + 1) % n;
            const bool i_opaque = alphas[i] > alpha_threshold;
            const bool j_opaque = alphas[j] > alpha_threshold;

            if (i_opaque) {
                pps.emplace_back(
                    lift_uv_to_position(uvs[i], triangle_texcoords, triangle_vertices));
            }

            if (i_opaque != j_opaque) {
                pps.emplace_back(make_crossing(i, j));
                if (i_opaque && !j_opaque) {
                    // Leaving opaque region: finish this run.
                    emit_polygon(pps);
                    pps.clear();
                }
            }
        }
        if (!pps.empty()) {
            emit_polygon(pps);
        }
    }

    return all_opaque;
}

} // namespace

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

    la_runtime_assert(image.extent(0) > 0);
    la_runtime_assert(image.extent(1) > 0);
    la_runtime_assert(image.extent(2) == 4, "expected rgba image");

    logger().debug(
        "mesh {}v{}e{}f using {}",
        mesh.get_num_vertices(),
        mesh.get_num_edges(),
        mesh.get_num_facets(),
        mesh.get_attribute_name(texcoord_id));
    logger().debug("texture {}x{}x{}", image.extent(0), image.extent(1), image.extent(2));
    logger().debug("alpha_threshold {}", options.alpha_threshold);

    const auto& texcoord_attr = mesh.template get_indexed_attribute<Scalar>(texcoord_id);
    const auto texcoord_indices = reshaped_view(texcoord_attr.indices(), 3);
    const auto texcoord_values = matrix_view(texcoord_attr.values());
    const auto vertices = vertex_view(mesh);
    const auto facets = facet_view(mesh);

    const auto get_triangle_data = [&](Index ff) {
        Eigen::Matrix<Scalar, 3, 2, Eigen::RowMajor> triangle_texcoords;
        Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor> triangle_vertices;
        for (const Eigen::Index ii : lagrange::range(3)) {
            triangle_texcoords.row(ii) = texcoord_values.row(texcoord_indices(ff, ii));
            triangle_vertices.row(ii) = vertices.row(facets(ff, ii));
        }
        la_runtime_assert(triangle_texcoords.minCoeff() >= 0.0);
        la_runtime_assert(triangle_texcoords.maxCoeff() <= 1.0);
        return std::make_pair(triangle_texcoords, triangle_vertices);
    };

    using TriMatrix = Eigen::Matrix<Scalar, 3, 3, Eigen::RowMajor>;
    using QuadMatrix = Eigen::Matrix<Scalar, 4, 3, Eigen::RowMajor>;

    auto make_tri = [](const auto& p0, const auto& p1, const auto& p2) {
        TriMatrix tri;
        tri.row(0) = p0.transpose();
        tri.row(1) = p1.transpose();
        tri.row(2) = p2.transpose();
        return tri;
    };

    auto make_quad = [](const auto& p0, const auto& p1, const auto& p2, const auto& p3) {
        QuadMatrix quad;
        quad.row(0) = p0.transpose();
        quad.row(1) = p1.transpose();
        quad.row(2) = p2.transpose();
        quad.row(3) = p3.transpose();
        return quad;
    };

    struct Payload
    {
        std::vector<TriMatrix> triangles;
        std::vector<QuadMatrix> quads;
        std::vector<TriMatrix> committed_triangles;
        std::vector<QuadMatrix> committed_quads;
    };
    tbb::enumerable_thread_specific<Payload> payloads;
    const auto loop = [&](const Index ff) {
        const auto triangle_data = get_triangle_data(ff);
        const auto& triangle_texcoords = std::get<0>(triangle_data);
        const auto& triangle_vertices = std::get<1>(triangle_data);
        auto& payload = payloads.local();
        la_debug_assert(payload.triangles.empty());
        la_debug_assert(payload.quads.empty());
        bool all_opaque = true;
        rasterize_triangle_data(
            triangle_texcoords,
            image.extent(0),
            image.extent(1),
            [&](const std::array<Eigen::Vector<size_t, 2>, 4>& ijs) {
                auto emit_polygon = [&](const auto& pps) {
                    // Fan-triangulate the polygon (vertices are in convex order).
                    if (pps.size() == 3) {
                        payload.triangles.emplace_back(make_tri(pps[0], pps[1], pps[2]));
                    } else if (pps.size() == 4) {
                        payload.quads.emplace_back(make_quad(pps[0], pps[1], pps[2], pps[3]));
                    } else if (pps.size() >= 5) {
                        for (size_t k = 1; k + 1 < pps.size(); ++k) {
                            payload.triangles.emplace_back(make_tri(pps[0], pps[k], pps[k + 1]));
                        }
                    }
                };
                all_opaque &= predicate_impl(
                    emit_polygon,
                    image,
                    static_cast<Scalar>(options.alpha_threshold),
                    triangle_texcoords,
                    triangle_vertices,
                    ijs);
            });
        if (all_opaque) {
            // Bypass tessellation if all texels are opaque.
            payload.committed_triangles.emplace_back(triangle_vertices);
        } else {
            payload.committed_triangles.insert(
                payload.committed_triangles.end(),
                payload.triangles.begin(),
                payload.triangles.end());
            payload.committed_quads.insert(
                payload.committed_quads.end(),
                payload.quads.begin(),
                payload.quads.end());
        }
        payload.triangles.clear();
        payload.quads.clear();
    };
    tbb::parallel_for(static_cast<Index>(0), mesh.get_num_facets(), loop);

    if (logger().should_log(spdlog::level::debug)) {
        size_t num_triangles = 0;
        size_t num_quads = 0;
        for (const auto& payload : payloads) {
            num_triangles += payload.committed_triangles.size();
            num_quads += payload.committed_quads.size();
        }
        logger().debug("num_committed_triangles {}", num_triangles);
        logger().debug("num_committed_quads {}", num_quads);
    }

    SurfaceMesh<Scalar, Index> result;
    for (const auto& payload : payloads) {
        for (const auto& triangle : payload.committed_triangles) {
            const auto ii = result.get_num_vertices();
            result.add_vertices(3, [&](const Index vv, span<Scalar> pp) {
                la_debug_assert(vv < 3);
                la_debug_assert(pp.size() == 3);
                const auto row = triangle.row(vv);
                std::copy(row.data(), row.data() + pp.size(), pp.begin());
            });
            la_debug_assert(result.get_num_vertices() == ii + 3);
            result.add_triangle(ii + 0, ii + 1, ii + 2);
        }
        for (const auto& quad : payload.committed_quads) {
            const auto ii = result.get_num_vertices();
            result.add_vertices(4, [&](const Index vv, span<Scalar> pp) {
                la_debug_assert(vv < 4);
                la_debug_assert(pp.size() == 3);
                const auto row = quad.row(vv);
                std::copy(row.data(), row.data() + pp.size(), pp.begin());
            });
            la_debug_assert(result.get_num_vertices() == ii + 4);
            result.add_quad(ii + 0, ii + 1, ii + 2, ii + 3);
        }
    }

    return result;
}

#define LA_X_extract_mesh_with_alpha_mask(_, Scalar, Index)   \
    template auto extract_mesh_with_alpha_mask(               \
        const SurfaceMesh<Scalar, Index>& mesh,               \
        const image::experimental::View3D<const float> image, \
        const ExtractMeshWithAlphaMaskOptions& options) -> SurfaceMesh<Scalar, Index>;

LA_SURFACE_MESH_X(extract_mesh_with_alpha_mask, 0)

} // namespace lagrange::texproc
