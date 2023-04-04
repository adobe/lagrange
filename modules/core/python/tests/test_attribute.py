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

from .assets import single_triangle, single_triangle_with_index, cube
from .utils import address, assert_sharing_raw_data


class TestAttribute:
    def test_basics(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")
        assert attr.num_channels == 1
        assert attr.usage == lagrange.AttributeUsage.Scalar
        assert attr.element_type == lagrange.AttributeElement.Vertex
        assert attr.external

        data = attr.data
        assert not data.flags["OWNDATA"]
        #assert not data.flags["WRITEABLE"]
        assert len(data.shape) == 1
        assert data.shape[0] == 3

    def test_attribute_wrap(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")
        assert attr.num_channels == 1

        # Wrap should cause attribute to keep `data` alive.
        data = np.array([1, 2, 3], dtype=np.intc)
        data_ref_count = sys.getrefcount(data)
        attr.data = data
        assert attr.external
        assert sys.getrefcount(data) == data_ref_count + 1
        assert_sharing_raw_data(attr.data, data)

        # Rewrap should release attribute's hold on `data`.
        data2 = np.array([4, 5, 6], dtype=np.intc)
        data2_ref_count = sys.getrefcount(data2)
        attr.data = data2
        assert attr.external
        assert sys.getrefcount(data) == data_ref_count
        assert sys.getrefcount(data2) == data2_ref_count + 1
        assert_sharing_raw_data(attr.data, data2)

    def test_internal_copy(self, single_triangle):
        ori_data = np.array([0, 0, 0], dtype=np.intc)
        mesh = single_triangle
        id = mesh.wrap_as_attribute(
            "index",
            lagrange.AttributeElement.Vertex,
            lagrange.AttributeUsage.Scalar,
            ori_data,
        )
        attr = mesh.attribute(id)
        assert attr.external
        assert sys.getrefcount(ori_data) == 3

        # `data` is a view of the internal buffer wrapped by `attr`
        data = attr.data
        assert address(data) == address(ori_data)
        assert not data.flags["OWNDATA"]
        assert sys.getrefcount(data) == 2
        assert sys.getrefcount(ori_data) == 3

        attr.create_internal_copy()
        assert not attr.external
        assert sys.getrefcount(ori_data) == 2
        data2 = attr.data
        assert address(data) != address(data2)
        assert not data2.flags["OWNDATA"]

        # `data` is still valid because `ori_data` still exists.
        assert np.all(data == data2)

        # Wrap anohter buffer.
        data3 = np.array([4, 5, 6], dtype=np.intc)
        attr.data = data3
        assert attr.external
        assert address(attr.data) != address(data)
        assert address(attr.data) != address(data2)

        # Note that `data2` is still referring to the old internal data of
        # attribute, which has been cleared.  Thus, `data2` is invalid, and it
        # is up to the user to not use it.

    def test_delete_attribute(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")
        assert attr.num_channels == 1

        mesh.delete_attribute("vertex_index")

        # `attr` is no longer valid.
        with pytest.raises(RuntimeError) as e:
            num_channels = attr.num_channels

    def test_delete_attribute_with_wrap(self, single_triangle_with_index):
        data = np.array([-1, 1, 0], dtype=np.intc)
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")
        attr.data = data

        assert sys.getrefcount(data) == 3
        mesh.delete_attribute("vertex_index")
        assert sys.getrefcount(data) == 2

    def test_read(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")

        assert np.all(attr.data == np.array([1, 2, 3], dtype=np.intc))

    @pytest.mark.skip("Write is not yet supported by dlpack/numpy :(")
    def test_write(self, single_triangle_with_index):
        data = attr.data
        assert not data.flags["WRITEABLE"]

        data.setflags(write=True)  # This is current not supported.
        data[0] = 10

    def test_default_value(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")
        assert attr.default_value == 0
        attr.default_value = 1
        assert attr.default_value == 1

        attr.create_internal_copy()
        attr.insert_elements(1)
        assert attr.data[-1] == 1
