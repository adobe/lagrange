/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

/**
 * Spatial queries powered by a bounding-volume hierarchy:
 * closest-point distances between meshes, mesh-to-mesh distance metrics,
 * and proximity-based vertex welding.
 *
 * Omitted option fields fall back to library defaults.
 */

import type { SurfaceMesh } from "./core.js";

/**
 * What to do when multiple source values map to the same target element.
 * - `"average"`: average the incoming values.
 * - `"keepFirst"`: keep the first value, discard the rest.
 * - `"error"`: throw on collision.
 */
export type MappingPolicy = "average" | "keepFirst" | "error";

export interface MeshDistancesOptions {
  /** Attribute name to write distances into. Default: `"@distance_to_mesh"`. */
  outputAttributeName?: string;
}

export interface WeldOptions {
  /** Vertices closer than this distance are merged. Default: `1e-6`. */
  radius?: number;
  /** Only consider vertices on the mesh boundary. Default: `false`. */
  boundaryOnly?: boolean;
  /** How to combine floating-point attributes of merged vertices. Default: `"average"`. */
  collisionPolicyFloat?: MappingPolicy;
  /** How to combine integer attributes of merged vertices. Default: `"keepFirst"`. */
  collisionPolicyIntegral?: MappingPolicy;
}

/**
 * Distance queries between meshes. Accessible as `lagrange.bvh`.
 */
export interface BVHModule {
  /**
   * For every vertex of `source`, compute the distance to the closest point
   * on `target` and store it as a per-vertex attribute on `source`.
   * In-place on `source`; `target` is untouched.
   */
  computeMeshDistances(source: SurfaceMesh, target: SurfaceMesh, opts?: MeshDistancesOptions): void;
  /**
   * Symmetric Hausdorff distance: the largest closest-point distance from any
   * vertex of either mesh to the other. Sensitive to outliers — good for
   * worst-case tolerancing.
   */
  computeHausdorff(source: SurfaceMesh, target: SurfaceMesh): number;
  /**
   * Symmetric Chamfer distance: the average closest-point distance between
   * the two meshes. Smoother than Hausdorff — good for shape-similarity
   * scoring.
   */
  computeChamfer(source: SurfaceMesh, target: SurfaceMesh): number;
  /**
   * Merge nearby vertices (stricter than `core.removeDuplicateVertices`
   * because it uses a radius rather than exact match). In-place.
   */
  weldVertices(mesh: SurfaceMesh, opts?: WeldOptions): void;
}

export const bvhModuleKeys = [
  "computeMeshDistances",
  "computeHausdorff",
  "computeChamfer",
  "weldVertices",
] as const satisfies readonly (keyof BVHModule)[];
