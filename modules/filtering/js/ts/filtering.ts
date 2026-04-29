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
 * Anisotropic smoothing of mesh geometry and per-vertex scalar fields.
 * Uses a normal/vertex diffusion formulation that preserves sharp features
 * better than naive Laplacian smoothing.
 *
 * Omitted option fields fall back to library defaults.
 */

import type { SurfaceMesh } from "./core.js";

/**
 * - `"normalSmoothing"`: diffuse facet normals, then fit geometry to them.
 *   Preserves sharp features better.
 * - `"vertexSmoothing"`: diffuse vertex positions directly. Faster, more aggressive.
 */
export type SmoothingMethod = "normalSmoothing" | "vertexSmoothing";

export interface MeshSmoothingOptions {
  /** Smoothing strategy. Default: `"normalSmoothing"`. */
  method?: SmoothingMethod;
  /**
   * Curvature-based feature weight in `[0, 1]`. Higher values protect
   * high-curvature regions (edges, corners) from being flattened.
   * Default: `0.02`.
   */
  curvatureWeight?: number;
  /** Diffusion time-step for normals; larger = more smoothing per call. Default: `0.1`. */
  normalSmoothingWeight?: number;
  /** Trade-off between matching smoothed normals and keeping positions. Default: `1`. */
  gradientWeight?: number;
  /** Gradient scaling: `<1` smooths further, `>1` sharpens. Default: `0`. */
  gradientModulationScale?: number;
  /** How strongly geometry is pulled toward the target normal field. Default: `0.1`. */
  normalProjectionWeight?: number;
}

export interface AttributeSmoothingOptions {
  /** Curvature-based feature weight in `[0, 1]`; higher = protect features more. Default: `0.02`. */
  curvatureWeight?: number;
  /** Diffusion time-step for normals used to build the anisotropic metric. Default: `0.1`. */
  normalSmoothingWeight?: number;
  /** Trade-off between matching gradients and keeping values. Default: `1`. */
  gradientWeight?: number;
  /** Gradient scaling; `<1` smooths more, `>1` sharpens. Default: `0`. */
  gradientModulationScale?: number;
}

/**
 * Smoothing / denoising. Accessible as `lagrange.filtering`.
 */
export interface FilteringModule {
  /**
   * Feature-preserving mesh smoothing. Moves vertex positions so the surface
   * becomes smoother while preserving corners and creases. In-place.
   */
  meshSmoothing(mesh: SurfaceMesh, opts?: MeshSmoothingOptions): void;
  /**
   * Smooth a named per-vertex scalar attribute across the surface. Useful for
   * cleaning up painted weights, noisy curvature, etc. In-place.
   */
  scalarAttributeSmoothing(mesh: SurfaceMesh, attributeName: string, opts?: AttributeSmoothingOptions): void;
}

export const filteringModuleKeys = [
  "meshSmoothing",
  "scalarAttributeSmoothing",
] as const satisfies readonly (keyof FilteringModule)[];
