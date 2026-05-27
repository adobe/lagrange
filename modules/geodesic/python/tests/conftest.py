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

"""
Pytest fixtures for geodesic module tests.

Note: The `single_triangle` fixture is provided by modules/conftest.py and
is automatically available to all tests in this module.
"""

import lagrange.primitive

import pytest


@pytest.fixture
def sphere_mesh():
    """
    Generate a sphere mesh for geodesic testing.

    Returns a sphere with radius 10.0, 32 longitude sections, and 16 latitude sections.
    This resolution provides a good balance between accuracy and performance for testing
    geodesic distance calculations.
    """
    mesh = lagrange.primitive.generate_sphere(
        radius=10.0, num_longitude_sections=32, num_latitude_sections=16, triangulate=True
    )
    return mesh
