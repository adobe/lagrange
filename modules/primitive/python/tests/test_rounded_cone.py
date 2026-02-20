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
import pytest
import math


class TestRoundedCone:
    def test_cone(self):
        mesh = lagrange.primitive.generate_rounded_cone(triangulate=True)
        assert lagrange.is_closed(mesh)
        assert lagrange.is_oriented(mesh)
        assert lagrange.is_manifold(mesh)
        assert lagrange.compute_euler(mesh) == 2
        assert len(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.UV)) == 1
        assert len(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.Normal)) == 1

    def test_cone_with_bevel(self):
        mesh = lagrange.primitive.generate_rounded_cone(
            triangulate=True, bevel_radius_top=0.1, bevel_radius_bottom=0.1
        )
        assert lagrange.is_closed(mesh)
        assert lagrange.is_oriented(mesh)
        assert lagrange.is_manifold(mesh)
        assert lagrange.compute_euler(mesh) == 2
        assert len(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.UV)) == 1
        assert len(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.Normal)) == 1

    def test_sphere(self):
        mesh = lagrange.primitive.generate_rounded_cone(
            radius_top=0.5,
            radius_bottom=0.5,
            height=1,
            triangulate=True,
            bevel_radius_top=0.5,
            bevel_radius_bottom=0.5,
            bevel_segments_top=16,
            bevel_segments_bottom=16,
        )
        assert lagrange.is_closed(mesh)
        assert lagrange.is_oriented(mesh)
        assert lagrange.is_manifold(mesh)
        assert lagrange.compute_euler(mesh) == 2
        assert len(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.UV)) == 1
        assert len(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.Normal)) == 1

    def test_partial_cone(self):
        mesh = lagrange.primitive.generate_rounded_cone(
            radius_top=0.5,
            radius_bottom=0.5,
            height=1,
            triangulate=True,
            end_sweep_angle=math.radians(270),
        )
        assert lagrange.is_closed(mesh)
        assert lagrange.is_oriented(mesh)
        assert lagrange.is_manifold(mesh)
        assert lagrange.compute_euler(mesh) == 2
        assert len(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.UV)) == 1
        assert len(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.Normal)) == 1
