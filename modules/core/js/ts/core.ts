/**
 * Core mesh type and the operations that run on it.
 */

/**
 * A triangle/polygon mesh: vertices, facets, and attached attributes
 * (positions, normals, UVs, per-vertex/facet scalars, ...). The central
 * data type of the library — most functions take or return a `SurfaceMesh`.
 *
 * Create one with `new lagrange.core.SurfaceMesh(dimension)` (usually `3`)
 * and populate it via `addVertices` / `addTriangles`, or load one from a
 * file buffer via `lagrange.io.loadMeshFromBuffer(...)`.
 *
 * Backed by WASM heap memory. Call {@link delete} when finished to free it
 * (JS garbage collection does not reclaim it).
 *
 * Terminology:
 * - **vertex**: a unique 3D point.
 * - **facet**: a polygon (triangle, quad, ...) made of vertex references.
 * - **corner**: one occurrence of a vertex inside a facet. Attributes that
 *   differ between adjacent facets (seam UVs, hard-edge normals) live at
 *   corner level.
 * - **edge**: oriented pair of vertices; only populated for some queries.
 */
export interface SurfaceMesh {
  getNumVertices(): number;
  getNumFacets(): number;
  getNumCorners(): number;
  getNumEdges(): number;
  /** Dimension of vertex positions (`3` for 3D meshes, `2` for 2D). */
  getDimension(): number;
  /** True if every facet has exactly 3 vertices. */
  isTriangleMesh(): boolean;
  /** True if every facet has exactly 4 vertices. */
  isQuadMesh(): boolean;
  /** True if every facet has the same vertex count. */
  isRegular(): boolean;
  /** True if facets have varying vertex counts (mixed tris/quads/...). */
  isHybrid(): boolean;
  /** Vertex count per facet for regular meshes; `0` for hybrid meshes. */
  getVertexPerFacet(): number;
  /** Vertex count of a specific facet. */
  getFacetSize(facetId: number): number;
  /** Vertex id of the `localVertexId`-th corner of `facetId`. */
  getFacetVertex(facetId: number, localVertexId: number): number;
  /** First corner id belonging to `facetId` (inclusive). */
  getFacetCornerBegin(facetId: number): number;
  /** Corner id one past the last corner of `facetId` (exclusive). */
  getFacetCornerEnd(facetId: number): number;
  /** Vertex id the given corner refers to. */
  getCornerVertex(cornerId: number): number;
  /** Facet the given corner belongs to. */
  getCornerFacet(cornerId: number): number;
  /** Copy of one vertex's coordinates. Length equals {@link getDimension}. */
  getPosition(vertexId: number): Float32Array;
  /** Copy of the vertex ids that form `facetId`. */
  getFacetVertices(facetId: number): Uint32Array;
  /** Release unused capacity reserved by prior `add*` calls. */
  shrinkToFit(): void;
  /** Drop all facets. Vertices are kept. */
  clearFacets(): void;
  /** Drop all vertices and facets. */
  clearVertices(): void;
  /** Whether a named attribute exists on the mesh. */
  hasAttribute(name: string): boolean;
  /**
   * Append one vertex. `vertex.length` must equal {@link getDimension}
   * (typically `3`).
   */
  addVertex(vertex: Float32Array | ArrayLike<number>): void;
  /** Append a single triangle from three vertex ids. */
  addTriangle(v0: number, v1: number, v2: number): void;
  /**
   * Append many triangles from a flat vertex-id buffer:
   * `[t0v0, t0v1, t0v2, t1v0, t1v1, t1v2, ...]`. Length must be a
   * multiple of 3.
   */
  addTriangles(indices: Uint32Array | ArrayLike<number>): void;
  /**
   * Append many vertices from a flat coordinate buffer. Length must be
   * a multiple of {@link getDimension}.
   */
  addVertices(coords: Float64Array | ArrayLike<number>): void;
  /** Remove the listed vertices; remaining vertices and facets are reindexed. */
  removeVertices(indices: number[] | Uint32Array): void;
  /** Remove the listed facets. */
  removeFacets(indices: number[] | Uint32Array): void;
  /**
   * Reverse facet orientation (winding order). Pass a list of facet ids to
   * flip only those, or omit to flip every facet.
   */
  flipFacets(indices?: number[] | Uint32Array): void;
  /**
   * Return a new `SurfaceMesh` that is an independent copy of this one.
   *
   * Underlying attribute buffers are shared via copy-on-write until either
   * mesh is mutated, so the clone is cheap. Required when you want to
   * modify a derived mesh without affecting the original — plain JS
   * assignment (`let b = a`) only copies the handle to the same WASM
   * object.
   *
   * Pass `{ strip: true }` to drop all non-reserved attributes
   * (normals, UVs, tangents, user attributes, ...) and keep only the core
   * topology (vertex positions, facet indices, corner connectivity).
   * Useful to hand the mesh to an algorithm that would otherwise preserve
   * stale derived data.
   *
   * The returned mesh owns its own WASM handle and must be freed with
   * {@link delete}.
   */
  clone(opts?: { strip?: boolean }): SurfaceMesh;
  /** Free the underlying WASM memory. The mesh must not be used afterwards. */
  delete(): void;
}

