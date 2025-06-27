/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include "mapbox/earcut.h"

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/delaunay_split.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/strings.h>
#include <lagrange/utils/tracy.h>
#include <lagrange/views.h>

#include <Eigen/Dense>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

template <typename Scalar, typename Index>
void append_triangles_from_quad(
    const SurfaceMesh<Scalar, Index>& mesh,
    Index f,
    std::vector<Index>& new_to_old_corners,
    std::vector<Index>& new_to_old_facets)
{
    LAGRANGE_ZONE_SCOPED;

    // Collect coordinates in a padded matrix
    Eigen::Matrix<Scalar, 4, 3, Eigen::RowMajor> points;
    points.setZero();
    {
        LAGRANGE_ZONE_SCOPED;
        const auto& vertex_positions = vertex_view(mesh);
        const auto facet_vertices = mesh.get_facet_vertices(f);
        const auto dim = mesh.get_dimension();
        for (Index lv = 0; lv < 4; ++lv) {
            points.row(lv).head(dim) = vertex_positions.row(facet_vertices[lv]);
        }
    }

    auto lv = internal::delaunay_split(points.row(0), points.row(1), points.row(2), points.row(3));

    {
        LAGRANGE_ZONE_SCOPED;
        const auto corner_begin = mesh.get_facet_corner_begin(f);
        for (Index lf : {0, 1}) {
            for (Index k = 0; k < 3; ++k) {
                new_to_old_corners.push_back(corner_begin + static_cast<Index>(lv[lf][k]));
            }
            new_to_old_facets.push_back(f);
        }
    }
}

template <typename Scalar, typename Index>
Eigen::Vector3<Scalar> facet_normal(const SurfaceMesh<Scalar, Index>& mesh, Index f)
{
    LAGRANGE_ZONE_SCOPED;

    if (mesh.get_dimension() == 2) {
        return {0, 0, 1};
    } else {
        // Robust polygon normal calculation
        // n ~ p0 x p1 + p1 x p2 + ... + pn x p0
        la_debug_assert(mesh.get_dimension() == 3);
        Eigen::Vector3<Scalar> nrm = Eigen::Vector3<Scalar>::Zero();
        const auto& vertex_positions = vertex_view(mesh);
        const auto facet_vertices = mesh.get_facet_vertices(f);
        for (size_t i = 0, n = facet_vertices.size(); i < n; ++i) {
            size_t j = (i + 1) % n;
            nrm += vertex_positions.row(facet_vertices[i])
                       .template head<3>()
                       .cross(vertex_positions.row(facet_vertices[j]).template head<3>());
        }
        return nrm;
    }
}

template <typename Scalar, typename Index>
auto find_best_2d_axes(const SurfaceMesh<Scalar, Index>& mesh, Index f)
{
    LAGRANGE_ZONE_SCOPED;

    auto nrm = facet_normal(mesh, f);

    const Index max_axis = [&] {
        Eigen::Index max_axis_;
        nrm.cwiseAbs().maxCoeff(&max_axis_);
        return static_cast<Index>(max_axis_);
    }();

    std::array<Index, 2> axes;
    axes[0] = (max_axis + 1) % 3;
    axes[1] = (max_axis + 2) % 3;
    if (nrm[max_axis] < 0) {
        // Swap projection axes to preserve polygon orientation
        std::swap(axes[0], axes[1]);
    }

    return axes;
}

template <typename Scalar, typename Index>
void append_triangles_from_polygon(
    SurfaceMesh<Scalar, Index>& mesh,
    Index f,
    std::vector<std::array<Scalar, 2>>& polygon,
    mapbox::detail::Earcut<Index>& earcut,
    std::vector<Index>& new_to_old_corners,
    std::vector<Index>& new_to_old_facets)
{
    LAGRANGE_ZONE_SCOPED;

    // Compute projection axes
    auto axes = find_best_2d_axes(mesh, f);

    // Fill polygon data
    polygon.clear();
    auto vertex_positions = vertex_view(mesh);
    auto facet_vertices = mesh.get_facet_vertices(f);
    for (Index v : facet_vertices) {
        const Scalar x = vertex_positions.row(v)[axes[0]];
        const Scalar y = vertex_positions.row(v)[axes[1]];
        polygon.push_back({x, y});
    }

    // First contour define the main polygon. Following contours defined holes (not used here)
    span<std::vector<std::array<Scalar, 2>>> polygons(&polygon, 1);
    earcut(polygons); // => result = 3 * faces, clockwise

    la_debug_assert(earcut.indices.size() % 3 == 0);

    // Append new triangles
    {
        LAGRANGE_ZONE_SCOPED;
        const Index num_triangles = static_cast<Index>(earcut.indices.size() / 3);
        const auto corner_begin = mesh.get_facet_corner_begin(f);
        for (Index lf = 0; lf < num_triangles; ++lf) {
            for (Index k = 0; k < 3; ++k) {
                new_to_old_corners.push_back(corner_begin + earcut.indices[3 * lf + k]);
            }
            new_to_old_facets.push_back(f);
        }
    }
}

