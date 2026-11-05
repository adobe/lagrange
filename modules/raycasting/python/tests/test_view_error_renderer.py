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
"""CPU renderer-state and CLI tests; only the GPU renderer is mocked."""

import importlib.util
import sys
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import lagrange
import numpy as np
import pytest

pytest.importorskip("pygfx")
wgpu = pytest.importorskip("wgpu")
spec = importlib.util.spec_from_file_location(
    "view_error_script", Path(__file__).parents[1] / "scripts" / "remove_occluded.py"
)
assert spec is not None and spec.loader is not None
renderer_script = importlib.util.module_from_spec(spec)
spec.loader.exec_module(renderer_script)


@pytest.fixture
def geometry():
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = np.array(
        [[0, 0, 0], [1, 0, 0], [0, 1, 0], [3, 0, 0], [4, 0, 0], [3, 1, 0], [9, 9, 9]],
        dtype=np.float64,
    )
    mesh.facets = np.array([[0, 1, 2], [3, 4, 5]], dtype=np.uint32)
    triangle = lagrange.SurfaceMesh()
    triangle.vertices = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float64)
    triangle.facets = np.array([[0, 1, 2]], dtype=np.uint32)
    scene = lagrange.scene.SimpleScene3D()
    scene.add_mesh(mesh)
    scene.add_mesh(triangle)
    for mi, x in [(0, 0), (0, 10), (1, 20)]:
        instance = lagrange.scene.MeshInstance3D()
        instance.mesh_index = mi
        transform = np.eye(4)
        transform[0, 3] = x
        instance.transform = transform
        scene.add_instance(instance)
    return scene


@pytest.fixture
def gpu(monkeypatch):
    state = SimpleNamespace(instances=[], calls=[], scene=None, fail=False, image=None)

    class Renderer:
        def __init__(self, target, **kwargs):
            state.instances.append(self)

        def render(self, scene, camera):
            state.scene = scene
            meshes = [
                obj
                for obj in scene.children
                if isinstance(obj, renderer_script.gfx.Mesh) and obj.visible
            ]
            faces = sum(mesh.geometry.indices.draw_range[1] for mesh in meshes)
            state.calls.append(camera.local.matrix.copy())
            if state.fail:
                raise RuntimeError("render failed")
            self.image = np.zeros((2, 2, 4), dtype=np.uint8)
            self.image[:, :, :3] = 40 + faces * 10 if faces else 0
            self.image[:, :, 3] = 255

        def snapshot(self):
            return (state.image if state.image is not None else self.image).copy()

    def unexpected_gpu(*args, **kwargs):
        pytest.fail("unit tests must not request a GPU adapter")

    monkeypatch.setattr(wgpu.gpu, "request_adapter_sync", unexpected_gpu)
    monkeypatch.setattr(renderer_script.gfx, "WgpuRenderer", Renderer)
    return state


def test_world_geometry_follows_mesh_instance_facet_order(geometry):
    vertices, faces = renderer_script._scene_world_geometry(geometry)
    np.testing.assert_array_equal(
        faces, [[0, 1, 2], [3, 4, 5], [7, 8, 9], [10, 11, 12], [14, 15, 16]]
    )
    np.testing.assert_array_equal(vertices[7:14], vertices[:7] + [10, 0, 0])
    np.testing.assert_array_equal(vertices[14:17], vertices[:3] + [20, 0, 0])


