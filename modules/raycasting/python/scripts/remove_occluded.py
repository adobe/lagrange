#!/usr/bin/env python3

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
"""Filter a scene's facets using cached visibility and a pygfx view-error evaluator.

Run from the repository root::

    uv run --extra scripts modules/raycasting/python/scripts/remove_occluded.py D.obj

The result is always saved, to ``culled.glb`` unless ``--output`` specifies another path.
Installed wheels ship ``remove_occluded.py``; install ``adobe-lagrange[scripts]`` for its dependencies.

This script renders white, two-sided, flat-shaded geometry, not source textures/materials.
Pixel error includes all pixels, normalized by reference foreground area. Rendering settings
are fixed, but images are not expected to match pyrender or different GPU backends exactly.
World geometry, camera poses, reference images, mesh/material, and GPU buffers are reused.
Each probe updates only the active triangle indices; empty view indices request all views.
The mask follows source-mesh, instance, then local-facet order.
"""

import argparse
import logging
import math
from pathlib import Path

import lagrange
import numpy as np
import pygfx as gfx


def _fibonacci_cameras(num_views, center, radius):
    """Quasi-uniform camera positions on a sphere using a Fibonacci spiral."""
    golden = (1 + 5**0.5) / 2
    center = np.asarray(center, dtype=float)
    positions = []
    for index in range(num_views):
        theta = math.acos(1 - 2 * (index + 0.5) / num_views)
        phi = 2 * math.pi * index / golden
        offset = np.array(
            [math.sin(theta) * math.cos(phi), math.sin(theta) * math.sin(phi), math.cos(theta)]
        )
        positions.append(center + radius * offset)
    return positions


def _look_at_matrix(eye, target, up=(0, 0, 1)):
    """Camera-to-world transform looking from ``eye`` toward ``target``."""
    eye = np.asarray(eye, dtype=float)
    target = np.asarray(target, dtype=float)
    up = np.asarray(up, dtype=float)
    forward = target - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    if np.linalg.norm(right) < 1e-6:
        right = np.cross(forward, np.array([0.0, 1.0, 0.0]))
    right /= np.linalg.norm(right)
    true_up = np.cross(right, forward)
    matrix = np.eye(4)
    matrix[:3, 0] = right
    matrix[:3, 1] = true_up
    matrix[:3, 2] = -forward
    matrix[:3, 3] = eye
    return matrix


def _scene_world_geometry(scene):
    """Concatenate world-space triangles in source-mesh, instance, local-facet order."""
    all_vertices = []
    all_facets = []
    vertex_offset = 0
    for mesh_index in range(scene.num_meshes):
        mesh = scene.get_mesh(mesh_index)
        vertices = np.asarray(mesh.vertices, dtype=np.float64)
        facets = np.asarray(mesh.facets, dtype=np.int64)
        for instance_index in range(scene.num_instances(mesh_index)):
            transform = np.asarray(
                scene.get_instance(mesh_index, instance_index).transform, dtype=np.float64
            )
            all_vertices.append(vertices @ transform[:3, :3].T + transform[:3, 3])
            all_facets.append(facets + vertex_offset)
            vertex_offset += len(vertices)
    return np.concatenate(all_vertices), np.concatenate(all_facets)


