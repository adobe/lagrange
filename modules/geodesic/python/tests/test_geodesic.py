#
# Copyright 2025 Adobe. All rights reserved.
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


class TestDGPCEngine:
    def test_single_source(self, single_triangle):
        assert not single_triangle.has_attribute("@geodesic_distance")
        assert not single_triangle.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineDGPC(single_triangle)
        dist_id, angle_id = engine.single_source_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
        )
        assert single_triangle.has_attribute("@geodesic_distance")
        assert single_triangle.has_attribute("@polar_angle")
        assert single_triangle.get_attribute_id("@geodesic_distance") == dist_id
        assert single_triangle.get_attribute_id("@polar_angle") == angle_id

    def test_point_to_point(self, single_triangle):
        assert not single_triangle.has_attribute("@geodesic_distance")
        assert not single_triangle.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineDGPC(single_triangle)
        distance = engine.point_to_point_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
            target_facet_id=0,
            target_facet_bc=[0.6, 0.2],
        )
        assert single_triangle.has_attribute("@geodesic_distance")
        assert distance >= 0.0


class TestHeatEngine:
    def test_single_source(self, single_triangle):
        assert not single_triangle.has_attribute("@geodesic_distance")
        assert not single_triangle.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineHeat(single_triangle)
        dist_id = engine.single_source_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
        )
        assert single_triangle.has_attribute("@geodesic_distance")
        assert not single_triangle.has_attribute("@polar_angle")
        assert single_triangle.get_attribute_id("@geodesic_distance") == dist_id

    def test_point_to_point(self, single_triangle):
        assert not single_triangle.has_attribute("@geodesic_distance")
        assert not single_triangle.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineHeat(single_triangle)
        distance = engine.point_to_point_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
            target_facet_id=0,
            target_facet_bc=[0.6, 0.2],
        )
        assert single_triangle.has_attribute("@geodesic_distance")
        assert distance >= 0.0


class TestMMPEngine:
    def test_single_source(self, single_triangle):
        assert not single_triangle.has_attribute("@geodesic_distance")
        assert not single_triangle.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineMMP(single_triangle)
        dist_id = engine.single_source_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
        )
        assert single_triangle.has_attribute("@geodesic_distance")
        assert not single_triangle.has_attribute("@polar_angle")
        assert single_triangle.get_attribute_id("@geodesic_distance") == dist_id

    def test_point_to_point(self, single_triangle):
        assert not single_triangle.has_attribute("@geodesic_distance")
        assert not single_triangle.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineMMP(single_triangle)
        distance = engine.point_to_point_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
            target_facet_id=0,
            target_facet_bc=[0.6, 0.2],
        )
        assert single_triangle.has_attribute("@geodesic_distance")
        assert distance >= 0.0

    def test_point_to_point_path(self, single_triangle):
        engine = lagrange.geodesic.GeodesicEngineMMP(single_triangle)
        points, facet_ids = engine.point_to_point_geodesic_path(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
            target_facet_id=0,
            target_facet_bc=[0.6, 0.2],
        )
        assert len(points) >= 2  # At least source and target
        assert all(len(p) == 3 for p in points)  # Each point is 3D
        assert len(facet_ids) == len(points) - 1  # One facet per segment


class TestSphereGeodesic:
    """
    Test geodesic distances on a sphere.

    For a sphere of radius r, the relationship between Euclidean distance d_E
    and geodesic distance d_G is:

        d_G = 2 * r * arcsin(d_E / (2 * r))

    This test verifies this mathematical relationship.
    """

    def test_geodesic_distance_on_sphere(self, sphere_mesh):
        """
        Test that geodesic distance on a sphere matches the theoretical formula.

        The geodesic distance should satisfy:
            d_geodesic = 2 * r * arcsin(d_euclidean / (2 * r))

        And the bounds:
            d_euclidean <= d_geodesic <= pi * r
        """
        radius = 10.0

        # Create MMP engine
        engine = lagrange.geodesic.GeodesicEngineMMP(sphere_mesh)

        # Test with several pairs of points
        test_cases = [
            # (source_facet, target_facet) - different locations on sphere
            (0, 10),  # Nearby points
            (0, 50),  # Medium distance
            (0, 100),  # Larger distance
        ]

        for source_facet, target_facet in test_cases:
            # Compute geodesic distance
            d_geodesic = engine.point_to_point_geodesic(
                source_facet_id=source_facet,
                target_facet_id=target_facet,
                source_facet_bc=[0.33, 0.33],
                target_facet_bc=[0.33, 0.33],
            )

            # Compute Euclidean distance between the same points
            vertices = sphere_mesh.vertices
            facets = sphere_mesh.facets

            # Get barycentric interpolated positions
            source_triangle = facets[source_facet]
            source_pos = (
                (1 - 0.33 - 0.33) * vertices[source_triangle[0]]
                + 0.33 * vertices[source_triangle[1]]
                + 0.33 * vertices[source_triangle[2]]
            )

            target_triangle = facets[target_facet]
            target_pos = (
                (1 - 0.33 - 0.33) * vertices[target_triangle[0]]
                + 0.33 * vertices[target_triangle[1]]
                + 0.33 * vertices[target_triangle[2]]
            )

            d_euclidean = np.linalg.norm(target_pos - source_pos)

            # Theoretical geodesic distance on sphere
            # d_G = 2 * r * arcsin(d_E / (2 * r))
            d_theoretical = 2 * radius * np.arcsin(d_euclidean / (2 * radius))

            # Verify bounds: d_euclidean <= d_geodesic <= pi * r
            assert d_euclidean <= d_geodesic, (
                f"Geodesic shorter than Euclidean: {d_geodesic} < {d_euclidean}"
            )
            assert d_geodesic <= np.pi * radius, (
                f"Geodesic longer than max: {d_geodesic} > {np.pi * radius}"
            )

            # Verify the geodesic distance matches theory
            # Allow some tolerance due to mesh discretization (~5%)
            # Note: The theoretical formula assumes points on an exact sphere, but we're working
            # with a triangulated mesh where barycentric coordinates lie on planar facets.
            # The tolerance accounts for this discretization error.
            tolerance = 0.05 * d_theoretical
            assert np.abs(d_geodesic - d_theoretical) < tolerance, (
                f"Geodesic distance {d_geodesic:.4f} doesn't match theoretical {d_theoretical:.4f} "
                f"(Euclidean: {d_euclidean:.4f}, error: {np.abs(d_geodesic - d_theoretical):.4f})"
            )
