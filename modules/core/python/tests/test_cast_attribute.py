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
from .assets import single_triangle


class TestCastAttribute:
    def test_simple(self, single_triangle):
        mesh = single_triangle
        attr_id = mesh.create_attribute(
            "test",
            element=lagrange.AttributeElement.Value,
            initial_values=np.eye(3, dtype=np.float32),
        )
        assert mesh.attribute("test").dtype == np.float32

        # Cast to double
        attr_id = lagrange.cast_attribute(mesh, "test", np.float64)
        assert mesh.get_attribute_name(attr_id) == "test"
        assert mesh.attribute("test").dtype == np.float64

        # Cast to uint8
        attr_id = lagrange.cast_attribute(mesh, "test", np.uint8)
        assert mesh.get_attribute_name(attr_id) == "test"
        assert mesh.attribute("test").dtype == np.uint8

        # Cast to another attribute with type int32
        attr_id = lagrange.cast_attribute(
            mesh, "test", np.int32, output_attribute_name="test2"
        )
        assert mesh.get_attribute_name(attr_id) == "test2"
        assert mesh.attribute("test2").dtype == np.int32
        assert mesh.attribute("test").dtype == np.uint8  # Same as before

        # Cast to python float
        attr_id = lagrange.cast_attribute(mesh, "test", float)
        assert mesh.get_attribute_name(attr_id) == "test"
        assert mesh.attribute("test").dtype == np.float64

        # Cast to python int
        attr_id = lagrange.cast_attribute(mesh, "test", int)
        assert mesh.get_attribute_name(attr_id) == "test"
        assert mesh.attribute("test").dtype == np.int64
