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
"""
Run instance-level occlusion culling on every asset in a folder. For each input scene the
fully-occluded instances are dropped and the result is saved to a mirror output directory.

Uses the lagrange.raycasting.remove_occluded_instances Python binding directly — no CLI
subprocess, no analytics. Intended as a simple bulk-processing tool.

Usage:
    uv run python batch_cull_instances.py <input_dir> [--output-dir DIR]
        [--num-rays N] [--batch-size N] [--until-converged]
        [--extensions .obj .glb] [--output-extension .obj] [--recursive]
"""

import argparse
import contextlib
import logging
import sys
import time
import traceback
from pathlib import Path

import lagrange
import lagrange.io
import lagrange.raycasting

# lagrange routes its C++ spdlog through Python's `logging` module (sink installed at module
# import). Without basicConfig(level=INFO) we'd only see WARNING+ — i.e., we'd miss every
# per-batch "X/Y instances visible" line and the "All instances visible, stopping early"
# notice. Configure root logging with a terse format so progress is actually visible.
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s", datefmt="%H:%M:%S"
)


@contextlib.contextmanager
def _suppress_lagrange_below(level: int):
    """Temporarily raise the `lagrange` logger threshold — useful around loader calls that
    emit chatty warnings (e.g. the glTF stitch_vertices nag) without silencing the rest of
    the run. Restores the previous level on exit."""
    lg = logging.getLogger("lagrange")
    prev = lg.level
    lg.setLevel(level)
    try:
        yield
    finally:
        lg.setLevel(prev)


def find_assets(input_dir: Path, extensions: list[str], recursive: bool) -> list[Path]:
    assets: list[Path] = []
    for ext in extensions:
        pattern = f"*{ext}"
        assets.extend(input_dir.rglob(pattern) if recursive else input_dir.glob(pattern))
    return sorted(assets)


def process_asset(
    asset: Path,
    output: Path,
    num_rays: int,
    batch_size: int,
    until_converged: bool,
) -> bool:
    try:
        with _suppress_lagrange_below(logging.ERROR):
            scene = lagrange.io.load_simple_scene(str(asset))
    except Exception as exc:
        print(f"  load failed: {exc}", file=sys.stderr)
        return False

    n_meshes_in = scene.num_meshes
    n_instances_in = scene.total_num_instances
    if n_instances_in == 0:
        print("  empty scene; skipping")
        return False

    try:
        result = lagrange.raycasting.remove_occluded_instances(
            scene,
            num_rays=num_rays,
            batch_size=batch_size,
            until_converged=until_converged,
        )
    except Exception:
        traceback.print_exc()
        return False

    n_meshes_out = result.num_meshes
    n_instances_out = result.total_num_instances
    print(
        f"  meshes: {n_meshes_in} → {n_meshes_out}, instances: {n_instances_in} → {n_instances_out}"
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        lagrange.io.save_simple_scene(str(output), result)
    except Exception as exc:
        print(f"  save failed: {exc}", file=sys.stderr)
        return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Bulk instance-level occlusion culling on a folder of assets."
    )
    parser.add_argument("input_dir", type=Path, help="Directory containing input assets.")

    # Defaults mirror OccludedInstanceEstimateOptions in
    # modules/raycasting/include/lagrange/raycasting/remove_occluded_instances.h.
    parser.add_argument("--num-rays", type=int, default=1_600_000_000, help="Total rays per asset.")
    parser.add_argument("--batch-size", type=int, default=20_000_000, help="Rays per batch.")
    parser.add_argument(
        "--until-converged",
        action="store_true",
        help="Stop early when a batch finds no new visible instances.",
    )

    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Output directory (default: <input_dir>/culled).",
    )
    parser.add_argument(
        "--extensions",
        nargs="+",
        default=[".obj", ".glb", ".gltf", ".fbx"],
        help="Input extensions to process.",
    )
    parser.add_argument(
        "--output-extension",
        default=".obj",
        help="Extension for output files (default: .obj).",
    )
    parser.add_argument(
        "--recursive",
        action="store_true",
        help="Recurse into subdirectories of input_dir.",
    )

    args = parser.parse_args()

    if not args.input_dir.is_dir():
        print(f"Input dir not found: {args.input_dir}", file=sys.stderr)
        return 1

    output_dir = args.output_dir or (args.input_dir / "culled")
    output_dir.mkdir(parents=True, exist_ok=True)

    assets = find_assets(args.input_dir, args.extensions, args.recursive)
    if not assets:
        print("No assets matched.", file=sys.stderr)
        return 1

    print(
        f"Found {len(assets)} asset(s). Output → {output_dir}. "
        f"Rays/asset: {args.num_rays:,} (batch {args.batch_size:,})."
    )

    succeeded = 0
    total_start = time.time()
    for i, asset in enumerate(assets, 1):
        rel = asset.relative_to(args.input_dir)
        out_path = output_dir / rel.with_suffix(args.output_extension)
        print(f"[{i}/{len(assets)}] {rel}")
        start = time.time()
        ok = process_asset(
            asset,
            out_path,
            args.num_rays,
            args.batch_size,
            args.until_converged,
        )
        elapsed = time.time() - start
        if ok:
            succeeded += 1
            print(f"  saved {out_path} ({elapsed:.1f}s)")
        else:
            print(f"  FAILED ({elapsed:.1f}s)", file=sys.stderr)

    total_elapsed = time.time() - total_start
    print(
        f"\nDone: {succeeded}/{len(assets)} succeeded in {total_elapsed:.1f}s "
        f"(avg {total_elapsed / max(len(assets), 1):.1f}s/asset)."
    )
    return 0 if succeeded == len(assets) else 1


if __name__ == "__main__":
    sys.exit(main())
