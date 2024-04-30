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
import lagrange

import numpy as np
import pytest
import sys

from .assets import cube


class TestIndexedAttribute:
    def test_attribute_basics(self, cube):
        mesh = cube
        id = mesh.create_attribute(
            name="test",
            element=lagrange.AttributeElement.Indexed,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.arange(8, dtype=float),
            initial_indices=np.arange(mesh.num_corners, dtype=np.intc),
        )
        assert mesh.is_attribute_indexed(id)
        attr = mesh.indexed_attribute(id)
        assert attr.usage == lagrange.AttributeUsage.Scalar
        assert attr.element_type == lagrange.AttributeElement.Indexed
        assert attr.num_channels == 1

        attr_values = attr.values
        attr_indices = attr.indices

        assert not attr_values.external
        assert attr_values.element_type == lagrange.AttributeElement.Value
        assert attr_values.num_channels == 1

        assert not attr_indices.external
        assert attr_indices.element_type == lagrange.AttributeElement.Corner
        assert attr_indices.num_channels == 1