template <typename Scalar, typename Index>
void triangulate_polygonal_facets_earcut(SurfaceMesh<Scalar, Index>& mesh)
{
    LAGRANGE_ZONE_SCOPED;

    if (mesh.is_triangle_mesh()) {
        return;
    }

    const Index dim = mesh.get_dimension();
    la_runtime_assert(dim == 2 || dim == 3, "Mesh dimension must be 2 or 3");

    const Index old_num_facets = mesh.get_num_facets();
    std::vector<bool> to_remove(old_num_facets, false);

    // Local data
    std::vector<std::array<Scalar, 2>> polygon;
    mapbox::detail::Earcut<Index> earcut;
    std::vector<Index> new_to_old_corners;
    std::vector<Index> new_to_old_facets;
    bool need_removal = false;

    for (Index f = 0; f < old_num_facets; ++f) {
        const auto facet_size = mesh.get_facet_size(f);
        if (facet_size != 3) {
            need_removal = true;
        }
        if (facet_size <= 2) {
            to_remove[f] = true;
        } else if (facet_size == 4) {
            // Triangulate quad
            to_remove[f] = true;
            append_triangles_from_quad(mesh, f, new_to_old_corners, new_to_old_facets);
        } else if (facet_size > 4) {
            // Triangulate polygon via ear cutting
            to_remove[f] = true;
            append_triangles_from_polygon(
                mesh,
                f,
                polygon,
                earcut,
                new_to_old_corners,
                new_to_old_facets);
        }
    }

    {
        // It is more efficient to add all triangles all at once. This is especially true if the
        // mesh has edge information/attributes, since adding new triangles involves sorting &
        // indexing new edges.
        LAGRANGE_ZONE_SCOPED;
        std::vector<Index> new_vertices;
        {
            new_vertices.reserve(new_to_old_corners.size());
            auto c2v = mesh.get_corner_to_vertex().get_all();
            for (auto c : new_to_old_corners) {
                new_vertices.push_back(c2v[c]);
            }
        }
        if (!new_to_old_facets.empty()) {
            // TODO: Remove this if once we fix the add_xxx operations to not trigger any copy
            mesh.add_triangles(static_cast<Index>(new_to_old_facets.size()), new_vertices);
        }
    }

    auto remap_facet_attributes = [&](auto&& attr) {
        la_debug_assert(attr.get_element_type() == AttributeElement::Facet);
        for (Index f = old_num_facets; f < mesh.get_num_facets(); ++f) {
            auto target_row = attr.ref_row(f);
            auto source_row = attr.get_row(new_to_old_facets[f - old_num_facets]);
            std::copy(source_row.begin(), source_row.end(), target_row.begin());
        }
    };

    auto remap_corner_attributes = [&](auto&& attr) {
        la_debug_assert(attr.get_element_type() == AttributeElement::Corner);
        la_debug_assert(old_num_facets < mesh.get_num_facets());
        const Index corner_begin = mesh.get_facet_corner_begin(old_num_facets);
        for (Index c = corner_begin; c < mesh.get_num_corners(); ++c) {
            auto target_row = attr.ref_row(c);
            auto source_row = attr.get_row(new_to_old_corners[c - corner_begin]);
            std::copy(source_row.begin(), source_row.end(), target_row.begin());
        }
    };

    // Remap facet, corner, and indexed attributes
    if (mesh.get_num_facets() != old_num_facets) {
        LAGRANGE_ZONE_SCOPED;
        par_foreach_named_attribute_write<
            AttributeElement::Indexed | AttributeElement::Facet | AttributeElement::Corner>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                using AttributeType = std::decay_t<decltype(attr)>;
                // Do not remap reserved attributes...
                if (!mesh.attr_name_is_reserved(name)) {
                    if constexpr (AttributeType::IsIndexed) {
                        remap_corner_attributes(attr.indices());
                    } else {
                        if (attr.get_element_type() == AttributeElement::Facet) {
                            remap_facet_attributes(attr);
                        } else {
                            remap_corner_attributes(attr);
                        }
                    }
                }
            });
    }

    // Finally, cleanup facets marked for removal
    if (need_removal) {
        LAGRANGE_ZONE_SCOPED;
        mesh.remove_facets([&](Index f) -> bool {
            if (f < old_num_facets) {
                return to_remove[f];
            } else {
                return false;
            }
        });
    }

    // We do not automatically remap edge attributes, as we don't have a way to interpret how to
    // remap those attributes
    {
        LAGRANGE_ZONE_SCOPED;
        bool has_edge_attr = false;
        seq_foreach_attribute_read<AttributeElement::Edge>(mesh, [&](auto&&) {
            has_edge_attr = true;
        });
        if (has_edge_attr) {
            logger().warn(
                "Mesh has edge attributes. Those will not be remapped to the new triangular "
                "facets. Please remap them manually.");
        }
    }

    mesh.compress_if_regular();
}

