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


class TestGetUniqueAttributeName:
    def test_unused_name_returned_unchanged(self, single_triangle):
        mesh = single_triangle
        result = lagrange.get_unique_attribute_name(mesh, "color")
        assert result == "color"

    def test_existing_name_gets_suffix(self, single_triangle):
        mesh = single_triangle
        mesh.create_attribute(
            "color",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.zeros(mesh.num_vertices, dtype=np.float64),
        )
        result = lagrange.get_unique_attribute_name(mesh, "color")
        assert result == "color.0"

    def test_multiple_existing_names_get_incremented_suffix(self, single_triangle):
        mesh = single_triangle
        for suffix in ["color", "color.0", "color.1"]:
            mesh.create_attribute(
                suffix,
                element=lagrange.AttributeElement.Vertex,
                usage=lagrange.AttributeUsage.Scalar,
                initial_values=np.zeros(mesh.num_vertices, dtype=np.float64),
            )
        result = lagrange.get_unique_attribute_name(mesh, "color")
        assert result == "color.2"

    def test_raises_after_exhausting_attempts(self, single_triangle):
        mesh = single_triangle
        # Create 'color' and 'color.0' through 'color.999' (1001 attributes total).
        names = ["color"] + [f"color.{i}" for i in range(1000)]
        for name in names:
            mesh.create_attribute(
                name,
                element=lagrange.AttributeElement.Vertex,
                usage=lagrange.AttributeUsage.Scalar,
                initial_values=np.zeros(mesh.num_vertices, dtype=np.float64),
            )
        with pytest.raises(Exception):
            lagrange.get_unique_attribute_name(mesh, "color")

    def test_custom_separator(self, single_triangle):
        mesh = single_triangle
        mesh.create_attribute(
            "color",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.zeros(mesh.num_vertices, dtype=np.float64),
        )
        result = lagrange.get_unique_attribute_name(mesh, "color", separator="_")
        assert result == "color_0"

    def test_custom_postfix(self, single_triangle):
        mesh = single_triangle
        mesh.create_attribute(
            "color",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.zeros(mesh.num_vertices, dtype=np.float64),
        )
        result = lagrange.get_unique_attribute_name(mesh, "color", postfix=".bak")
        assert result == "color.0.bak"

    def test_custom_separator_and_postfix(self, single_triangle):
        mesh = single_triangle
        mesh.create_attribute(
            "color",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.zeros(mesh.num_vertices, dtype=np.float64),
        )
        result = lagrange.get_unique_attribute_name(mesh, "color", separator="_", postfix=".tmp")
        assert result == "color_0.tmp"

    def test_custom_max_increment(self, single_triangle):
        mesh = single_triangle
        # Create 'color' and 'color.0' through 'color.9' (11 attributes total).
        names = ["color"] + [f"color.{i}" for i in range(10)]
        for name in names:
            mesh.create_attribute(
                name,
                element=lagrange.AttributeElement.Vertex,
                usage=lagrange.AttributeUsage.Scalar,
                initial_values=np.zeros(mesh.num_vertices, dtype=np.float64),
            )
        # Should raise with max_increment=10 since we need to try up to 10
        with pytest.raises(Exception):
            lagrange.get_unique_attribute_name(mesh, "color", max_increment=10)

    def test_disable_warning(self, single_triangle):
        mesh = single_triangle
        mesh.create_attribute(
            "color",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.zeros(mesh.num_vertices, dtype=np.float64),
        )
        # This should not emit a warning (testing mainly for no crash)
        result = lagrange.get_unique_attribute_name(mesh, "color", emit_warning=False)
        assert result == "color.0"
