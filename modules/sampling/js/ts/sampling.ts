/**
 * Generate point sets on a mesh surface. Useful for point-cloud generation,
 * Monte-Carlo integration, particle placement, and distance metrics.
 *
 * Omitted option fields fall back to library defaults.
 */

export interface UniformSamplingOptions {
  /** How many points to produce. Default: `0`. */
  numSamples?: number;
  /** Seed for the random generator; pass a non-zero value for reproducible results. Default: `0`. */
  seed?: number;
}

export interface BlueNoiseSamplingOptions {
  /** How many points to produce. Default: `0`. */
  numSamples?: number;
  /** Seed for the random generator. Default: `0`. */
  seed?: number;
  /** Weight toward the mesh's density map (`0` = ignore, `1` = full bias). Default: `0`. */
  bias?: number;
  /** Candidates drawn per accepted sample; higher = better distribution, slower. Default: `8`. */
  oversampleRatio?: number;
  /** Scale factor on the Poisson-disk radius. Larger = more spread-out samples. Default: `4`. */
  rmaxFactor?: number;
}

/** Output of {@link SamplingModule.sampleUniform}. All arrays share `numSamples` ordering. */
export interface UniformSamplingResult {
  /** Flat 3D positions: `[x0, y0, z0, x1, y1, z1, ...]`. Length is `3 * numSamples`. */
  positions: Float64Array;
  /** Facet id each sample lies on. */
  facetIndices: Uint32Array;
  /** Number of samples actually produced. */
  numSamples: number;
}

/** Output of {@link SamplingModule.sampleBlueNoise}. All arrays share `numSamples` ordering. */
export interface BlueNoiseSamplingResult {
  /** Flat 3D positions: `[x0, y0, z0, x1, y1, z1, ...]`. Length is `3 * numSamples`. */
  positions: Float64Array;
  /** Facet id each sample lies on. */
  facetIndices: Uint32Array;
  /** Value of the underlying density function at each sample. */
  densities: Float64Array;
  /** Number of samples actually produced. */
  numSamples: number;
}

export interface EpsilonSamplingOptions {
  /** Seed for the random generator. Default: `0`. */
  seed?: number;
  /**
   * Number of consecutive rejected darts after which sampling terminates. Larger values produce
   * a more nearly maximal (better covered) sampling at the cost of more computation. Default:
   * `1000`.
   */
  maxFailedAttempts?: number;
  /** Whether to also output barycentric coordinates for each sample. Default: `false`. */
  computeBarycentric?: boolean;
  /**
   * Whether to check if every facet-referenced vertex is epsilon-covered by a sample, adding it
   * to the samples if not. Default: `false`.
   */
  ensureVertexCoverage?: boolean;
}

/** Output of {@link SamplingModule.sampleEpsilon}. All arrays share `numSamples` ordering. */
export interface EpsilonSamplingResult {
  /** Flat 3D positions: `[x0, y0, z0, x1, y1, z1, ...]`. Length is `3 * numSamples`. */
  positions: Float64Array;
  /** Facet id each sample lies on. */
  facetIndices: Uint32Array;
  /**
   * Flat barycentric coordinates: `[b0, b1, b2, ...]`. Length is `3 * numSamples`. Only
   * populated when `computeBarycentric` is `true`.
   */
  barycentric: Float64Array;
  /** Number of samples actually produced. */
  numSamples: number;
}

import type { SurfaceMesh } from "./core.js";

/**
 * Surface point-sampling. Accessible as `lagrange.sampling`.
 */
export interface SamplingModule {
  /**
   * Draw points independently and uniformly at random on the mesh surface
   * (weighted by triangle area). Fast but clumpy.
   */
  sampleUniform(mesh: SurfaceMesh, opts?: UniformSamplingOptions): UniformSamplingResult;
  /**
   * Draw points with a Poisson-disk (blue-noise) distribution: neighbors
   * are kept at least `rmaxFactor`-scaled apart. Slower than uniform sampling
   * but produces visibly nicer coverage.
   */
  sampleBlueNoise(mesh: SurfaceMesh, opts?: BlueNoiseSamplingOptions): BlueNoiseSamplingResult;
  /**
   * Generate an epsilon sampling graded by a local feature size (LFS) field: every point on the
   * surface is meant to lie within `epsilon * lfs` of some sample (an epsilon-sample in the
   * Amenta-Bern sense). Sampling is denser where LFS is small and sparser where LFS is large.
   * Unlike {@link sampleBlueNoise}, the number of samples is not specified upfront; it emerges
   * from `epsilon` and the LFS field.
   *
   * @param lfsAttribute Name of a per-vertex, single-channel scalar attribute holding the local
   *   feature size. Values must be strictly positive.
   * @param epsilon Sampling density constant. Must be strictly positive.
   */
  sampleEpsilon(
    mesh: SurfaceMesh,
    lfsAttribute: string,
    epsilon: number,
    opts?: EpsilonSamplingOptions,
  ): EpsilonSamplingResult;
}

export const samplingModuleKeys = [
  "sampleUniform",
  "sampleBlueNoise",
  "sampleEpsilon",
] as const satisfies readonly (keyof SamplingModule)[];