template <typename Scalar, typename Index>
void triangulate_polygonal_facets_centroid_fan(SurfaceMesh<Scalar, Index>& mesh)
{
    if (mesh.is_triangle_mesh()) {
        return;
    }

    const Index dim = mesh.get_dimension();
    la_runtime_assert(dim == 2 || dim == 3, "Mesh dimension must be 2 or 3");

    const Index old_num_vertices = mesh.get_num_vertices();
    const Index old_num_facets = mesh.get_num_facets();
    std::vector<Scalar> centroids;
    std::vector<Index> facets_to_remove;
    std::vector<Index> new_triangles;

    auto vertices = vertex_view(mesh);
    Eigen::Matrix<Scalar, 1, 3> c;
    Index new_vertex_count = 0;
    Index new_triangle_count = 0;

    for (Index fid = 0; fid < old_num_facets; ++fid) {
        const auto facet_size = mesh.get_facet_size(fid);
        if (facet_size != 3) {
            auto f = mesh.get_facet_vertices(fid);
            facets_to_remove.push_back(fid);

            c.setZero();
            for (Index i = 0; i < facet_size; i++) {
                c.head(dim) += vertices.row(f[i]).head(dim);
            }
            c /= static_cast<Scalar>(facet_size);

            if (dim == 3) {
                centroids.push_back(c[0]);
                centroids.push_back(c[1]);
                centroids.push_back(c[2]);
            } else {
                centroids.push_back(c[0]);
                centroids.push_back(c[1]);
            }

            Index cid = old_num_vertices + new_vertex_count;
            new_vertex_count++;
            for (Index i = 0; i < facet_size; i++) {
                Index v0 = f[i];
                Index v1 = f[(i + 1) % facet_size];
                new_triangles.push_back(cid);
                new_triangles.push_back(v0);
                new_triangles.push_back(v1);
            }
            new_triangle_count += facet_size;
        }
    }

    mesh.add_vertices(new_vertex_count, {centroids.data(), centroids.size()});
    mesh.add_triangles(new_triangle_count, {new_triangles.data(), new_triangles.size()});

    auto remap_vertex_attribute = [&](auto&& attr) {
        auto data = matrix_ref(attr);

        for (Index i = 0; i < new_vertex_count; ++i) {
            Index ci = old_num_vertices + i;
            Index fi = facets_to_remove[i];
            auto f = mesh.get_facet_vertices(fi);
            la_debug_assert(!f.empty());

            size_t facet_size = f.size();
            data.row(ci).setZero();
            for (size_t j = 0; j < facet_size; ++j) {
                data.row(ci) += data.row(f[j]);
            }
            data.row(ci) /= static_cast<Scalar>(facet_size);
        }
    };

    auto remap_facet_attribute = [&](auto&& attr) {
        auto data = matrix_ref(attr);

        Index facet_count = old_num_facets;
        for (Index i = 0; i < new_vertex_count; ++i) {
            // Each new vertex corresponds to a facet centroid, fi is that facet.
            Index fi = facets_to_remove[i];
            Index facet_size = mesh.get_facet_size(fi);

            for (Index j = 0; j < facet_size; ++j) {
                Index fj = facet_count + j;
                data.row(fj) = data.row(fi);
            }
            facet_count += facet_size;
        }
    };

    auto remap_corner_attribute = [&](auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        auto data = matrix_ref(attr);
        std::vector<ValueType> buffer(data.cols());
        Eigen::Map<Eigen::RowVectorX<ValueType>> centroid_data(buffer.data(), buffer.size());

        Index facet_count = old_num_facets;
        for (Index i = 0; i < new_vertex_count; ++i) {
            Index fi = facets_to_remove[i];
            auto f = mesh.get_facet_vertices(fi);
            la_debug_assert(!f.empty());
            size_t facet_size = f.size();

            // Compute centroid data
            centroid_data.setZero();
            const Index c_begin = mesh.get_facet_corner_begin(fi);
            const Index c_end = mesh.get_facet_corner_end(fi);
            for (Index ci = c_begin; ci < c_end; ++ci) {
                centroid_data += data.row(ci);
            }
            centroid_data /= static_cast<ValueType>(facet_size);

            // Update data for newly added corners
            for (size_t j = 0; j < facet_size; ++j) {
                Index old_c1 = c_begin + static_cast<Index>(j);
                Index old_c2 = c_begin + static_cast<Index>(j + 1) % facet_size;

                Index fj = facet_count + static_cast<Index>(j);
                Index c0 = mesh.get_facet_corner_begin(fj);
                Index c1 = c0 + 1;
                Index c2 = c0 + 2;

                data.row(c0) = centroid_data;
                data.row(c1) = data.row(old_c1);
                data.row(c2) = data.row(old_c2);
            }
            facet_count += static_cast<Index>(facet_size);
        }
    };

    auto remap_indexed_attribute = [&](auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        auto& value_attr = attr.values();
        auto& index_attr = attr.indices();

        size_t old_num_elements = value_attr.get_num_elements();
        value_attr.resize_elements(old_num_elements + static_cast<size_t>(new_vertex_count));
        std::vector<ValueType> buffer(value_attr.get_num_channels());
        Eigen::Map<Eigen::RowVectorX<ValueType>> centroid_data(buffer.data(), buffer.size());

        auto values = matrix_ref(value_attr);
        auto indices = vector_ref(index_attr);

        Index facet_count = old_num_facets;
        for (Index i = 0; i < new_vertex_count; ++i) {
            Index fi = facets_to_remove[i];
            Index facet_size = mesh.get_facet_size(fi);
            Index ci_begin = mesh.get_facet_corner_begin(fi);

            centroid_data.setZero();
            for (Index j = 0; j < facet_size; j++) {
                Index ci = ci_begin + j;
                centroid_data += values.row(indices(ci));
            }
            centroid_data /= static_cast<ValueType>(facet_size);

            values.row(old_num_elements + i) = centroid_data;

            for (Index j = 0; j < facet_size; j++) {
                Index fj = facet_count + j;
                Index cj_begin = mesh.get_facet_corner_begin(fj);
                indices(cj_begin) = static_cast<Index>(old_num_elements) + i;
                indices(cj_begin + 1) = indices(ci_begin + j);
                indices(cj_begin + 2) = indices(ci_begin + (j + 1) % facet_size);
            }
            facet_count += facet_size;
        }
    };

    // Map attributes
    par_foreach_named_attribute_write<
        AttributeElement::Vertex | AttributeElement::Facet | AttributeElement::Corner |
        AttributeElement::Indexed>(mesh, [&](std::string_view name, auto&& attr) {
        if (mesh.attr_name_is_reserved(name)) return;

        using AttributeType = std::decay_t<decltype(attr)>;
        if constexpr (AttributeType::IsIndexed) {
            remap_indexed_attribute(attr);
        } else {
            switch (attr.get_element_type()) {
            case AttributeElement::Vertex: remap_vertex_attribute(attr); break;
            case AttributeElement::Facet: remap_facet_attribute(attr); break;
            case AttributeElement::Corner: remap_corner_attribute(attr); break;
            default: break;
            }
        }
    });

    // We do not automatically remap edge attributes, as we don't have a way to interpret how to
    // remap those attributes
    {
        LAGRANGE_ZONE_SCOPED;
        bool has_edge_attr = false;
        seq_foreach_attribute_read<AttributeElement::Edge>(mesh, [&](auto&&) {
            has_edge_attr = true;
        });
        if (has_edge_attr) {
            logger().warn(
                "Mesh has edge attributes. Those will not be remapped to the new triangular "
                "facets. Please remap them manually.");
        }
    }

    mesh.remove_facets({facets_to_remove.data(), facets_to_remove.size()});
    mesh.compress_if_regular();
}

template <typename Scalar, typename Index>
void triangulate_polygonal_facets(
    SurfaceMesh<Scalar, Index>& mesh,
    const TriangulationOptions& options)
{
    switch (options.scheme) {
    case TriangulationOptions::Scheme::Earcut: triangulate_polygonal_facets_earcut(mesh); break;
    case TriangulationOptions::Scheme::CentroidFan:
        triangulate_polygonal_facets_centroid_fan(mesh);
        break;
    }
}

// Iterate over mesh (scalar, index) types
#define LA_X_triangulate_polygonal_facets(_, Scalar, Index) \
    template LA_CORE_API void triangulate_polygonal_facets( \
        SurfaceMesh<Scalar, Index>& mesh,                   \
        const TriangulationOptions& options);
LA_SURFACE_MESH_X(triangulate_polygonal_facets, 0)

} // namespace lagrange
