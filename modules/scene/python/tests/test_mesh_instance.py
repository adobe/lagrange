#
# Copyright 2023 Adobe. All rights reserved.
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

import pytest
import numpy as np


class TestMeshInstance:
    def test_empty_instance(self):
        instance = lagrange.scene.MeshInstance3D()
        assert instance.mesh_index == lagrange.invalid_index
        assert np.all(instance.transform == np.identity(4))

    def test_update_instance(self):
        instance = lagrange.scene.MeshInstance3D()
        instance.mesh_index = 15
        assert instance.mesh_index == 15

        M = np.arange(16).reshape((4, 4), order="C")
        instance.transform = M
        assert np.all(instance.transform == M)
