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
"""Benchmark and plot texture-compositing solver convergence.

The script rasterizes compositing inputs from a mesh and multiview image, computes an exact
direct-solver reference, sweeps the number of Gauss-Seidel iterations used by the multigrid solver,
and plots error and runtime.

Usage:
    uv run --extra scripts modules/texproc/python/examples/plot_convergence_benchmark.py \
        --mesh data/corp/texproc/prepared/pumpkin.glb \
        --multiview data/corp/texproc/original/multiview.png \
        --output convergence.png

    uv run --extra scripts ./modules/texproc/python/examples/plot_convergence_benchmark.py \
        --mesh data/corp/texproc/mickey/mesh.glb \
        --multiview data/corp/texproc/mickey/multiview.png \
        --grid 3 3 --output convergence.png
"""

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any

import lagrange
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from texture_from_multiview import (  # noqa: E402  # ty: ignore[unresolved-import]
    append_cameras,
    split_multiview,
)


def rasterize_inputs(
    mesh_path: Path,
    cameras_path: Path | None,
    multiview_path: Path,
    grid: tuple[int, int],
    width: int,
    height: int,
    base_confidence: float,
) -> tuple[lagrange.SurfaceMesh, list[np.ndarray], list[np.ndarray]]:
    """Rasterize compositing inputs from a mesh and multiview image."""
    with Image.open(multiview_path) as multiview:
        views = split_multiview(multiview, grid)

    scene = lagrange.io.load_scene(mesh_path, stitch_vertices=True)
    if cameras_path is not None:
        cameras = json.loads(cameras_path.read_text(encoding="utf-8"))
        append_cameras(scene, cameras)
    textures, weights = lagrange.texproc.rasterize_textures_from_renders(
        scene,
        views,
        width=width,
        height=height,
        base_confidence=base_confidence,
    )
    return lagrange.scene.scene_to_mesh(scene), textures, weights


def compute_image_distance(approx: np.ndarray, reference: np.ndarray) -> dict[str, float]:
    """Compute per-channel distance metrics between identically shaped images."""
    if approx.shape != reference.shape:
        raise ValueError(f"Image shape mismatch: {approx.shape} != {reference.shape}")
    difference = approx.astype(np.float64) - reference.astype(np.float64)
    abs_difference = np.abs(difference)
    return {
        "rmse": float(np.sqrt(np.mean(np.square(difference)))),
        "mae": float(np.mean(abs_difference)),
        "max_abs": float(np.max(abs_difference)),
    }


def compositing_options(args: argparse.Namespace) -> dict[str, Any]:
    gradient_normalizations = {
        "per-edge": lagrange.texproc.GradientNormalization.PerEdge,
        "per-texel-sqrt": lagrange.texproc.GradientNormalization.PerTexelSqrt,
    }
    return {
        "value_weight": args.value_weight,
        "quadrature_samples": args.quadrature,
        "jitter_epsilon": args.jitter_epsilon,
        "clamp_to_range": tuple(args.clamp) if args.clamp is not None else None,
        "gradient_normalization": gradient_normalizations[args.gradient_normalization],
        "stiffness_regularization_weight": args.regularization,
        "smooth_low_weight_areas": args.smooth_low_weight_areas,
        "sanity_check": args.sanity_check,
        "num_multigrid_levels": args.num_multigrid_levels,
        "num_v_cycles": args.num_v_cycles,
    }


def run_benchmark(
    mesh: lagrange.SurfaceMesh,
    textures: list[np.ndarray],
    weights: list[np.ndarray],
    args: argparse.Namespace,
) -> dict[str, Any]:
    """Run the direct reference and multigrid convergence sweep."""
    options = compositing_options(args)

    print("Computing direct-solver reference...")
    start = time.perf_counter()
    reference = lagrange.texproc.texture_compositing(
        mesh, textures, weights, use_direct_solver=True, **options
    )
    direct_seconds = time.perf_counter() - start
    print(f"direct: {direct_seconds:.3f} s")

    iterations = []
    for count in range(2, args.max_gauss_seidel_iterations + 1, 1):
        start = time.perf_counter()
        approx = lagrange.texproc.texture_compositing(
            mesh,
            textures,
            weights,
            use_direct_solver=False,
            num_gauss_seidel_iterations=count,
            **options,
        )
        solve_seconds = time.perf_counter() - start
        result = {
            "num_gauss_seidel_iterations": count,
            "num_v_cycles": args.num_v_cycles,
            **compute_image_distance(approx, reference),
            "solve_seconds": solve_seconds,
        }
        iterations.append(result)
        print(
            f"iterations={count:2d} rmse={result['rmse']:.3e} "
            f"mae={result['mae']:.3e} max={result['max_abs']:.3e} "
            f"time={solve_seconds:.3f} s"
        )

    return {
        "reference_solver": "direct",
        "direct_solve_seconds": direct_seconds,
        "max_gauss_seidel_iterations": args.max_gauss_seidel_iterations,
        "options": {
            "value_weight": args.value_weight,
            "quadrature_samples": args.quadrature,
            "jitter_epsilon": args.jitter_epsilon,
            "clamp_to_range": args.clamp,
            "gradient_normalization": args.gradient_normalization,
            "stiffness_regularization_weight": args.regularization,
            "smooth_low_weight_areas": args.smooth_low_weight_areas,
            "sanity_check": args.sanity_check,
            "num_multigrid_levels": args.num_multigrid_levels,
            "num_v_cycles": args.num_v_cycles,
        },
        "iterations": iterations,
    }


