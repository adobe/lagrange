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
class TestBlueNoiseSampleUniform:
    def test_single_triangle(self, single_triangle, num_samples):
        mesh = single_triangle
        samples, source_facets, densities = lagrange.sampling.sample_blue_noise(
            mesh,
            num_samples,
        )
        assert len(samples) == num_samples
        assert len(source_facets) == num_samples
        assert len(densities) == num_samples
        assert np.all(source_facets < mesh.num_facets)
        assert np.all(densities > 0)

    def test_square(self, square, num_samples):
        mesh = square
        samples, source_facets, densities = lagrange.sampling.sample_blue_noise(
            mesh,
            num_samples,
        )
        assert len(samples) == num_samples
        assert len(source_facets) == num_samples
        assert len(densities) == num_samples
        assert np.all(source_facets < mesh.num_facets)
        assert np.all(densities > 0)


def blue_noise_sample_with_density(
    mesh: lagrange.SurfaceMesh,
    num_samples: int,
    density_id: int,
) -> bool:
    """Returns True if sampling succeeded, False if DensityError was raised."""
    density_name = mesh.get_attribute_name(density_id)
    assert density_name == "density"
    assert not mesh.is_attribute_indexed(density_id)

    try:
        samples, source_facets, densities = lagrange.sampling.sample_blue_noise(
            mesh,
            num_samples,
            density_attribute=density_name,
        )
        # If we get here, density was valid
        assert len(samples) == num_samples
        assert len(source_facets) == num_samples
        assert len(densities) == num_samples
        assert np.all(source_facets < mesh.num_facets)
        assert np.all(densities > 0)
        return True
    except lagrange.sampling.DensityError:
        # Density was invalid
        return False


# FIXME throws when num_samples=0
@pytest.mark.parametrize("num_samples", [1, 10, 100])
class TestBlueNoiseSampleDensity:
    def test_non_finite_vertex_density(self, single_triangle, num_samples):
        mesh = single_triangle
        density_values = np.array([np.nan, np.inf, 0.0])
        assert density_values.size == mesh.num_vertices
        density_id = mesh.create_attribute(
            "density",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=density_values,
        )
        success = blue_noise_sample_with_density(mesh, num_samples, density_id)
        assert not success  # Should fail due to non-finite density

    def test_all_zero_vertex_density(self, cube, num_samples):
        mesh = cube
        lagrange.triangulate_polygonal_facets(mesh)
        density_values = np.zeros(mesh.num_vertices)
        assert density_values.size == mesh.num_vertices
        density_id = mesh.create_attribute(
            "density",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=density_values,
        )
        success = blue_noise_sample_with_density(mesh, num_samples, density_id)
        assert not success  # Should fail due to zero sum

    def test_negative_facet_density(self, square, num_samples):
        mesh = square
        density_values = np.array([-1.0, 2.0])
        assert density_values.size == mesh.num_facets
        density_id = mesh.create_attribute(
            "density",
            element=lagrange.AttributeElement.Facet,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=density_values,
        )
        success = blue_noise_sample_with_density(mesh, num_samples, density_id)
        assert not success  # Should fail due to negative density

    def test_valid_facet_density(self, square, num_samples):
        mesh = square
        density_values = np.array([1.0, 2.0])
        assert density_values.size == mesh.num_facets
        density_id = mesh.create_attribute(
            "density",
            element=lagrange.AttributeElement.Facet,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=density_values,
        )
        success = blue_noise_sample_with_density(mesh, num_samples, density_id)
        assert success  # Should succeed with valid density

    def test_valid_vertex_density(self, square, num_samples):
        mesh = square
        density_values = np.array([1.0, 2.0, 3.0, 4.0])
        assert density_values.size == mesh.num_vertices
        density_id = mesh.create_attribute(
            "density",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=density_values,
        )
        success = blue_noise_sample_with_density(mesh, num_samples, density_id)
        assert success  # Should succeed with valid density
