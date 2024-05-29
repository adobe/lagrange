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

from .assets import cube, cube_with_uv


class TestFilterAttributes:
    def test_included(self, cube_with_uv):
        mesh = cube_with_uv

        mesh2 = lagrange.filter_attributes(mesh, included_attributes=["uv"])
        assert mesh2.has_attribute("uv")

        uv_id = mesh.get_attribute_id("uv")
        mesh2 = lagrange.filter_attributes(mesh, included_attributes=[uv_id])
        assert mesh2.has_attribute("uv")

    def test_excluded(self, cube_with_uv):
        mesh = cube_with_uv
        mesh2 = lagrange.filter_attributes(mesh, excluded_attributes=["uv"])
        assert not mesh2.has_attribute("uv")

    def test_usage(self, cube_with_uv):
        mesh = cube_with_uv

        mesh2 = lagrange.filter_attributes(mesh, included_usages=[lagrange.AttributeUsage.UV])
        assert mesh2.has_attribute("uv")

        mesh2 = lagrange.filter_attributes(mesh, included_usages=[])
        assert not mesh2.has_attribute("uv")

    def test_element_type(self, cube_with_uv):
        mesh = cube_with_uv

        mesh2 = lagrange.filter_attributes(
            mesh, included_element_types=[lagrange.AttributeElement.Indexed]
        )
        assert mesh2.has_attribute("uv")

        mesh2 = lagrange.filter_attributes(mesh, included_element_types=[])
        assert not mesh2.has_attribute("uv")
