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


# FIXME throws when num_samples=0
@pytest.mark.parametrize("num_samples", [1, 10, 100])
class TestUniformSample:
    def test_single_triangle(self, single_triangle, num_samples):
        mesh = single_triangle
        samples, source_facets = lagrange.sampling.sample_uniform(mesh, num_samples)
        assert len(samples) == num_samples
        assert len(source_facets) == num_samples
        assert np.all(source_facets < mesh.num_facets)

    def test_square(self, square, num_samples):
        mesh = square
        samples, source_facets = lagrange.sampling.sample_uniform(mesh, num_samples)
        assert len(samples) == num_samples
        assert len(source_facets) == num_samples
        assert np.all(source_facets < mesh.num_facets)
