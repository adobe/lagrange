/**
 * Refine a mesh by inserting new vertices/faces. Use for LOD up-resing,
 * smoothing low-poly cages, and displacement pipelines.
 *
 * Omitted option fields fall back to library defaults.
 */

import type { SurfaceMesh } from "./core.js";

/**
 * Subdivision algorithm:
 * - `"catmullClark"`: smooth quad-dominant surfaces. The standard film/DCC choice.
 * - `"loop"`: smooth triangle meshes.
 * - `"bilinear"`: no smoothing — each face is split into smaller copies of
 *   itself. Useful for displacement-map pipelines where the smoothing is
 *   added later by the displacement.
 */
export type SchemeType = "bilinear" | "catmullClark" | "loop";

/**
 * - `"uniform"`: split every face the same number of times.
 * - `"edgeAdaptive"`: split only where `maxEdgeLength` / `maxChordalDeviation`
 *   are exceeded — saves polys in flat regions.
 */
export type RefinementType = "uniform" | "edgeAdaptive";

/**
 * How vertices on mesh boundaries are handled.
 * - `"none"`: boundaries drift inward (sharp corners lost).
 * - `"edgeOnly"`: boundary edges stay put, corners can round.
 * - `"edgeAndCorner"`: boundary edges and corner vertices are pinned.
 */
export type VertexBoundaryInterpolation = "none" | "edgeOnly" | "edgeAndCorner";

/**
 * How per-corner (face-varying) attributes such as UVs are interpolated.
 * Progressively smoother: `"none"` keeps the most detail / sharp seams,
 * `"all"` smooths everything. `"corners*"` / `"boundaries"` are intermediate
 * settings mirroring OpenSubdiv's face-varying rules.
 */
export type FaceVaryingInterpolation =
  | "none"
  | "cornersOnly"
  | "cornersPlus1"
  | "cornersPlus2"
  | "boundaries"
  | "all";

export interface SubdivisionOptions {
  /**
   * Subdivision scheme. Omit to let the library pick based on mesh topology
   * (Catmull-Clark for quads, Loop for triangles, Bilinear for pure
   * displacement inputs).
   */
  scheme?: SchemeType;
  /** How many times to subdivide. Default: `1`. */
  numLevels?: number;
  /** Uniform vs. edge-adaptive refinement. Default: `"uniform"`. */
  refinement?: RefinementType;
  /** Rule for boundary vertex smoothing. Default: `"edgeOnly"`. */
  vertexBoundaryInterpolation?: VertexBoundaryInterpolation;
  /** Rule for per-corner attribute smoothing (e.g. UVs). Default: `"none"`. */
  faceVaryingInterpolation?: FaceVaryingInterpolation;
  /**
   * Evaluate the limit surface (what the mesh converges to at infinite
   * subdivision) instead of the finite-level approximation. Default: `false`.
   */
  useLimitSurface?: boolean;
  /** Run a topology sanity check on the input. Default: `false`. */
  validateTopology?: boolean;
  /** Keep shared-index connectivity where possible. Adaptive refinement only. Default: `false`. */
  preserveSharedIndices?: boolean;
  /** Edge-adaptive: refine any edge longer than this. */
  maxEdgeLength?: number;
  /** Edge-adaptive: refine whenever geometry deviates more than this from the limit surface. */
  maxChordalDeviation?: number;
}

/**
 * Mesh subdivision schemes. Accessible as `lagrange.subdivision`.
 */
export interface SubdivisionModule {
  /**
   * Smooth subdivision via OpenSubdiv. Picks Catmull-Clark, Loop, or
   * Bilinear based on {@link SubdivisionOptions.scheme} (or mesh topology
   * when unset).
   */
  subdivideMesh(mesh: SurfaceMesh, opts?: SubdivisionOptions): SurfaceMesh;
  /**
   * Split each edge at its midpoint and retriangulate. No smoothing —
   * geometry is preserved. Cheap way to add resolution before displacement.
   */
  midpointSubdivision(mesh: SurfaceMesh): SurfaceMesh;
  /** √3 subdivision, a triangle-only refiner with good isotropy. Triangle meshes only. */
  sqrtSubdivision(mesh: SurfaceMesh): SurfaceMesh;
}

export const subdivisionModuleKeys = [
  "subdivideMesh",
  "midpointSubdivision",
  "sqrtSubdivision",
] as const satisfies readonly (keyof SubdivisionModule)[];