class ViewErrorRenderer:
    """Persistent offscreen pygfx evaluator for white shaded scene geometry."""

    def __init__(self, scene, views=32, resolution=1024, save_dir=None, verbose=True):
        if views <= 0 or resolution <= 0:
            raise ValueError("views and resolution must be positive")
        self._renderer = self._scene = self._camera = self._mesh = self._indices = None
        self._target = None
        self._save_dir = Path(save_dir) if save_dir is not None else None
        self._verbose = verbose
        self._eval_counter = 0
        if self._save_dir is not None:
            self._save_dir.mkdir(parents=True, exist_ok=True)
        self._vertices, self._facets = _scene_world_geometry(scene)
        lower = self._vertices.min(axis=0)
        upper = self._vertices.max(axis=0)
        center = (lower + upper) / 2
        bbox_radius = float(np.linalg.norm(upper - lower) / 2)
        if bbox_radius <= 0:
            raise ValueError("scene has zero extent - nothing to render")
        camera_radius = bbox_radius * 2.5
        positions = _fibonacci_cameras(views, center, camera_radius)
        self._camera_poses = [_look_at_matrix(position, center) for position in positions]
        yfov = 2 * math.atan(bbox_radius / camera_radius) * 1.1

        try:
            self._target = gfx.Texture(dim=2, size=(resolution, resolution, 1), format="rgba8unorm")
            self._renderer = gfx.WgpuRenderer(
                self._target, pixel_scale=1, ppaa="none", enable_events=False
            )
            self._scene = gfx.Scene()
            self._scene.add(gfx.Background(None, gfx.BackgroundMaterial("#000000")))
            geometry = gfx.Geometry(
                positions=self._vertices.astype(np.float32),
                indices=self._facets.astype(np.uint32),
            )
            self._indices = geometry.indices
            self._mesh = gfx.Mesh(
                geometry,
                gfx.MeshPhongMaterial(color="white", flat_shading=True, side="both"),
            )
            self._scene.add(self._mesh)
            self._camera = gfx.PerspectiveCamera(
                math.degrees(yfov),
                1,
                depth_range=(bbox_radius * 0.01, camera_radius + bbox_radius * 2),
            )
            self._camera.add(gfx.DirectionalLight(intensity=4.0))
            self._scene.add(self._camera)
            images = self._render_views(range(views), "reference", "original")
            self._foreground_counts = [
                int((image.sum(axis=2, dtype=np.uint16) > 10).sum()) for image in images
            ]
            self._original_images = [image.astype(np.int16) for image in images]
        except BaseException:
            self.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def _render_views(self, view_indices, desc, save_label=None):
        """Render requested camera poses using the persistent mesh and buffers."""
        renderer, scene, camera = self._renderer, self._scene, self._camera
        if renderer is None or scene is None or camera is None:
            raise RuntimeError("renderer is closed")
        images = []
        view_indices = [int(view_index) for view_index in view_indices]
        for index, view_index in enumerate(view_indices):
            if self._verbose:
                print(
                    f"\r  {desc}: view {view_index} ({index + 1}/{len(view_indices)})   ",
                    end="",
                    flush=True,
                )
            camera.local.matrix = self._camera_poses[view_index]
            renderer.render(scene, camera)
            color = renderer.snapshot()[:, :, :3].copy()
            images.append(color)
            if save_label is not None:
                self._save_image(color, save_label, view_index)
        if self._verbose and view_indices:
            print()
        return images

    def _save_image(self, color, label, view_index):
        if self._save_dir is None:
            return
        from PIL import Image

        Image.fromarray(color).save(self._save_dir / f"{label}_{view_index:03d}.png")

    def evaluate(self, keep_mask, view_indices):
        """Evaluate a flattened keep mask; empty view indices request all cached views."""
        indices = self._indices
        mesh = self._mesh
        if self._renderer is None or indices is None or mesh is None:
            raise RuntimeError("renderer is closed")
        keep = np.asarray(keep_mask, dtype=bool)
        views = np.asarray(view_indices, dtype=np.int64)
        if views.size == 0:
            views = np.arange(len(self._camera_poses))
        kept_facets = self._facets[keep]
        count = len(kept_facets)
        if count:
            indices.data[:count] = kept_facets
            indices.update_range(0, count)
        indices.draw_range = (0, count)
        mesh.visible = count > 0
        label = f"eval{self._eval_counter:04d}" if self._save_dir is not None else None
        description = f"eval {self._eval_counter}"
        self._eval_counter += 1
        rendered = self._render_views(views, description, label)
        errors = []
        for view_index, image in zip(views, rendered):
            foreground = self._foreground_counts[view_index]
            if foreground == 0:
                errors.append(0.0)
                continue
            difference = self._original_images[view_index] - image.astype(np.int16)
            squared_error = np.einsum("ijk,ijk->", difference, difference, dtype=np.int64)
            errors.append(float(np.sqrt(squared_error / (foreground * 3.0)) / 255.0))
        return errors

    def close(self):
        """Drop owned resources; pygfx's process-wide shared device is not destroyed."""
        if self._scene is not None:
            self._scene.clear()
        self._renderer = self._scene = self._camera = self._mesh = self._indices = None
        self._target = None


