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
}

} // namespace lagrange::python
