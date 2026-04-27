/**
 * Procedural mesh generators: spheres, cubes, cones, tori, swept surfaces.
 * Each comes with UVs and normals ready for rendering.
 *
 * Omitted option fields fall back to sensible defaults.
 */

import type { SurfaceMesh } from "./core.js";

/**
 * Options shared by every primitive generator. Pass only the fields you want
 * to change from the defaults.
 */
export interface PrimitiveOptions {
  /** World-space center of the primitive. Default: `[0, 0, 0]`. */
  center?: [number, number, number];
  /** Include a top cap, for primitives that have one. Default: `true`. */
  withTopCap?: boolean;
  /** Include a bottom cap, for primitives that have one. Default: `true`. */
  withBottomCap?: boolean;
  /** Include cross-section geometry when the shape is swept less than a full turn. Default: `true`. */
  withCrossSection?: boolean;
  /** Triangulate polygonal output. Default: `true`. */
  triangulate?: boolean;
  /** Use a canonical UV layout instead of one parameterized by shape settings. Default: `false`. */
  fixedUv?: boolean;
  /** Distance below which two vertices are considered coincident (weld tolerance). Default: `1e-6`. */
  distThreshold?: number;
  /** Dihedral angle above which an edge is marked as sharp (radians). Default: `30° in radians`. */
  angleThreshold?: number;
  /** Numerical tolerance for scalar comparisons during generation. Default: `1e-6`. */
  epsilon?: number;
  /** Padding between UV charts to avoid texture bleeding. Default: `0.005`. */
  uvPadding?: number;
}

/** UV-sphere: longitude/latitude grid. Set sweep angles to build a wedge. */
export interface SphereOptions extends PrimitiveOptions {
  /** Sphere radius. */
  radius?: number;
  /** Divisions around the vertical axis. */
  numLongitudeSections?: number;
  /** Divisions between the two poles. */
  numLatitudeSections?: number;
  /** Start of the longitudinal sweep, in radians. */
  startSweepAngle?: number;
  /** End of the longitudinal sweep, in radians. Full sphere: `2π`. */
  endSweepAngle?: number;
}

/** Donut / ring shape. Set sweep angles to build a partial torus. */
export interface TorusOptions extends PrimitiveOptions {
  /** Distance from the torus center to the center of the tube. */
  majorRadius?: number;
  /** Tube thickness (radius of the circular cross-section). */
  minorRadius?: number;
  /** Divisions around the main ring. */
  ringSegments?: number;
  /** Divisions around the tube cross-section. */
  pipeSegments?: number;
  /** Start of the main-ring sweep, in radians. */
  startSweepAngle?: number;
  /** End of the main-ring sweep, in radians. Full torus: `2π`. */
  endSweepAngle?: number;
}

/** Rounded box. Set `bevelRadius = 0` for a sharp cube. */
export interface RoundedCubeOptions extends PrimitiveOptions {
  /** Extent along X. */
  width?: number;
  /** Extent along Y. */
  height?: number;
  /** Extent along Z. */
  depth?: number;
  /** Subdivisions along X. */
  widthSegments?: number;
  /** Subdivisions along Y. */
  heightSegments?: number;
  /** Subdivisions along Z. */
  depthSegments?: number;
  /** Corner/edge bevel radius. `0` gives a sharp cube. */
  bevelRadius?: number;
  /** Bevel-arc subdivisions. */
  bevelSegments?: number;
}

/** Cone/cylinder/truncated cone with optional rounded rims. */
export interface RoundedConeOptions extends PrimitiveOptions {
  /** Radius of the top disk. Equal to `radiusBottom` for a cylinder; `0` for a pointed cone. */
  radiusTop?: number;
  /** Radius of the bottom disk. */
  radiusBottom?: number;
  /** Distance between top and bottom caps. */
  height?: number;
  /** Bevel radius where the top cap meets the side. */
  bevelRadiusTop?: number;
  /** Bevel radius where the bottom cap meets the side. */
  bevelRadiusBottom?: number;
  /** Divisions around the vertical axis. */
  radialSections?: number;
  /** Bevel-arc subdivisions at the top. */
  bevelSegmentsTop?: number;
  /** Bevel-arc subdivisions at the bottom. */
  bevelSegmentsBottom?: number;
  /** Subdivisions along the side between top and bottom. */
  sideSegments?: number;
  /** Start of the sweep around the axis, in radians. */
  startSweepAngle?: number;
  /** End of the sweep around the axis, in radians. Full cone: `2π`. */
  endSweepAngle?: number;
}

/** Flat 2D disc with optional angular wedge. */
export interface DiscOptions extends PrimitiveOptions {
  /** Outer radius. */
  radius?: number;
  /** Start angle of the wedge, in radians. */
  startAngle?: number;
  /** End angle of the wedge, in radians. Full disc: `2π`. */
  endAngle?: number;
  /** Radial wedge subdivisions. */
  radialSections?: number;
  /** Concentric rings. */
  numRings?: number;
}

/** Regular octahedron. */
export interface OctahedronOptions extends PrimitiveOptions {
  /** Circumscribing sphere radius. */
  radius?: number;
}

/** Regular icosahedron. */
export interface IcosahedronOptions extends PrimitiveOptions {
  /** Circumscribing sphere radius. */
  radius?: number;
}