def plot_results(data: dict[str, Any], output: Path | None) -> None:
    """Plot convergence errors and runtimes."""
    iterations = data["iterations"]
    gs_iterations = [entry["num_gauss_seidel_iterations"] for entry in iterations]
    rmse = [entry["rmse"] for entry in iterations]
    mae = [entry["mae"] for entry in iterations]
    max_abs = [entry["max_abs"] for entry in iterations]
    solve_seconds = [entry["solve_seconds"] for entry in iterations]
    direct_seconds = data["direct_solve_seconds"]

    fig, (ax_err, ax_time) = plt.subplots(1, 2, figsize=(12, 5))

    ax_err.semilogy(gs_iterations, rmse, "o-", label="RMSE")
    ax_err.semilogy(gs_iterations, mae, "s-", label="MAE")
    ax_err.semilogy(gs_iterations, max_abs, "^-", label="max abs")
    ax_err.set_xlabel("Number of Gauss-Seidel iterations")
    ax_err.set_ylabel("Distance to direct solution")
    ax_err.set_title("Iterative solver convergence")
    ax_err.grid(True, which="both", ls=":", alpha=0.5)
    ax_err.legend()

    ax_time.plot(gs_iterations, solve_seconds, "o-", color="tab:green", label="iterative solve")
    ax_time.axhline(
        direct_seconds,
        color="tab:red",
        ls="--",
        label=f"direct solve ({direct_seconds:.3f} s)",
    )
    ax_time.set_xlabel("Number of Gauss-Seidel iterations")
    ax_time.set_ylabel("Solve time (s)")
    ax_time.set_title("Solve time vs. iterations")
    ax_time.grid(True, ls=":", alpha=0.5)
    ax_time.legend()

    subtitle = ", ".join(f"{key}={value}" for key, value in data["options"].items())
    fig.suptitle(subtitle, fontsize=9)
    fig.tight_layout()

    if output is not None:
        fig.savefig(output, dpi=150, bbox_inches="tight")
        print(f"Saved figure to {output}")
    else:
        plt.show()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mesh", type=Path, required=True, help="Input mesh or scene with UVs.")
    parser.add_argument(
        "--cameras",
        type=Path,
        default=None,
        help="Optional camera JSON when the input scene has no cameras.",
    )
    parser.add_argument("--multiview", type=Path, required=True, help="Input multiview image.")
    parser.add_argument("--grid", type=int, nargs=2, default=(4, 4), metavar=("ROWS", "COLS"))
    parser.add_argument("--width", type=int, default=1024, help="Rasterized texture width.")
    parser.add_argument("--height", type=int, default=1024, help="Rasterized texture height.")
    parser.add_argument(
        "--base-confidence", type=float, default=0.0, help="Confidence in the mesh base texture."
    )
    parser.add_argument(
        "--output", type=Path, default=None, help="Save the figure instead of showing it."
    )
    parser.add_argument(
        "--json-output", type=Path, default=None, help="Optionally save raw benchmark results."
    )
    parser.add_argument("--value-weight", type=float, default=1e3)
    parser.add_argument("--quadrature", type=int, choices=(1, 3, 6, 12, 24, 32), default=6)
    parser.add_argument("--jitter-epsilon", type=float, default=1e-4)
    parser.add_argument(
        "--gradient-normalization",
        choices=("per-edge", "per-texel-sqrt"),
        default="per-texel-sqrt",
    )
    parser.add_argument("--regularization", type=float, default=1e-9)
    parser.add_argument("--smooth-low-weight-areas", action="store_true")
    parser.add_argument(
        "--sanity-check",
        action=argparse.BooleanOptionalAction,
        default=None,
        help="Override solver sanity checks (defaults depend on build type).",
    )
    parser.add_argument("--clamp", type=float, nargs=2, default=None, metavar=("MIN", "MAX"))
    parser.add_argument("--num-multigrid-levels", type=int, default=4)
    parser.add_argument("--num-v-cycles", type=int, default=4)
    parser.add_argument("--max-gauss-seidel-iterations", type=int, default=30)
    args = parser.parse_args()
    if args.max_gauss_seidel_iterations < 2:
        parser.error("--max-gauss-seidel-iterations must be at least 2")
    return args


def main() -> None:
    args = parse_args()
    mesh, textures, weights = rasterize_inputs(
        args.mesh,
        args.cameras,
        args.multiview,
        (args.grid[0], args.grid[1]),
        args.width,
        args.height,
        args.base_confidence,
    )
    results = run_benchmark(mesh, textures, weights, args)

    if args.json_output is not None:
        args.json_output.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
        print(f"Saved benchmark results to {args.json_output}")
    plot_results(results, args.output)


if __name__ == "__main__":
    main()