/** Sentinel returned in place of a scalar value that is not set or not valid. */
export const invalidScalar: number = Infinity;

/** Sentinel returned in place of a vertex / facet / corner id that is missing or invalid. */
export const invalidIndex: number = 0xffffffff;

/**
 * Construct meshes, measure them, clean them up, and compute per-element
 * attributes. Accessible as `lagrange.core`.
 */
export interface CoreModule {
  /**
   * {@link SurfaceMesh} constructor. Use as `new lagrange.core.SurfaceMesh(3)`
   * for a 3D mesh, or `2` for a 2D mesh.
   */
  SurfaceMesh: new (dimension: number) => SurfaceMesh;
  /**
   * Return a new mesh in which every attribute is per-vertex — any per-corner
   * seams (hard-edge normals, UV charts) are resolved by duplicating the
   * vertices they sit on.
   *
   * Required for GPU rendering: a unified index buffer is what WebGL/WebGPU
   * expect. Chain with `bindings.meshToBabylonMeshDataView64(...)` for
   * zero-copy upload.
   */
  unifyIndexBuffer(mesh: SurfaceMesh): SurfaceMesh;
  /**
   * Fill topological holes whose boundary has at most `maxHoleSize` vertices.
   * Modifies `mesh` in place. Set `triangulateHoles` to triangulate the new
   * patches; otherwise they are inserted as a single polygon.
   */
  closeSmallHoles(
    mesh: SurfaceMesh,
    maxHoleSize: number,
    triangulateHoles?: boolean,
  ): void;
  /** Remove degenerate (non-triangle / zero-area) facets. In-place. Triangle meshes only. */
  removeDegenerateFacets(mesh: SurfaceMesh): void;
  /** Merge vertices that share the same position. In-place. */
  removeDuplicateVertices(mesh: SurfaceMesh): void;
  /**
   * Subdivide edges longer than `maxEdgeLength` (Euclidean length).
   * Set `recursive` to keep splitting until no edge exceeds the threshold.
   * In-place.
   */
  splitLongEdges(
    mesh: SurfaceMesh,
    maxEdgeLength: number,
    recursive: boolean,
  ): void;

  // --- Normals ---

  /**
   * Attach per-vertex normals. Equivalent to fully smooth shading.
   * In-place: adds a normal attribute.
   */
  computeVertexNormal(mesh: SurfaceMesh, opts?: VertexNormalOptions): void;
  /**
   * Attach per-facet normals. Equivalent to flat shading.
   * In-place: adds a normal attribute.
   */
  computeFacetNormal(mesh: SurfaceMesh): void;
  /**
   * Attach indexed (per-corner) normals with crease-based smoothing:
   * edges whose dihedral angle exceeds `featureAngleThreshold` (radians)
   * stay as hard edges, the rest are smoothed. This is the "edge split"
   * style of normal used by most DCC tools. In-place.
   */
  computeNormal(
    mesh: SurfaceMesh,
    featureAngleThreshold: number,
    opts?: NormalOptions,
  ): void;
  /**
   * Attach tangent / bitangent attributes for normal mapping.
   * Requires the mesh to already have UVs and normals.
   * In-place.
   */
  computeTangentBitangent(
    mesh: SurfaceMesh,
    opts?: TangentBitangentOptions,
  ): void;