/** Rectangular plane with rounded corners. */
export interface RoundedPlaneOptions extends PrimitiveOptions {
  /** Extent along X. */
  width?: number;
  /** Extent along Y. */
  height?: number;
  /** Corner bevel radius. `0` gives a sharp rectangle. */
  bevelRadius?: number;
  /** Subdivisions along X. */
  widthSegments?: number;
  /** Subdivisions along Y. */
  heightSegments?: number;
  /** Bevel-arc subdivisions. */
  bevelSegments?: number;
}

/**
 * The path along which {@link PrimitiveModule.generateSweptSurface} drags the
 * 2D profile. Discriminated via `type`:
 * - `"linear"`: straight segment from {@link LinearSweepOptions.from} to `to`.
 * - `"circular"`: arc around {@link CircularSweepOptions.axis}.
 */
export type SweepOptions = LinearSweepOptions | CircularSweepOptions;

interface SweepOptionsBase {
  /** Number of evaluation samples along the sweep. Default: `16`. */
  numSamples?: number;
  /** Close the sweep into a loop (wraps back on itself). */
  periodic?: boolean;
  /** Parameter range used by `twistFunction`/`taperFunction`/`offsetFunction`. Default: `[0, 1]`. */
  domain?: [number, number];
  /** Pivot point applied to the profile before sweeping. Default: `[0, 0, 0]`. */
  pivot?: [number, number, number];
  /** If `true`, the profile rotates to stay perpendicular to the path tangent. Default: `true`. */
  followTangent?: boolean;
  /** Rotation around the tangent as a function of `t`. Return radians. */
  twistFunction?: (t: number) => number;
  /** Uniform profile scale as a function of `t`. Return `1` to keep original size. */
  taperFunction?: (t: number) => number;
  /** Lateral offset of the profile as a function of `t`, in world units. */
  offsetFunction?: (t: number) => number;
}

export interface LinearSweepOptions extends SweepOptionsBase {
  type: "linear";
  /** Start point of the sweep `[x, y, z]`. */
  from: [number, number, number];
  /** End point of the sweep `[x, y, z]`. */
  to: [number, number, number];
}

export interface CircularSweepOptions extends SweepOptionsBase {
  type: "circular";
  /** A point on the circular path `[x, y, z]` (defines the radius). */
  point: [number, number, number];
  /** Rotation axis `[x, y, z]`. Should be a unit vector. */
  axis: [number, number, number];
  /** Total sweep angle, in radians. Default: `2π` (full loop). */
  angle?: number;
}

export interface SweptSurfaceOptions extends PrimitiveOptions {
  /** Map the profile's arc length to the U axis of the generated UVs. Default: `true`. */
  useUAsProfileLength?: boolean;
  /** Angle (radians) at which to split UVs across profile corners. Default: `π/4`. */
  profileAngleThreshold?: number;
  /** Max profile segment length before the UV is split. `≤ 0` disables splitting. Default: `0`. */
  maxProfileLength?: number;
}

export interface SubdividedSphereOptions extends PrimitiveOptions {
  /** Sphere radius. */
  radius?: number;
  /**
   * Number of subdivisions on top of the base icosahedron. `0` = the bare
   * icosahedron; each increment roughly quadruples the face count and
   * makes the surface rounder.
   */
  subdivLevel?: number;
}

/**
 * Procedural mesh generators. Accessible as `lagrange.primitive`.
 */
export interface PrimitiveModule {
  /** UV-sphere built from latitude/longitude grid. */
  generateSphere(opts?: SphereOptions): SurfaceMesh;
  /** Donut. */
  generateTorus(opts?: TorusOptions): SurfaceMesh;
  /** Box with optional bevelled edges. */
  generateRoundedCube(opts?: RoundedCubeOptions): SurfaceMesh;
  /** Cone, truncated cone, or cylinder (depending on top/bottom radii) with optional rim bevels. */
  generateRoundedCone(opts?: RoundedConeOptions): SurfaceMesh;
  /** Flat disc / wedge. */
  generateDisc(opts?: DiscOptions): SurfaceMesh;
  /** 8-face regular solid. */
  generateOctahedron(opts?: OctahedronOptions): SurfaceMesh;
  /** 20-face regular solid. */
  generateIcosahedron(opts?: IcosahedronOptions): SurfaceMesh;
  /** Rectangle with rounded corners. */
  generateRoundedPlane(opts?: RoundedPlaneOptions): SurfaceMesh;
  /**
   * Geodesic-style sphere: icosahedron repeatedly subdivided and projected
   * to the sphere. More uniform triangle sizes than {@link generateSphere}.
   */
  generateSubdividedSphere(opts?: SubdividedSphereOptions): SurfaceMesh;
  /**
   * Extrude/revolve a 2D profile curve along a sweep path to build a surface.
   *
   * @param profile Flat 2D polyline `[x0, y0, x1, y1, ...]`.
   * @param sweep Either a linear or circular path (discriminated by `type`).
   * @param opts Smoothing, UV, and general primitive options.
   */
  generateSweptSurface(profile: number[] | Float64Array, sweep: SweepOptions, opts?: SweptSurfaceOptions): SurfaceMesh;
}

export const primitiveModuleKeys = [
  "generateSphere",
  "generateTorus",
  "generateRoundedCube",
  "generateRoundedCone",
  "generateDisc",
  "generateOctahedron",
  "generateIcosahedron",
  "generateRoundedPlane",
  "generateSubdividedSphere",
  "generateSweptSurface",
] as const satisfies readonly (keyof PrimitiveModule)[];
