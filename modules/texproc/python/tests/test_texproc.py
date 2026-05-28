#
# Copyright 2025 Adobe. All rights reserved.
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


class TestTextureProcessing:
    def test_filtering(self, quad_mesh, quad_tex):
        assert quad_mesh.has_attribute("uv")
        assert quad_mesh.num_vertices == 4
        assert quad_mesh.num_facets == 2
        assert quad_tex.dtype == np.float32

        quad_tex_ = lagrange.texproc.texture_filtering(
            quad_mesh,
            quad_tex,
            stiffness_regularization_weight=1e-5,
        )

        assert quad_tex_.shape == quad_tex.shape

    def test_stitching_quad(self, quad_mesh, quad_tex):
        assert quad_mesh.has_attribute("uv")
        assert quad_mesh.num_vertices == 4
        assert quad_mesh.num_facets == 2
        assert quad_tex.dtype == np.float32

        quad_tex_ = lagrange.texproc.texture_stitching(quad_mesh, quad_tex)

        assert quad_tex_.shape == quad_tex.shape

    def test_stitching_cube(self, cube_with_uv, quad_tex):
        assert cube_with_uv.has_attribute("uv")
        assert cube_with_uv.num_vertices == 8
        assert cube_with_uv.num_facets == 12
        assert quad_tex.dtype == np.float32

        quad_tex_ = lagrange.texproc.texture_stitching(
            cube_with_uv,
            quad_tex,
            stiffness_regularization_weight=1e-5,
        )

        assert quad_tex_.shape == quad_tex.shape

    def test_geodesic_dilation(self, quad_mesh, quad_tex):
        assert quad_mesh.has_attribute("uv")
        assert quad_mesh.num_vertices == 4
        assert quad_mesh.num_facets == 2
        assert quad_tex.dtype == np.float32

        quad_tex[32:48, 24:32, :] = 1.0
        quad_tex_ = lagrange.texproc.geodesic_dilation(quad_mesh, quad_tex)

        assert quad_tex_.shape == quad_tex.shape

    def test_geodesic_position(self, quad_mesh):
        assert quad_mesh.has_attribute("uv")
        assert quad_mesh.num_vertices == 4
        assert quad_mesh.num_facets == 2

        pos_map = lagrange.texproc.geodesic_position(quad_mesh, 128, 128)

        assert pos_map.shape == (128, 128, 3)

    def test_rasterize_and_compose(self, quad_scene, quad_tex):
        mesh = lagrange.scene.scene_to_mesh(quad_scene)

        assert mesh.has_attribute("uv")
        assert mesh.num_vertices == 4
        assert mesh.num_facets == 2
        assert quad_tex.dtype == np.float32

        views = []
        num_cameras = 8
        for _ in range(num_cameras):
            views.append(quad_tex.copy())

        colors, weights = lagrange.texproc.rasterize_textures_from_renders(
            quad_scene,
            views,
            width=128,
            height=128,
            base_confidence=0,
        )

        assert len(colors) == num_cameras
        assert len(weights) == num_cameras
        for color, weight in zip(colors, weights):
            assert color.shape == (128, 128, 4)
            assert weight.shape == (128, 128, 1)

        final_color = lagrange.texproc.texture_compositing(
            mesh,
            colors,
            weights,
            clamp_to_range=(0.0, 1.0),
        )

        assert final_color.shape == (128, 128, 4)

    def test_camera_transforms_property(self):
        ct = lagrange.CameraTransforms()

        # 4x4 setter: full matrix with last row [0, 0, 0, 1]
        view = np.eye(4, dtype=np.float32)
        view[0, 3] = 1.5
        view[1, 3] = -2.5
        ct.view = view
        assert np.array_equal(ct.view, view)

        # 3x4 setter: compact [R|t] form; getter returns 4x4 with last row [0,0,0,1] appended
        view34 = view[:3, :]  # shape (3, 4)
        ct.view = view34
        expected = np.eye(4, dtype=np.float32)
        expected[:3, :] = view34
        assert np.allclose(ct.view, expected)

        proj = np.arange(16, dtype=np.float32).reshape(4, 4)
        ct.projection = proj
        assert np.array_equal(ct.projection, proj)

    def test_rasterize_with_mesh_and_cameras(self, quad_scene, quad_tex):
        mesh = lagrange.scene.scene_to_mesh(quad_scene)
        cameras = lagrange.scene.camera_transforms_from_scene(quad_scene)
        assert len(cameras) == 8
        for cam in cameras:
            assert cam.view.shape == (4, 4)
            assert cam.projection.shape == (4, 4)

        views = [quad_tex.copy() for _ in range(len(cameras))]

        colors, weights = lagrange.texproc.rasterize_textures_from_renders(
            mesh,
            cameras,
            views,
            width=128,
            height=128,
            base_confidence=0,
        )

        assert len(colors) == len(cameras)
        assert len(weights) == len(cameras)
        for color, weight in zip(colors, weights):
            assert color.shape == (128, 128, 4)
            assert weight.shape == (128, 128, 1)