def parse_args():
    def tradeoff(value):
        return None if value.lower() == "none" else float(value)

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0] if __doc__ else None)
    parser.add_argument("input", type=Path, help="Scene file to load, such as .glb or .obj.")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("culled.glb"),
        help="Where to save the culled scene (default: culled.glb).",
    )
    parser.add_argument(
        "--max-view-error", type=float, default=0.01, help="Worst-view error budget."
    )
    parser.add_argument("--views", type=int, default=12, help="Number of camera views.")
    parser.add_argument("--resolution", type=int, default=1024, help="Render resolution in pixels.")
    parser.add_argument(
        "--num-rays", type=int, default=1_600_000_000, help="Visibility ray budget."
    )
    parser.add_argument("--num-threshold-iterations", type=int, default=20)
    parser.add_argument("--num-refinement-iterations", type=int, default=10)
    parser.add_argument(
        "--smoothness-tradeoff", type=tradeoff, default=0.5, help="[0, 1] or 'none'."
    )
    parser.add_argument("--save-dir", type=Path, help="Write every rendered view here as PNGs.")
    parser.add_argument("--quiet", action="store_true", help="Suppress per-view render progress.")
    args = parser.parse_args()

    for name in (
        "views",
        "resolution",
        "num_rays",
        "num_threshold_iterations",
        "num_refinement_iterations",
    ):
        if getattr(args, name) <= 0:
            parser.error(f"--{name.replace('_', '-')} must be positive")
    if not math.isfinite(args.max_view_error) or args.max_view_error < 0:
        parser.error("--max-view-error must be finite and non-negative")
    if args.smoothness_tradeoff is not None and (
        not math.isfinite(args.smoothness_tradeoff) or not 0 <= args.smoothness_tradeoff <= 1
    ):
        parser.error("--smoothness-tradeoff must be 'none' or a finite value in [0, 1]")

    if args.output.is_dir():
        parser.error(f"--output must be a filename, not a directory: {args.output}")
    extension = args.output.suffix.lower()
    if extension not in (".obj", ".gltf", ".glb"):
        parser.error(
            f"--output must end in .obj, .gltf, or .glb (got '{extension or 'no extension'}')"
        )
    if args.save_dir is not None and args.save_dir.is_file():
        parser.error(f"--save-dir must be a directory, not a file: {args.save_dir}")

    return args


def main():
    """CLI: filter cached visibility with a view-error budget and save the result."""

    args = parse_args()

    logging.basicConfig(level=logging.INFO)
    scene = lagrange.io.load_simple_scene(args.input, triangulate=True)
    with ViewErrorRenderer(
        scene,
        views=args.views,
        resolution=args.resolution,
        save_dir=args.save_dir,
        verbose=not args.quiet,
    ) as renderer:
        facet_filter = lagrange.raycasting.OccludedFacetsFilter(
            scene,
            num_rays=args.num_rays,
            batch_size=min(args.num_rays, 50_000_000),
            adaptive=False,
        )
        culled, parameters = facet_filter.apply_with_view_error(
            renderer.evaluate,
            max_view_error=args.max_view_error,
            num_views=args.views,
            num_threshold_iterations=args.num_threshold_iterations,
            num_refinement_iterations=args.num_refinement_iterations,
            smoothness_tradeoff=args.smoothness_tradeoff,
        )

    print(
        f"within_error_budget={parameters.within_error_budget} "
        f"worst_view_error={parameters.worst_view_error:.6f} threshold={parameters.threshold:.6f} "
        f"graphcut_smoothness={parameters.graphcut_smoothness:.6g} "
        f"removed_fraction={parameters.removed_fraction:.4f} "
        f"num_isolated_facets={parameters.num_isolated_facets}"
    )
    lagrange.io.save_simple_scene(args.output, culled)
    print(f"Saved culled scene to {args.output}")


if __name__ == "__main__":
    main()
