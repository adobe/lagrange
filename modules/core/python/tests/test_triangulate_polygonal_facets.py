#
# Copyright 2022 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
import lagrange
import numpy as np
import pytest


class TestTriangulatePolygonalFacets:
    def test_empty_mesh(self):
        mesh = lagrange.SurfaceMesh()
        lagrange.triangulate_polygonal_facets(mesh)
        assert mesh.num_vertices == 0

    def test_cube(self, cube):
        mesh = cube

        # Update growth policy to allow copy.
        mesh.attribute(
            mesh.attr_id_corner_to_vertex
        ).growth_policy = lagrange.AttributeGrowthPolicy.WarnAndCopy

        lagrange.triangulate_polygonal_facets(mesh)
        assert mesh.num_facets == 12

    def test_cube_with_attribute(self, cube):
        mesh = cube
        attr_id = lagrange.compute_normal(mesh)

        # Update growth policy to allow copy.
        mesh.attribute(
            mesh.attr_id_corner_to_vertex
        ).growth_policy = lagrange.AttributeGrowthPolicy.WarnAndCopy

        lagrange.triangulate_polygonal_facets(mesh)
        assert mesh.num_facets == 12
        assert mesh.is_attribute_indexed(attr_id)

        normal_attr = mesh.indexed_attribute(attr_id)
        normal_indices = normal_attr.indices
        assert normal_indices.num_elements == mesh.num_corners

    @pytest.mark.parametrize("scheme", ["earcut", "centroid_fan", "delaunay"])
    def test_selected_facets_bool_mask(self, cube, scheme):
        # All schemes honor `selected_facets`, including for quads. Triangulate only two of the
        # six cube (quad) facets and check the other four survive as quads.
        mesh = cube
        mask = np.zeros(mesh.num_facets, dtype=bool)
        mask[0] = True
        mask[2] = True

        lagrange.triangulate_polygonal_facets(mesh, scheme, mask)

        sizes = sorted(mesh.get_facet_size(f) for f in range(mesh.num_facets))
        assert sizes.count(4) == 4  # four untouched quads
        # A quad becomes 2 triangles (earcut / delaunay) or 4 triangles via a centroid fan.
        expected_triangles = {"earcut": 4, "delaunay": 4, "centroid_fan": 8}[scheme]
        assert sizes.count(3) == expected_triangles

    def test_selected_facets_inputs_are_equivalent(self, cube):
        vertices = cube.vertices.copy()
        facets = cube.facets.copy()

        def triangulate(selected):
            mesh = lagrange.SurfaceMesh()
            mesh.vertices = vertices
            mesh.facets = facets
            lagrange.triangulate_polygonal_facets(mesh, "centroid_fan", selected)
            return sorted(mesh.get_facet_size(f) for f in range(mesh.num_facets))

        mask = np.zeros(cube.num_facets, dtype=bool)
        mask[0] = True
        mask[2] = True

        from_mask = triangulate(mask)
        from_list = triangulate([0, 2])
        from_array = triangulate(np.array([0, 2], dtype=np.uint32))

        assert from_mask == from_list == from_array

    def test_selected_facets_default_triangulates_all(self, cube):
        mesh = cube
        lagrange.triangulate_polygonal_facets(mesh, "centroid_fan")
        assert all(mesh.get_facet_size(f) == 3 for f in range(mesh.num_facets))

    @pytest.mark.parametrize("scheme", ["earcut", "centroid_fan"])
    def test_selected_facets_empty_is_noop_with_edges(self, cube, scheme):
        # An empty selection must be a no-op for both schemes, even when the mesh has edge
        # connectivity (guards against add_polygons asserting on empty facet buffers).
        mesh = cube
        mesh.initialize_edges()
        old_num_facets = mesh.num_facets
        empty_mask = np.zeros(mesh.num_facets, dtype=bool)
        lagrange.triangulate_polygonal_facets(mesh, scheme, empty_mask)
        assert mesh.num_facets == old_num_facets

    def test_selected_facets_bad_mask_size(self, cube):
        mesh = cube
        with pytest.raises(RuntimeError):
            lagrange.triangulate_polygonal_facets(
                mesh, "centroid_fan", np.zeros(mesh.num_facets + 1, dtype=bool)
            )

    def test_selected_facets_out_of_range(self, cube):
        mesh = cube
        with pytest.raises(RuntimeError):
            lagrange.triangulate_polygonal_facets(mesh, "centroid_fan", [mesh.num_facets])

    def test_cube_delaunay(self, cube):
        mesh = cube
        area = lagrange.compute_mesh_area(mesh)

        # Update growth policy to allow copy.
        mesh.attribute(
            mesh.attr_id_corner_to_vertex
        ).growth_policy = lagrange.AttributeGrowthPolicy.WarnAndCopy

        lagrange.triangulate_polygonal_facets(mesh, "delaunay")
        # Delaunay is earcut + edge flips: no new vertices, each quad -> 2 triangles.
        assert mesh.num_vertices == 8
        assert mesh.num_facets == 12
        assert all(mesh.get_facet_size(f) == 3 for f in range(mesh.num_facets))
        assert lagrange.compute_mesh_area(mesh) == pytest.approx(area, rel=1e-5)

    def test_delaunay_refines_polygon_sliver(self):
        # A near-triangle 8-gon: edge A--B is subdivided by points on a shallow convex arc bulging
        # away from apex C, so each interior point is a thin convex ear. Plain earcut clips these
        # into sliver triangles; the delaunay scheme flips them away. A polygon with more than four
        # vertices is required here because quads bypass `mapbox::refine`.
        A = np.array([0.0, 0.0, 0.0])
        B = np.array([1.0, 0.0, 0.0])
        C = np.array([0.5, 1.0, 0.0])
        n = 6
        pts = [A]
        for i in range(1, n):
            t = i / n
            pts.append(np.array([t, -1e-5 * t * (1.0 - t), 0.0]))
        pts += [B, C]
        pts = np.array(pts)

        def triangulate(scheme):
            mesh = lagrange.SurfaceMesh()
            mesh.vertices = pts
            mesh.add_polygon(np.arange(len(pts), dtype=np.uint32))
            lagrange.triangulate_polygonal_facets(mesh, scheme)
            assert mesh.num_vertices == len(pts)  # no new vertices
            assert mesh.num_facets == n  # (n + 2)-gon -> n triangles
            v = mesh.vertices
            areas = [
                0.5 * np.linalg.norm(np.cross(v[b] - v[a], v[c] - v[a])) for a, b, c in mesh.facets
            ]
            return min(areas), sorted(tuple(sorted(t)) for t in mesh.facets)

        earcut_min, earcut_tris = triangulate("earcut")
        delaunay_min, delaunay_tris = triangulate("delaunay")

        # The two schemes must differ (guards against "delaunay" silently mapping to earcut), and
        # delaunay's smallest triangle must be far larger than earcut's near-degenerate sliver.
        assert earcut_tris != delaunay_tris
        assert earcut_min < 1e-6
        assert delaunay_min > 1e-3

    def test_unsupported_scheme(self, cube):
        with pytest.raises(RuntimeError):
            lagrange.triangulate_polygonal_facets(cube, "not_a_scheme")

    def test_cube_with_attribute_centroid_fan(self, cube):
        mesh = cube
        attr_id = lagrange.compute_normal(mesh)
        area = lagrange.compute_mesh_area(mesh)

        lagrange.triangulate_polygonal_facets(mesh, "centroid_fan")
        assert mesh.num_vertices == 8 + 6
        assert mesh.num_facets == 24
        assert mesh.is_attribute_indexed(attr_id)

        normal_attr = mesh.indexed_attribute(attr_id)
        normal_indices = normal_attr.indices
        assert normal_indices.num_elements == mesh.num_corners

        assert lagrange.compute_mesh_area(mesh) == pytest.approx(area, rel=1e-5)
