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


def make_grid(n):
    """A unit-square grid mesh in the z = 0 plane with n subdivisions per side."""
    xs = np.linspace(0, 1, n + 1)
    xv, yv = np.meshgrid(xs, xs)
    vertices = np.stack([xv.ravel(), yv.ravel(), np.zeros((n + 1) * (n + 1))], axis=1).astype(
        np.float64
    )

    def vid(i, j):
        return i * (n + 1) + j

    facets = []
    for i in range(n):
        for j in range(n):
            facets.append([vid(i, j), vid(i, j + 1), vid(i + 1, j + 1)])
            facets.append([vid(i, j), vid(i + 1, j + 1), vid(i + 1, j)])

    mesh = lagrange.SurfaceMesh()
    mesh.vertices = vertices
    mesh.facets = np.array(facets, dtype=np.uint32)
    return mesh


def set_lfs(mesh, value):
    mesh.create_attribute(
        "lfs",
        element=lagrange.AttributeElement.Vertex,
        usage=lagrange.AttributeUsage.Scalar,
        initial_values=np.full(mesh.num_vertices, value, dtype=np.float64),
    )


def nearest_distances(query, samples):
    d2 = ((query[:, None, :] - samples[None, :, :]) ** 2).sum(axis=-1)
    return np.sqrt(d2.min(axis=1))


class TestEpsilonSample:
    def test_coverage(self):
        mesh = make_grid(20)
        lfs = 0.1
        set_lfs(mesh, lfs)
        epsilon = 1.0
        radius = epsilon * lfs
        samples, facets, _ = lagrange.sampling.sample_epsilon(
            mesh, "lfs", epsilon, seed=7, ensure_vertex_coverage=True
        )
        assert len(samples) > 0
        assert len(samples) == len(facets)
        assert np.all(facets < mesh.num_facets)

        nearest = nearest_distances(mesh.vertices, samples)
        assert np.all(nearest <= radius + 1e-6)

    def test_determinism(self):
        mesh = make_grid(10)
        set_lfs(mesh, 0.1)
        a = lagrange.sampling.sample_epsilon(mesh, "lfs", 1.0, seed=42)
        b = lagrange.sampling.sample_epsilon(mesh, "lfs", 1.0, seed=42)
        assert np.array_equal(a[0], b[0])
        assert np.array_equal(a[1], b[1])

    def test_density_grading(self):
        dense = make_grid(20)
        set_lfs(dense, 0.05)
        sparse = make_grid(20)
        set_lfs(sparse, 0.15)
        dense_samples, _, _ = lagrange.sampling.sample_epsilon(dense, "lfs", 1.0, seed=5)
        sparse_samples, _, _ = lagrange.sampling.sample_epsilon(sparse, "lfs", 1.0, seed=5)
        assert len(dense_samples) > len(sparse_samples)

    def test_barycentric(self):
        mesh = make_grid(10)
        set_lfs(mesh, 0.1)
        samples, facets, bary = lagrange.sampling.sample_epsilon(
            mesh, "lfs", 1.0, seed=3, compute_barycentric=True
        )
        assert bary.shape == (len(samples), 3)
        assert np.allclose(bary.sum(axis=1), 1.0)
        assert np.all(bary >= -1e-6)

        reconstructed = np.einsum("sc,scd->sd", bary, mesh.vertices[mesh.facets[facets]])
        assert np.allclose(reconstructed, samples, atol=1e-6)

    def test_no_barycentric_by_default(self):
        mesh = make_grid(10)
        set_lfs(mesh, 0.1)
        _, _, bary = lagrange.sampling.sample_epsilon(mesh, "lfs", 1.0, seed=3)
        assert bary.shape[0] == 0

    def test_missing_attribute(self):
        mesh = make_grid(4)
        set_lfs(mesh, 0.1)
        with pytest.raises(RuntimeError):
            lagrange.sampling.sample_epsilon(mesh, "does_not_exist", 1.0)

    def test_invalid_epsilon(self):
        mesh = make_grid(4)
        set_lfs(mesh, 0.1)
        with pytest.raises(RuntimeError):
            lagrange.sampling.sample_epsilon(mesh, "lfs", 0.0)