  // --- Mesh operations ---

  /** Convert every polygon with more than 3 vertices into triangles. In-place. */
  triangulatePolygonalFacets(mesh: SurfaceMesh): void;
  /** Merge several meshes into a single mesh. Returns the combined result. */
  combineMeshes(
    meshes: SurfaceMesh[],
    opts?: CombineMeshesOptions,
  ): SurfaceMesh;
  /**
   * Label connected components of the mesh. Returns the number of components
   * and attaches a per-facet component-id attribute.
   */
  computeComponents(mesh: SurfaceMesh, opts?: ComponentOptions): number;
  /**
   * Flip facet winding so normals point outward (or inward, via options).
   * Works per connected component. In-place.
   */
  orientOutward(mesh: SurfaceMesh, opts?: OrientOptions): void;
  /** Translate and scale the mesh to fit the unit cube centered at the origin. In-place. */
  normalizeMesh(mesh: SurfaceMesh): void;
  /** Attach a per-facet surface-area attribute. In-place. */
  computeFacetArea(mesh: SurfaceMesh): void;

  // --- Topology queries ---

  /** True when the mesh is both edge- and vertex-manifold. */
  isManifold(mesh: SurfaceMesh): boolean;
  /** True when every vertex has a disk-like neighborhood. */
  isVertexManifold(mesh: SurfaceMesh): boolean;
  /** True when every edge is shared by at most two facets. */
  isEdgeManifold(mesh: SurfaceMesh): boolean;
  /** True when the mesh has no boundary edges (watertight). */
  isClosed(mesh: SurfaceMesh): boolean;
  /** Euler characteristic: `V - E + F`. */
  computeEuler(mesh: SurfaceMesh): number;

  // --- Seam edges, valence, coloring ---

  /**
   * Mark edges that sit on a seam of the given indexed attribute
   * (different attribute values on the two sides). Adds a per-edge
   * boolean attribute. In-place.
   */
  computeSeamEdges(mesh: SurfaceMesh, indexedAttributeId: number): void;
  /** Attach a per-vertex valence (incident-edge count) attribute. In-place. */
  computeVertexValence(mesh: SurfaceMesh): void;
  /**
   * Color facets (or vertices) such that no neighbors share a color,
   * using a greedy algorithm. Useful for partitioning work or visualizing
   * topology. Adds a per-element color-id attribute. In-place.
   */
  computeGreedyColoring(mesh: SurfaceMesh, opts?: GreedyColoringOptions): void;

  // --- Additional mesh cleanup ---

  /** Remove facets that duplicate another facet. In-place. */
  removeDuplicateFacets(mesh: SurfaceMesh, opts?: RemoveDuplicateFacetsOptions): void;
  /** Remove vertices that no facet references. In-place. */
  removeIsolatedVertices(mesh: SurfaceMesh): void;
  /** Remove facets whose area is below the threshold. In-place. */
  removeNullAreaFacets(mesh: SurfaceMesh, opts?: RemoveNullAreaFacetsOptions): void;
  /** Collapse edges shorter than `threshold`. In-place. */
  removeShortEdges(mesh: SurfaceMesh, threshold?: number): void;
  /** Remove facets that repeat a vertex (e.g. `[a, b, a]`). In-place. */
  removeTopologicallyDegenerateFacets(mesh: SurfaceMesh): void;
  /**
   * Make the mesh manifold by duplicating non-manifold edges and vertices.
   * In-place.
   */
  resolveNonmanifoldness(mesh: SurfaceMesh): void;
  /** Make vertices manifold by duplicating (edges are left as-is). In-place. */
  resolveVertexNonmanifoldness(mesh: SurfaceMesh): void;
}