def test_masks_reuse_buffers_and_restore_geometry(geometry, gpu):
    with renderer_script.ViewErrorRenderer(
        geometry, views=3, resolution=2, verbose=False
    ) as renderer:
        scene = renderer._scene
        mesh, indices, positions = (
            renderer._mesh,
            renderer._indices,
            renderer._mesh.geometry.positions,
        )
        canonical = renderer._facets.copy()
        assert not np.shares_memory(renderer._facets, indices.data)
        with patch.object(indices, "update_range", wraps=indices.update_range) as update:
            errors = renderer.evaluate(
                np.array([0, 1, 0, 0, 1], dtype=np.uint8), np.array([2, 0], dtype=np.uint64)
            )
            assert len(errors) == 2 and min(errors) > 0
            np.testing.assert_array_equal(indices.data[:2], canonical[[1, 4]])
            assert indices.draw_range == (0, 2)
            update.assert_called_once_with(0, 2)
            for actual, view in zip(gpu.calls[-2:], [2, 0]):
                np.testing.assert_allclose(
                    actual, renderer._camera_poses[view], rtol=1e-12, atol=1e-12
                )
            empty = renderer.evaluate(np.zeros(5, dtype=np.uint8), np.array([], dtype=np.uint64))
            assert empty == pytest.approx([90 / 255] * 3)
            assert indices.draw_range == (0, 0) and not mesh.visible
            assert renderer.evaluate(np.ones(5, dtype=np.uint8), np.array([], dtype=np.uint64)) == [
                0,
                0,
                0,
            ]
            assert indices.draw_range == (0, 5) and mesh.visible
            update.assert_called_with(0, 5)
        np.testing.assert_array_equal(indices.data, canonical)
        np.testing.assert_array_equal(renderer._facets, canonical)
        assert renderer._mesh is mesh and renderer._indices is indices
        assert mesh.geometry.positions is positions
    assert not scene.children
    with pytest.raises(RuntimeError, match="closed"):
        renderer.evaluate(np.ones(5, dtype=np.uint8), np.array([], dtype=np.uint64))
    with pytest.raises(RuntimeError, match="closed"):
        renderer._render_views([0], "closed")


def test_pixel_error_counts_background_differences_but_not_alpha(geometry, gpu):
    with renderer_script.ViewErrorRenderer(
        geometry, views=1, resolution=2, verbose=False
    ) as renderer:
        reference = np.array(
            [[[100, 50, 25], [0, 0, 0]], [[0, 0, 0], [255, 255, 255]]], dtype=np.int16
        )
        renderer._original_images = [reference]
        renderer._foreground_counts = [2]
        gpu.image = np.zeros((2, 2, 4), dtype=np.uint8)
        gpu.image[:, :, :3] = reference
        gpu.image[0, 1, :3] = [255, 0, 0]
        errors = renderer.evaluate(np.ones(5, dtype=np.uint8), np.array([0], dtype=np.uint64))
        assert errors == pytest.approx([np.sqrt(255**2 / 6) / 255])


def test_render_failure_closes_owned_scene(geometry, gpu):
    with pytest.raises(RuntimeError, match="render failed"):
        with renderer_script.ViewErrorRenderer(
            geometry, views=1, resolution=2, verbose=False
        ) as renderer:
            scene = renderer._scene
            gpu.fail = True
            renderer.evaluate(np.ones(5, dtype=np.uint8), np.array([], dtype=np.uint64))
    assert not scene.children
    assert renderer._renderer is None
    renderer.close()


def test_reference_failure_releases_owned_scene(geometry, gpu):
    gpu.fail = True
    with pytest.raises(RuntimeError, match="render failed"):
        renderer_script.ViewErrorRenderer(geometry, views=1, resolution=2, verbose=False)
    assert not gpu.scene.children


@pytest.mark.parametrize("views,resolution", [(0, 2), (1, 0)])
def test_invalid_dimensions_do_not_construct_renderer(geometry, gpu, views, resolution):
    with pytest.raises(ValueError, match="positive"):
        renderer_script.ViewErrorRenderer(geometry, views=views, resolution=resolution)
    assert not gpu.instances


@pytest.mark.parametrize(
    "option,value",
    [
        (f"--{name}", "0")
        for name in (
            "views",
            "resolution",
            "num-rays",
            "num-threshold-iterations",
            "num-refinement-iterations",
        )
    ]
    + [("--max-view-error", value) for value in ("-1", "nan")]
    + [("--smoothness-tradeoff", value) for value in ("-0.1", "1.1", "nan")],
)
def test_parser_rejects_invalid_controls(monkeypatch, option, value):
    monkeypatch.setattr(sys, "argv", ["remove_occluded.py", "unused.obj", f"{option}={value}"])
    with pytest.raises(SystemExit) as exc:
        renderer_script.parse_args()
    assert exc.value.code == 2


@pytest.mark.parametrize("output", [None, "nested/custom.OBJ"])
def test_parser_returns_paths_and_default_output(monkeypatch, output):
    argv = ["remove_occluded.py", "input.obj", "--save-dir", "renders"]
    if output is not None:
        argv.extend(["--output", output])
    monkeypatch.setattr(sys, "argv", argv)
    args = renderer_script.parse_args()
    assert args.input == Path("input.obj")
    assert args.output == (Path(output) if output is not None else Path("culled.glb"))
    assert args.save_dir == Path("renders")


