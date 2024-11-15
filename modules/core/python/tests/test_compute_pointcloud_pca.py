#
# Copyright 2024 Adobe. All rights reserved.
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

a = 0.1
b = 0.4
c = 1.2
def make_points():
    return np.array([
        [a, 0, 0],
        [-a, 0, 0],
        [0, -b, 0],
        [0, b, 0],
        [0, 0, c],
        [0, 0, -c],
    ], dtype=np.float64)

class TestComputePointcloudPCA:
    def test_simple(self):
        points = make_points()
        print(points.shape)
        center, eigenvectors, eigenvalues = lagrange.compute_pointcloud_pca(points)
        assert center[0] == 0
        assert center[1] == 0
        assert center[2] == 0
        assert eigenvalues[0] == 2 * a * a
        assert eigenvalues[1] == 2 * b * b
        assert eigenvalues[2] == 2 * c * c