export const coreModuleKeys = [
  "SurfaceMesh",
  "unifyIndexBuffer",
  "closeSmallHoles",
  "removeDegenerateFacets",
  "removeDuplicateVertices",
  "splitLongEdges",
  "computeVertexNormal",
  "computeFacetNormal",
  "computeNormal",
  "computeTangentBitangent",
  "triangulatePolygonalFacets",
  "combineMeshes",
  "computeComponents",
  "orientOutward",
  "normalizeMesh",
  "computeFacetArea",
  "isManifold",
  "isVertexManifold",
  "isEdgeManifold",
  "isClosed",
  "computeEuler",
  "computeSeamEdges",
  "computeVertexValence",
  "computeGreedyColoring",
  "removeDuplicateFacets",
  "removeIsolatedVertices",
  "removeNullAreaFacets",
  "removeShortEdges",
  "removeTopologicallyDegenerateFacets",
  "resolveNonmanifoldness",
  "resolveVertexNonmanifoldness",
] as const satisfies readonly (keyof CoreModule)[];

/**
 * How a corner's normal is weighted when averaging into a vertex normal.
 * - `"uniform"`: every corner contributes equally.
 * - `"cornerTriangleArea"`: weight by incident triangle area.
 * - `"angle"`: weight by corner angle (least sensitive to tessellation).
 */
export type NormalWeightingType = "uniform" | "cornerTriangleArea" | "angle";

export interface VertexNormalOptions {
  /** Corner-weighting scheme. Default: `"angle"`. */
  weightType?: NormalWeightingType;
  /** Recompute weighted corner normals even if a cached attribute exists. Default: `false`. */
  recomputeWeightedCornerNormals?: boolean;
  /** Keep weighted corner normals as a separate attribute after computation. Default: `false`. */
  keepWeightedCornerNormals?: boolean;
  /** Merge vertices within this distance before computing. Default: `0` (exact match only). */
  distanceTolerance?: number;
}

export interface NormalOptions {
  /** Corner-weighting scheme. Default: `"angle"`. */
  weightType?: NormalWeightingType;
  /** Recompute facet normals even if cached. Default: `false`. */
  recomputeFacetNormals?: boolean;
  /** Keep facet normals as a separate attribute. Default: `false`. */
  keepFacetNormals?: boolean;
  /** Merge vertices within this distance before computing. Default: `0`. */
  distanceTolerance?: number;
  /**
   * Vertex ids that should always be treated as sharp (e.g. the tip of a cone).
   * Normals there will never be smoothed regardless of dihedral angle.
   */
  coneVertices?: number[];
}

export interface TangentBitangentOptions {
  /** Emit tangent as `vec4`, with the 4th component encoding bitangent sign. Default: `false`. */
  padWithSign?: boolean;
  /** Gram-Schmidt orthogonalize the bitangent against tangent and normal. Default: `false`. */
  orthogonalizeBitangent?: boolean;
  /** Keep an existing tangent attribute if already present on the mesh. Default: `false`. */
  keepExistingTangent?: boolean;
}

export interface CombineMeshesOptions {
  /** Carry attributes from inputs into the combined mesh. Default: `true`. */
  preserveAttributes?: boolean;
}

/**
 * How connectivity is traversed:
 * - `"edge"`: two facets are connected iff they share an edge.
 * - `"vertex"`: two facets are connected iff they share any vertex.
 */
export type ConnectivityType = "vertex" | "edge";

export interface ComponentOptions {
  /** Traversal mode. Default: `"edge"`. */
  connectivityType?: ConnectivityType;
  /**
   * Element ids (edges or vertices, depending on {@link connectivityType})
   * that act as walls — they block component growth across them.
   */
  blockerElements?: number[];
}

export interface OrientOptions {
  /** `true` → normals face outward; `false` → inward. Default: `true`. */
  positive?: boolean;
}

export interface SeamEdgesOptions {}

export interface VertexValenceOptions {}

/** Which mesh element to color. */
export type ColoringElementType = "vertex" | "facet";

export interface GreedyColoringOptions {
  /** Element to color. Default: `"facet"`. */
  elementType?: ColoringElementType;
  /** Upper bound on the number of distinct colors. Default: `8`. */
  numColorUsed?: number;
}

export interface RemoveDuplicateFacetsOptions {
  /**
   * If `true`, two facets with the same vertices but opposite winding
   * are considered distinct. Default: `false`.
   */
  considerOrientation?: boolean;
}

export interface RemoveNullAreaFacetsOptions {
  /** Facets with area at or below this threshold are removed. Default: `0`. */
  nullAreaThreshold?: number;
  /** Also remove vertices that become unreferenced after the removal. Default: `false`. */
  removeIsolatedVertices?: boolean;
}
