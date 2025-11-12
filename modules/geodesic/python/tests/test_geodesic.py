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

import pytest
import numpy as np


@pytest.fixture
def mesh():
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.eye(3))
    mesh.add_triangle(0, 1, 2)
    assert mesh.num_vertices == 3
    assert mesh.num_facets == 1

    return mesh


class TestDGPCEngine:
    def test_single_source(self, mesh):
        assert not mesh.has_attribute("@geodesic_distance")
        assert not mesh.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineDGPC(mesh)
        dist_id, angle_id = engine.single_source_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
        )
        assert mesh.has_attribute("@geodesic_distance")
        assert mesh.has_attribute("@polar_angle")
        assert mesh.get_attribute_id("@geodesic_distance") == dist_id
        assert mesh.get_attribute_id("@polar_angle") == angle_id

    def test_point_to_point(self, mesh):
        assert not mesh.has_attribute("@geodesic_distance")
        assert not mesh.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineDGPC(mesh)
        distance = engine.point_to_point_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
            target_facet_id=0,
            target_facet_bc=[0.6, 0.2],
        )
        assert mesh.has_attribute("@geodesic_distance")
        assert distance >= 0.0


class TestHeatEngine:
    def test_single_source(self, mesh):
        assert not mesh.has_attribute("@geodesic_distance")
        assert not mesh.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineHeat(mesh)
        dist_id = engine.single_source_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
        )
        assert mesh.has_attribute("@geodesic_distance")
        assert not mesh.has_attribute("@polar_angle")
        assert mesh.get_attribute_id("@geodesic_distance") == dist_id

    def test_point_to_point(self, mesh):
        assert not mesh.has_attribute("@geodesic_distance")
        assert not mesh.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineHeat(mesh)
        distance = engine.point_to_point_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
            target_facet_id=0,
            target_facet_bc=[0.6, 0.2],
        )
        assert mesh.has_attribute("@geodesic_distance")
        assert distance >= 0.0


class TestMMPEngine:
    def test_single_source(self, mesh):
        assert not mesh.has_attribute("@geodesic_distance")
        assert not mesh.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineMMP(mesh)
        dist_id = engine.single_source_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
        )
        assert mesh.has_attribute("@geodesic_distance")
        assert not mesh.has_attribute("@polar_angle")
        assert mesh.get_attribute_id("@geodesic_distance") == dist_id

    def test_point_to_point(self, mesh):
        assert not mesh.has_attribute("@geodesic_distance")
        assert not mesh.has_attribute("@polar_angle")
        engine = lagrange.geodesic.GeodesicEngineMMP(mesh)
        distance = engine.point_to_point_geodesic(
            source_facet_id=0,
            source_facet_bc=[0.3, 0.3],
            target_facet_id=0,
            target_facet_bc=[0.6, 0.2],
        )
        assert mesh.has_attribute("@geodesic_distance")
        assert distance >= 0.0
