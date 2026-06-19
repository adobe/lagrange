#
# Copyright 2026 Adobe. All rights reserved.
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


class TestFastWindingNumber:
    def test_is_inside_single(self, cube_triangular):
        engine = lagrange.winding.FastWindingNumber(cube_triangular)
        assert engine.is_inside([0.5, 0.5, 0.5])
        assert not engine.is_inside([2.0, 2.0, 2.0])
        assert not engine.is_inside([-1.0, 0.5, 0.5])

    def test_is_inside_batch(self, cube_triangular):
        engine = lagrange.winding.FastWindingNumber(cube_triangular)
        points = np.array(
            [
                [0.5, 0.5, 0.5],
                [0.1, 0.9, 0.5],
                [2.0, 2.0, 2.0],
                [-1.0, 0.5, 0.5],
            ],
            dtype=float,
        )
        inside = engine.is_inside(points)
        assert inside.shape == (4,)
        assert inside.dtype == bool
        np.testing.assert_array_equal(inside, [True, True, False, False])

    def test_solid_angle_single(self, cube_triangular):
        engine = lagrange.winding.FastWindingNumber(cube_triangular)
        # Solid angle is ~4*pi inside and ~0 outside.
        assert engine.solid_angle([0.5, 0.5, 0.5]) == pytest.approx(4.0 * np.pi, abs=1e-3)
        assert engine.solid_angle([5.0, 5.0, 5.0]) == pytest.approx(0.0, abs=1e-3)

    def test_solid_angle_batch(self, cube_triangular):
        engine = lagrange.winding.FastWindingNumber(cube_triangular)
        points = np.array(
            [
                [0.5, 0.5, 0.5],
                [5.0, 5.0, 5.0],
            ],
            dtype=float,
        )
        angles = engine.solid_angle(points)
        assert angles.shape == (2,)
        assert angles[0] == pytest.approx(4.0 * np.pi, abs=1e-3)
        assert angles[1] == pytest.approx(0.0, abs=1e-3)