def test_parser_accepts_missing_docstring(monkeypatch):
    monkeypatch.setattr(renderer_script, "__doc__", None)
    monkeypatch.setattr(sys, "argv", ["remove_occluded.py", "input.obj"])
    assert renderer_script.parse_args().input == Path("input.obj")


@pytest.mark.parametrize("kind", ["output-directory", "output-extension", "save-file"])
def test_parser_rejects_invalid_paths(monkeypatch, tmp_path, kind):
    target = tmp_path / "output.txt"
    option = "--output"
    if kind == "output-directory":
        target.mkdir()
    elif kind == "save-file":
        target.write_text("not a directory")
        option = "--save-dir"
    monkeypatch.setattr(sys, "argv", ["remove_occluded.py", "unused.obj", option, str(target)])
    with pytest.raises(SystemExit) as exc:
        renderer_script.parse_args()
    assert exc.value.code == 2


def test_png_output_creates_parent_directories(geometry, gpu, tmp_path):
    pytest.importorskip("PIL.Image")
    directory = tmp_path / "nested" / "renders"
    with renderer_script.ViewErrorRenderer(
        geometry, views=1, resolution=2, save_dir=directory, verbose=False
    ) as renderer:
        renderer.evaluate(np.ones(5, dtype=np.uint8), np.array([], dtype=np.uint64))
    assert (directory / "original_000.png").is_file()
    assert (directory / "eval0000_000.png").is_file()


@pytest.mark.parametrize(
    "tradeoff,output,expected",
    [("none", None, None), ("0", "custom.obj", 0.0)],
    ids=["default-output-disabled-smoothing", "explicit-output-zero-tradeoff"],
)
def test_cli_forwards_search_controls_and_always_saves(monkeypatch, tradeoff, output, expected):
    events = []
    saved = []
    searches = []
    source = object()

    class Renderer:
        def __init__(self, scene, **kwargs):
            assert scene is source
            events.append("renderer")

        def __enter__(self):
            return self

        def __exit__(self, *args):
            events.append("closed")

        def evaluate(self, keep, views):
            return [0.0]

    class Filter:
        def __init__(self, scene, **kwargs):
            assert events == ["renderer"] and scene is source
            assert kwargs == {"num_rays": 1000, "batch_size": 1000, "adaptive": False}

        def apply_with_view_error(self, evaluate, **kwargs):
            searches.append(kwargs)
            return source, SimpleNamespace(
                within_error_budget=True,
                worst_view_error=0,
                threshold=0,
                graphcut_smoothness=0,
                removed_fraction=0,
                num_isolated_facets=0,
            )

    monkeypatch.setattr(renderer_script, "ViewErrorRenderer", Renderer)
    monkeypatch.setattr(lagrange.raycasting, "OccludedFacetsFilter", Filter)
    monkeypatch.setattr(lagrange.io, "load_simple_scene", lambda *args, **kwargs: source)
    monkeypatch.setattr(
        lagrange.io, "save_simple_scene", lambda path, scene: saved.append((path, scene))
    )
    argv = [
        "remove_occluded.py",
        "input.obj",
        "--num-rays",
        "1000",
        "--smoothness-tradeoff",
        tradeoff,
        "--quiet",
    ]
    if output is not None:
        argv.extend(["--output", output])
    monkeypatch.setattr(sys, "argv", argv)
    renderer_script.main()
    assert searches[0]["smoothness_tradeoff"] == expected
    assert saved == [(Path(output) if output is not None else Path("culled.glb"), source)]
    assert events == ["renderer", "closed"]


def test_cli_rejects_invalid_controls_before_loading(monkeypatch, gpu):
    def fail_late(*args, **kwargs):
        pytest.fail("invalid controls must fail before scene loading")

    monkeypatch.setattr(lagrange.io, "load_simple_scene", fail_late)
    monkeypatch.setattr(
        sys, "argv", ["remove_occluded.py", "unused.obj", "--num-refinement-iterations", "0"]
    )
    with pytest.raises(SystemExit):
        renderer_script.main()
    assert not gpu.instances


def test_cli_help_does_not_construct_renderer(monkeypatch, gpu):
    monkeypatch.setattr(sys, "argv", ["remove_occluded.py", "--help"])
    with pytest.raises(SystemExit) as exc:
        renderer_script.main()
    assert exc.value.code == 0 and not gpu.instances
