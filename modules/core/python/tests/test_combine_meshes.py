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
import copy

import lagrange
import numpy as np
import pytest


class TestCombineMeshes:
    def test_empty(self):
        mesh = lagrange.combine_meshes([], True)
        assert mesh.num_vertices == 0
        assert mesh.num_facets == 0

    def test_single_mesh(self, cube):
        mesh = cube
        out = lagrange.combine_meshes([cube], True)
        assert np.all(out.vertices == mesh.vertices)
        assert np.all(out.facets == mesh.facets)

    def test_two_meshes(self, cube):
        mesh1 = cube
        mesh2 = lagrange.SurfaceMesh()
        mesh2.vertices = mesh1.vertices + 10
        facets = mesh1.facets
        mesh2.facets = np.copy(facets)  # A copy is needed because facets is not writable.

        out = lagrange.combine_meshes([mesh1, mesh2], True)
        assert np.all(out.vertices[:8] == mesh1.vertices)
        assert np.all(out.vertices[8:] == mesh2.vertices)
        assert np.all(out.facets[:6] == mesh1.facets)
        assert np.all(out.facets[6:] == mesh2.facets + mesh1.num_vertices)

    @pytest.mark.parametrize(
        "policy",
        [
            lagrange.ValueAttributePolicy.RequireEqual,
            lagrange.ValueAttributePolicy.ConcatenateIndices,
        ],
    )
    def test_initialized_empty_edges(self, policy):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))
        mesh.initialize_edges()
        mesh.create_attribute(
            "edge_value",
            element=lagrange.AttributeElement.Edge,
            usage=lagrange.AttributeUsage.Scalar,
            dtype=float,
            num_channels=1,
        )
        mesh.create_attribute(
            "edge_refs",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.EdgeIndex,
            dtype=np.uint32,
            num_channels=1,
        )
        combined = lagrange.combine_meshes([mesh, mesh], value_attribute_policy=policy)
        assert combined.num_vertices == 6
        assert combined.num_facets == 0
        assert combined.has_edges
        assert combined.num_edges == 0
        assert combined.attribute("edge_value").num_elements == 0
        assert combined.attribute("edge_refs").num_elements == 0
        assert mesh.num_vertices == 3
        assert mesh.has_edges
        assert mesh.num_edges == 0

    def test_two_meshes_with_attribute(self, cube):
        mesh = cube
        opt = lagrange.NormalOptions()
        lagrange.compute_normal(mesh)

        out = lagrange.combine_meshes([mesh, mesh], True)
        assert out.has_attribute(opt.output_attribute_name)
        assert out.is_attribute_indexed(opt.output_attribute_name)

    @pytest.mark.parametrize(
        "policy",
        [
            lagrange.ValueAttributePolicy.RequireEqual,
            lagrange.ValueAttributePolicy.ConcatenateIndices,
        ],
    )
    @pytest.mark.parametrize("second", ["Cube", "Ball"])
    def test_metadata(self, cube, caplog, policy, second):
        other = copy.copy(cube)
        cube.metadata["name"] = "Cube"
        other.metadata["name"] = second
        combined = lagrange.combine_meshes([cube, other], value_attribute_policy=policy)
        assert combined.has_attribute("name") == (second == "Cube")
        if second == "Cube":
            assert combined.metadata["name"] == "Cube"
        else:
            assert "Value attribute 'name' differs" in caplog.text
            assert any(record.levelname == "ERROR" for record in caplog.records)

    @pytest.mark.parametrize("preserve_attributes", [False, True])
    def test_all_empty_inputs(self, preserve_attributes):
        meshes = [lagrange.SurfaceMesh(), lagrange.SurfaceMesh()]
        combined = lagrange.combine_meshes(meshes, preserve_attributes=preserve_attributes)
        assert combined.num_vertices == 0
        assert combined.num_facets == 0

    @pytest.mark.parametrize("call_style", ["default", "legacy", "kwargs"])
    def test_default_policy(self, cube, call_style):
        assert not hasattr(lagrange, "CombineMeshesOptions")
        assert not hasattr(lagrange.ValueAttributePolicy, "Concatenate")
        cube.metadata["name"] = "Cube"
        cube.create_attribute(
            "refs",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.VertexIndex,
            initial_values=np.array([0], dtype=np.uint32),
        )
        if call_style == "default":
            combined = lagrange.combine_meshes([cube, cube])
        elif call_style == "legacy":
            combined = lagrange.combine_meshes([cube, cube], True)
        else:
            combined = lagrange.combine_meshes([cube, cube], preserve_attributes=True)
        assert np.array_equal(combined.attribute("refs").data.ravel(), [0, cube.num_vertices])
        assert combined.metadata["name"] == "Cube"

    @pytest.mark.parametrize(
        "policy",
        [
            lagrange.ValueAttributePolicy.RequireEqual,
            lagrange.ValueAttributePolicy.ConcatenateIndices,
        ],
    )
    @pytest.mark.parametrize(
        "usage",
        [
            lagrange.AttributeUsage.Scalar,
            lagrange.AttributeUsage.VertexIndex,
            lagrange.AttributeUsage.EdgeIndex,
        ],
    )
    def test_value_attribute_policies(self, cube, policy, usage):
        cube.initialize_edges()
        cube.create_attribute(
            "refs",
            element=lagrange.AttributeElement.Value,
            usage=usage,
            initial_values=np.array([0], dtype=np.uint32),
        )
        combined = lagrange.combine_meshes([cube, cube], value_attribute_policy=policy)
        expected = [0]
        if policy == lagrange.ValueAttributePolicy.ConcatenateIndices:
            if usage == lagrange.AttributeUsage.VertexIndex:
                expected.append(cube.num_vertices)
            elif usage == lagrange.AttributeUsage.EdgeIndex:
                expected.append(cube.num_edges)
        assert np.array_equal(combined.attribute("refs").data.ravel(), expected)

    @pytest.mark.parametrize("invalid_first", [False, True])
    @pytest.mark.parametrize("empty_mesh", [False, True])
    def test_uninitialized_edge_indices_raise(self, cube, invalid_first, empty_mesh):
        cube.initialize_edges()
        invalid = lagrange.SurfaceMesh()
        if not empty_mesh:
            invalid.add_vertices(np.eye(3))
            invalid.add_triangle(0, 1, 2)
        invalid.create_attribute(
            "unshared_refs",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.EdgeIndex,
            initial_values=np.array([] if empty_mesh else [0], dtype=np.uint32),
            num_channels=1,
        )
        meshes = [invalid, cube] if invalid_first else [cube, invalid]
        with pytest.raises(RuntimeError, match="EdgeIndex.*not initialized"):
            lagrange.combine_meshes(meshes)
        assert invalid.num_edges == 0
        assert cube.num_edges > 0

    @pytest.mark.parametrize("uninitialized_first", [False, True])
    def test_drop_edge_attributes_without_connectivity(self, uninitialized_first):
        missing = lagrange.SurfaceMesh()
        missing.add_vertices(np.eye(3))
        missing.add_triangle(0, 1, 2)
        missing.metadata["name"] = "Triangle"
        ready = copy.copy(missing)
        ready.initialize_edges()
        for mesh in [missing, ready]:
            mesh.create_attribute(
                "edge_data",
                element=lagrange.AttributeElement.Edge,
                usage=lagrange.AttributeUsage.Scalar,
                initial_values=np.arange(mesh.num_edges, dtype=np.float64),
                num_channels=1,
            )
        ready.create_attribute(
            "valid_refs",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.EdgeIndex,
            initial_values=np.array([0], dtype=np.uint32),
        )
        meshes = [missing, ready] if uninitialized_first else [ready, missing]
        combined = lagrange.combine_meshes(meshes)
        assert not combined.has_attribute("edge_data")
        assert not combined.has_attribute("valid_refs")
        assert combined.num_edges == 0
        assert combined.num_vertices == 6
        assert combined.num_facets == 2
        assert combined.metadata["name"] == "Triangle"
        assert missing.num_edges == 0
        assert ready.num_edges == 3

    def test_kwargs_without_attributes(self, cube, caplog):
        other = copy.copy(cube)
        cube.metadata["name"] = "Cube"
        other.metadata["name"] = "Other"
        combined = lagrange.combine_meshes(
            [cube, other],
            preserve_attributes=False,
            value_attribute_policy=lagrange.ValueAttributePolicy.ConcatenateIndices,
        )
        assert not combined.has_attribute("name")
        assert "differs" not in caplog.text
