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

#include <lagrange/bvh/EdgeAABBTree.h>
#include <lagrange/bvh/TriangleAABBTree.h>
#include <lagrange/bvh/compute_mesh_distances.h>
#include <lagrange/bvh/compute_uv_overlap.h>
#include <lagrange/bvh/remove_interior_shells.h>
#include <lagrange/bvh/weld_vertices.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/bvh.h>
#include <lagrange/python/eigen_utils.h>

#include "PyEdgeAABBTree.h"

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

void populate_bvh_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    using VertexArray3D = Eigen::Matrix<Scalar, Eigen::Dynamic, 3>;
    using VertexArray2D = Eigen::Matrix<Scalar, Eigen::Dynamic, 2>;
    using EdgeArray = Eigen::Matrix<Index, Eigen::Dynamic, 2>;

    using NDArray3D =
        nb::ndarray<Scalar, nb::numpy, nb::shape<-1, 3>, nb::c_contig, nb::device::cpu>;
    using NDArray2D =
        nb::ndarray<Scalar, nb::numpy, nb::shape<-1, 2>, nb::c_contig, nb::device::cpu>;

    using MeshType = SurfaceMesh<Scalar, Index>;

    // TriangleAABBTree bindings
    nb::class_<bvh::TriangleAABBTree<Scalar, Index, 3>>(m, "TriangleAABBTree3D")
        .def(nb::init<const MeshType&>(), "mesh"_a, "Construct AABB tree from a triangle mesh")
        .def("empty", &bvh::TriangleAABBTree<Scalar, Index, 3>::empty, "Check if the tree is empty")
        .def(
            "get_elements_in_radius",
            [](const bvh::TriangleAABBTree<Scalar, Index, 3>& tree,
               const GenericPoint<Scalar, 3>& query_point,
               Scalar radius) {
                std::vector<Index> elements;
                std::vector<Scalar> closest_pts;
                Point<Scalar, 3> q = to_eigen_point<Scalar, 3>(query_point);
                tree.foreach_triangle_in_radius(
                    q,
                    radius * radius, // Convert radius to squared distance
                    [&](Scalar,
                        Index triangle_id,
                        const Eigen::Matrix<Scalar, 1, 3>& closest_point) {
                        elements.push_back(triangle_id);
                        closest_pts.insert(
                            closest_pts.end(),
                            closest_point.data(),
                            closest_point.data() + 3);
                    });

                NDArray3D nb_closest_pts(closest_pts.data(), {closest_pts.size() / 3, 3});
                return std::tuple(
                    elements,
                    nb_closest_pts.cast()); // cast() used to copy memory to python.
            },
            "query_point"_a,
            "radius"_a,
            R"(Find all elements within a given radius from a query point.

:param query_point: Query point.
:param radius: Search radius.

:return: A tuple containing:
    - A list of element indices within the specified radius.
    - A NumPy array of shape (N, 3) containing the closest points on each element.)")
        .def(
            "get_closest_point",
            [](const bvh::TriangleAABBTree<Scalar, Index, 3>& tree,
               const GenericPoint<Scalar, 3>& query_point) {
                Point<Scalar, 3> q = to_eigen_point<Scalar, 3>(query_point);
                Index triangle_id;
                Eigen::Matrix<Scalar, 1, 3> closest_point;
                Scalar closest_sq_dist;

                tree.get_closest_point(q, triangle_id, closest_point, closest_sq_dist);
                return std::make_tuple(triangle_id, closest_point, closest_sq_dist);
            },
            "query_point"_a,
            R"(Find the closest element and point within the element to the query point

:param query_point: Query point.
:return: A tuple containing:
    - The index of the closest element.
    - A NumPy array representing the closest point on the element.
    - The squared distance between the query point and the closest point.)");

    // 2D version
    nb::class_<bvh::TriangleAABBTree<Scalar, Index, 2>>(m, "TriangleAABBTree2D")
        .def(
            nb::init<const SurfaceMesh<Scalar, Index>&>(),
            "mesh"_a,
            "Construct AABB tree from a 2D triangle mesh")
        .def("empty", &bvh::TriangleAABBTree<Scalar, Index, 2>::empty, "Check if the tree is empty")
        .def(
            "get_elements_in_radius",
            [](const bvh::TriangleAABBTree<Scalar, Index, 2>& tree,
               const GenericPoint<Scalar, 2>& query_point,
               Scalar radius) {
                std::vector<Index> elements;
                std::vector<Scalar> closest_pts;
                Point<Scalar, 2> q = to_eigen_point<Scalar, 2>(query_point);
                tree.foreach_triangle_in_radius(
                    q,
                    radius * radius, // Convert radius to squared distance
                    [&](Scalar,
                        Index triangle_id,
                        const Eigen::Matrix<Scalar, 1, 2>& closest_point) {
                        elements.push_back(triangle_id);
                        closest_pts.insert(
                            closest_pts.end(),
                            closest_point.data(),
                            closest_point.data() + 2);
                    });

                NDArray2D nb_closest_pts(closest_pts.data(), {closest_pts.size() / 2, 2});
                return std::tuple(
                    elements,
                    nb_closest_pts.cast()); // cast() used to copy memory to python.
            },
            "query_point"_a,
            "radius"_a,
            R"(Find all elements within a given radius from a query point.

:param query_point: Query point.
:param radius: Search radius.

:return: A tuple containing:
    - A list of element indices within the specified radius.
    - A NumPy array of shape (N, 2) containing the closest points on each element.)")
        .def(
            "get_closest_point",
            [](const bvh::TriangleAABBTree<Scalar, Index, 2>& tree,
               const GenericPoint<Scalar, 2>& query_point) {
                Point<Scalar, 2> q = to_eigen_point<Scalar, 2>(query_point);
                Index triangle_id;
                Eigen::Matrix<Scalar, 1, 2> closest_point;
                Scalar closest_sq_dist;

                tree.get_closest_point(q, triangle_id, closest_point, closest_sq_dist);
                return std::make_tuple(triangle_id, closest_point, closest_sq_dist);
            },
            "query_point"_a,
            R"(Find the closest element and point within the element to the query point

:param query_point: Query point.
:return: A tuple containing:
    - The index of the closest element.
    - A NumPy array representing the closest point on the element.
    - The squared distance between the query point and the closest point.)");

    // EdgeAABBTree bindings
    // 3D EdgeAABBTree
    nb::class_<bvh::PyEdgeAABBTree<Scalar, Index, 3>>(m, "EdgeAABBTree3D")
        .def(
            nb::init<const VertexArray3D&, const EdgeArray&>(),
            "vertices"_a,
            "edges"_a,
            "Construct AABB tree from 3D edge graph")
        .def("empty", &bvh::PyEdgeAABBTree<Scalar, Index, 3>::empty, "Check if the tree is empty")
        .def(
            "get_element_closest_point",
            [](const bvh::PyEdgeAABBTree<Scalar, Index, 3>& tree,
               const GenericPoint<Scalar, 3>& query_point,
               Index element_id) {
                Point<Scalar, 3> q = to_eigen_point<Scalar, 3>(query_point);
                Eigen::Matrix<Scalar, 1, 3> closest_point;
                Scalar closest_sq_dist;
                tree.get_element_closest_point(q, element_id, closest_point, closest_sq_dist);
                return std::make_tuple(closest_point, closest_sq_dist);
            },
            "query_point"_a,
            "element_id"_a,
            R"(Get the closest point on a specific edge.

:param query_point: Query point.
:param element_id: Index of the edge to query.

:return: A tuple containing:
    - A NumPy array representing the closest point on the edge.
    - The squared distance between the query point and the closest point.)")
        .def(
            "get_elements_in_radius",
            [](const bvh::PyEdgeAABBTree<Scalar, Index, 3>& tree,
               const GenericPoint<Scalar, 3>& query_point,
               Scalar radius) {
                std::vector<Index> elements;
                std::vector<Scalar> closest_pts;
                Point<Scalar, 3> q = to_eigen_point<Scalar, 3>(query_point);
                tree.foreach_element_in_radius(
                    q,
                    radius * radius, // Convert radius to squared distance
                    [&](Scalar, Index edge_id, const Eigen::Matrix<Scalar, 1, 3>& closest_point) {
                        elements.push_back(edge_id);
                        closest_pts.insert(
                            closest_pts.end(),
                            closest_point.data(),
                            closest_point.data() + 3);
                    });

                NDArray3D nb_closest_pts(closest_pts.data(), {closest_pts.size() / 3, 3});
                return std::tuple(
                    elements,
                    nb_closest_pts.cast()); // cast() used to copy memory to python.
            },
            "query_point"_a,
            "radius"_a,
            R"(Find all elements within a given radius from a query point.

:param query_point: Query point.
:param radius: Search radius.

:return: A tuple containing:
    - A list of element indices within the specified radius.
    - A NumPy array of shape (N, 3) containing the closest points on each element.)")
        .def(
            "get_containing_elements",
            [](const bvh::PyEdgeAABBTree<Scalar, Index, 3>& tree,
               const GenericPoint<Scalar, 3>& query_point) {
                std::vector<Index> results;
                Point<Scalar, 3> q = to_eigen_point<Scalar, 3>(query_point);
                tree.foreach_element_containing(
                    q,
                    [&results](Scalar, Index edge_id, const Eigen::Matrix<Scalar, 1, 3>&) {
                        results.push_back(edge_id);
                    });
                return results;
            },
            "query_point"_a,
            R"(Find all elements that contain the query point.

:param query_point: Query point.
:return: A list of element indices that contain the query point.)")
        .def(
            "get_closest_point",
            [](const bvh::PyEdgeAABBTree<Scalar, Index, 3>& tree,
               const GenericPoint<Scalar, 3>& query_point) {
                Point<Scalar, 3> q = to_eigen_point<Scalar, 3>(query_point);
                Index element_id = invalid<Index>();
                Eigen::Matrix<Scalar, 1, 3> closest_point;
                Scalar closest_sq_dist;
                tree.get_closest_point(q, element_id, closest_point, closest_sq_dist);
                return std::make_tuple(element_id, closest_point, closest_sq_dist);
            },
            "query_point"_a,
            R"(Find the closest element and point within the element to the query point

:param query_point: Query point.
:return: A tuple containing:
    - The index of the closest element.
    - A NumPy array representing the closest point on the element.
    - The squared distance between the query point and the closest point.)");

    // 2D EdgeAABBTree
    nb::class_<bvh::PyEdgeAABBTree<Scalar, Index, 2>>(m, "EdgeAABBTree2D")
        .def(
            nb::init<const VertexArray2D&, const EdgeArray&>(),
            "vertices"_a,
            "edges"_a,
            "Construct AABB tree from 2D edge graph")
        .def("empty", &bvh::PyEdgeAABBTree<Scalar, Index, 2>::empty, "Check if the tree is empty")
        .def(
            "get_element_closest_point",
            [](const bvh::PyEdgeAABBTree<Scalar, Index, 2>& tree,
               const GenericPoint<Scalar, 2>& query_point,
               Index element_id) {
                Point<Scalar, 2> q = to_eigen_point<Scalar, 2>(query_point);
                Point<Scalar, 2> closest_point;
                Scalar closest_sq_dist;
                tree.get_element_closest_point(q, element_id, closest_point, closest_sq_dist);
                return std::make_tuple(closest_point, closest_sq_dist);
            },
            "query_point"_a,
            "element_id"_a,
            R"(Get the closest point on a specific edge.

:param query_point: Query point.
:param element_id: Index of the edge to query.

:return: A tuple containing:
    - A NumPy array representing the closest point on the edge.
    - The squared distance between the query point and the closest point.)")
        .def(
            "get_elements_in_radius",
            [](const bvh::PyEdgeAABBTree<Scalar, Index, 2>& tree,
               const GenericPoint<Scalar, 2>& query_point,
               Scalar radius) {
                std::vector<Index> elements;
                std::vector<Scalar> closest_pts;
                Point<Scalar, 2> q = to_eigen_point<Scalar, 2>(query_point);
                tree.foreach_element_in_radius(
                    q,
                    radius * radius, // Convert radius to squared distance
                    [&](Scalar, Index edge_id, const Eigen::Matrix<Scalar, 1, 2>& closest_point) {
                        elements.push_back(edge_id);
                        closest_pts.insert(
                            closest_pts.end(),
                            closest_point.data(),
                            closest_point.data() + 2);
                    });

                NDArray2D nb_closest_pts(closest_pts.data(), {closest_pts.size() / 2, 2});
                return std::tuple(
                    elements,
                    nb_closest_pts.cast()); // cast() used to copy memory to python.
            },
            "query_point"_a,
            "radius"_a,
            R"(Find all elements within a given radius from a query point.

:param query_point: Query point.
:param radius: Search radius.

:return: A tuple containing:
    - A list of element indices within the specified radius.
    - A NumPy array of shape (N, 2) containing the closest points on each element.)")
        .def(
            "get_containing_elements",
            [](const bvh::PyEdgeAABBTree<Scalar, Index, 2>& tree,
               const GenericPoint<Scalar, 2>& query_point) {
                std::vector<Index> results;
                Point<Scalar, 2> q = to_eigen_point<Scalar, 2>(query_point);
                tree.foreach_element_containing(
                    q,
                    [&results](Scalar, Index edge_id, const Eigen::Matrix<Scalar, 1, 2>&) {
                        results.push_back(edge_id);
                    });
                return results;
            },
            "query_point"_a,
            R"(Find all elements that contain the query point.

:param query_point: Query point.
:return: A list of element indices that contain the query point.)")
        .def(
            "get_closest_point",
            [](const bvh::PyEdgeAABBTree<Scalar, Index, 2>& tree,
               const GenericPoint<Scalar, 2>& query_point) {
                Point<Scalar, 2> q = to_eigen_point<Scalar, 2>(query_point);
                Index element_id;
                Eigen::Matrix<Scalar, 1, 2> closest_point;
                Scalar closest_sq_dist;
                tree.get_closest_point(q, element_id, closest_point, closest_sq_dist);
                return std::make_tuple(element_id, closest_point, closest_sq_dist);
            },
            "query_point"_a,
            R"(Find the closest element and point within the element to the query point

:param query_point: Query point.
:return: A tuple containing:
    - The index of the closest element.
    - A NumPy array representing the closest point on the element.
    - The squared distance between the query point and the closest point.)");

    m.def(
        "compute_mesh_distances",
        [](MeshType& source, const MeshType& target, const std::string& output_attribute_name) {
            bvh::MeshDistancesOptions opts;
            opts.output_attribute_name = output_attribute_name;
            return bvh::compute_mesh_distances(source, target, opts);
        },
        "source"_a,
        "target"_a,
        "output_attribute_name"_a = std::string(bvh::MeshDistancesOptions{}.output_attribute_name),
        R"(Compute the distance from each vertex in source to the closest point on target.

The result is stored as a per-vertex scalar attribute on source.
Both meshes must have the same spatial dimension and target must be a triangle mesh.

:param source: Mesh whose vertices are queried. The output attribute is added here.
:param target: Triangle mesh against which distances are computed.
:param output_attribute_name: Name of the output per-vertex attribute.

:return: AttributeId of the newly created (or overwritten) distance attribute on source.)");

    m.def(
        "compute_hausdorff",
        &bvh::compute_hausdorff<Scalar, Index>,
        "source"_a,
        "target"_a,
        R"(Compute the symmetric Hausdorff distance between two meshes.

The Hausdorff distance is the maximum of the two directed Hausdorff distances:
.. math::

   H(A, B) = \max \left(
     \max_{a \in A} \text{dist}(a, B), \quad
     \max_{b \in B} \text{dist}(b, A)
   \right)

where dist(v, M) is the distance from vertex v to the closest point on mesh M.

Both meshes must have the same spatial dimension and must be triangle meshes.

:param source: First mesh.
:param target: Second mesh.

:return: Hausdorff distance.)");

    m.def(
        "compute_chamfer",
        &bvh::compute_chamfer<Scalar, Index>,
        "source"_a,
        "target"_a,
        R"(Compute the Chamfer distance between two meshes.

The Chamfer distance is defined as:
.. math::

   \begin{aligned}
     C(A, B) = &\frac{1}{\|A\|} \sum_{a \in A} \text{dist}(a, B)^2 \\
               &+ \frac{1}{\|B\|} \sum_{b \in B} \text{dist}(b, A)^2
   \end{aligned}

where dist(v, M) is the distance from vertex v to the closest point on mesh M.

Both meshes must have the same spatial dimension and must be triangle meshes.

:param source: First mesh.
:param target: Second mesh.

:return: Chamfer distance.)");

    m.def(
        "weld_vertices",
        [](MeshType& mesh, Scalar radius, bool boundary_only) {
            bvh::WeldOptions options;
            options.radius = radius;
            options.boundary_only = boundary_only;
            bvh::weld_vertices(mesh, options);
        },
        "mesh"_a,
        "radius"_a = 1e-6f,
        "boundary_only"_a = false,
        R"(Weld nearby vertices together of a surface mesh.

:param mesh: The target surface mesh to be welded in place.
:param radius: The maximum distance between vertices to be considered for welding. Default is 1e-6.
:param boundary_only: If true, only boundary vertices will be considered for welding. Defaults to False.

.. warning:: This method may introduce non-manifoldness and degeneracy in the mesh.)");

    m.def(
        "remove_interior_shells",
        bvh::remove_interior_shells<Scalar, Index>,
        "mesh"_a,
        R"(Removes interior shells from a (manifold, non-intersecting) mesh

.. warning:: This method assumes that the input mesh is closed, manifold and has no self-intersections.
             The result may be invalid if these conditions are not met.

:param mesh: Input mesh to process.
:return: A new mesh with interior shells removed.)");

    // UV overlap enum
    nb::enum_<bvh::UVOverlapMethod>(m, "UVOverlapMethod")
        .value(
            "SweepAndPrune",
            bvh::UVOverlapMethod::SweepAndPrune,
            "Zomorodian-Edelsbrunner sweep-and-prune.")
        .value("BVH", bvh::UVOverlapMethod::BVH, "AABB tree per-triangle query.")
        .value(
            "Hybrid",
            bvh::UVOverlapMethod::Hybrid,
            "Zomorodian-Edelsbrunner HYBRID algorithm (recursive divide-and-conquer).");

    // UV overlap result (Python NamedTuple)
    // Note: No direct nanobind support for NamedTuple yet, see:
    // https://github.com/wjakob/nanobind/discussions/1279
    nb::object typing = nb::module_::import_("typing");
    nb::object builtins = nb::module_::import_("builtins");
    nb::object UVOverlapResult = typing.attr("NamedTuple")(
        "UVOverlapResult",
        nb::make_tuple(
            nb::make_tuple("has_overlap", builtins.attr("bool")),
            nb::make_tuple("overlap_area", typing.attr("Optional")[builtins.attr("float")]),
            nb::make_tuple("overlapping_pairs", builtins.attr("list")),
            nb::make_tuple("overlap_coloring_id", builtins.attr("int"))));
    m.attr("UVOverlapResult") = UVOverlapResult;

    // compute_uv_overlap function
    m.def(
        "compute_uv_overlap",
        [UVOverlapResult](
            MeshType& mesh,
            std::string uv_attribute_name,
            bool compute_overlap_area,
            bool compute_overlap_coloring,
            std::string overlap_coloring_attribute_name,
            bool compute_overlapping_pairs,
            bvh::UVOverlapMethod method) -> nb::object {
            bvh::UVOverlapOptions opts;
            opts.uv_attribute_name = std::move(uv_attribute_name);
            opts.compute_overlap_area = compute_overlap_area;
            opts.compute_overlap_coloring = compute_overlap_coloring;
            opts.overlap_coloring_attribute_name = std::move(overlap_coloring_attribute_name);
            opts.compute_overlapping_pairs = compute_overlapping_pairs;
            opts.method = method;
            auto result = bvh::compute_uv_overlap(mesh, opts);

            nb::object area = result.overlap_area.has_value()
                                  ? nb::cast(result.overlap_area.value())
                                  : nb::none();

            nb::list pairs;
            for (auto& [i, j] : result.overlapping_pairs) {
                pairs.append(nb::make_tuple(i, j));
            }

            return UVOverlapResult(
                nb::cast(result.has_overlap),
                area,
                pairs,
                nb::cast(result.overlap_coloring_id));
        },
        "mesh"_a,
        "uv_attribute_name"_a = bvh::UVOverlapOptions{}.uv_attribute_name,
        "compute_overlap_area"_a = bvh::UVOverlapOptions{}.compute_overlap_area,
        "compute_overlap_coloring"_a = bvh::UVOverlapOptions{}.compute_overlap_coloring,
        "overlap_coloring_attribute_name"_a =
            bvh::UVOverlapOptions{}.overlap_coloring_attribute_name,
        "compute_overlapping_pairs"_a = bvh::UVOverlapOptions{}.compute_overlapping_pairs,
        "method"_a = bvh::UVOverlapOptions{}.method,
        R"(Compute pairwise UV triangle overlap.

For every pair of UV-space triangles whose 2-D bounding boxes intersect, an exact
separating-axis test using orient2D predicates confirms a genuine interior intersection
before computing the intersection area via Sutherland-Hodgman clipping.

Triangles that share only a boundary edge or a single vertex are never counted as
overlapping.

:param mesh: Input triangle mesh with a UV attribute.
:param uv_attribute_name: UV attribute name. Empty string uses the first UV attribute found. Vertex, indexed and corner attributes are supported.
:param compute_overlap_area: If True, compute the total overlap area (default: True).
:param compute_overlap_coloring: If True, compute a per-facet coloring attribute (default: False).
:param overlap_coloring_attribute_name: Name of the coloring attribute (default: "@uv_overlap_color").
:param compute_overlapping_pairs: If True, return the list of overlapping pairs (default: False).
:param method: Candidate detection algorithm (default: UVOverlapMethod.Hybrid).

:return: UVOverlapResult containing overlap detection results.)");
}

} // namespace lagrange::python
