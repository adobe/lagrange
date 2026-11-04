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

/** Scene graphs, instanced meshes, and conversions between scene representations. */

import type { SurfaceMesh } from "./core.js";

/** A mesh instance in a node; material ids index the scene's material collection. */
export interface SceneMeshInstance {
  mesh: number;
  /** Multiple slots use a mesh's per-facet `material_id` attribute to select a slot. */
  materials: number[];
}

/** A mesh instance to attach to a node. Omitted materials means no material. */
export interface SceneMeshInstanceOptions {
  mesh: number;
  materials?: readonly number[];
}

/** Owned snapshot of a node. Modifying it does not modify the scene. */
export interface SceneNode {
  name: string;
  /** Local-to-parent affine matrix: 16 column-major floats, translation at indices 12–14. */
  transform: Float32Array<ArrayBuffer>;
  /** `null` for a root node. */
  parent: number | null;
  children: number[];
  meshes: SceneMeshInstance[];
}

/**
 * Fields to create or patch a node. Omitted fields are unchanged on update;
 * a new node defaults to an unnamed, identity-transform root with no meshes.
 */
export interface NodeOptions {
  name?: string;
  /** Finite, float-representable column-major affine matrix; last row must be [0, 0, 0, 1]. */
  transform?: ArrayLike<number>;
  /** Reparent to this node; `null` makes the node a root. Local transform is unchanged. */
  parent?: number | null;
  /** Replace all mesh instances in the node. */
  meshes?: readonly SceneMeshInstanceOptions[];
}

/** Texture reference in a material. Texture and image asset editing is not exposed. */
export interface SceneTextureInfo {
  /** Index in the scene's texture collection, or `null` for no texture. */
  index: number | null;
  /** Nonnegative texture-coordinate set index. */
  texcoord: number;
}

/** Partial texture-reference update. Omitted fields are preserved. */
export interface TextureInfoOptions {
  index?: number | null;
  texcoord?: number;
}

/** Owned PBR material snapshot. Vectors and texture references do not alias the scene. */
export interface SceneMaterial {
  name: string;
  baseColorValue: Float32Array<ArrayBuffer>;
  baseColorTexture: SceneTextureInfo;
  emissiveValue: Float32Array<ArrayBuffer>;
  emissiveTexture: SceneTextureInfo;
  metallicValue: number;
  roughnessValue: number;
  metallicRoughnessTexture: SceneTextureInfo;
  alphaMode: "opaque" | "mask" | "blend";
  alphaCutoff: number;
  normalScale: number;
  normalTexture: SceneTextureInfo;
  occlusionStrength: number;
  occlusionTexture: SceneTextureInfo;
  doubleSided: boolean;
}

/**
 * Create or patch a PBR material. Omitted fields use native defaults on creation
 * and retain existing values on update. `null` clears a texture reference.
 * Scalars/vectors must be finite and float-representable; native extensions are preserved.
 */
export interface MaterialOptions {
  name?: string;
  /** RGBA base color, four components. */
  baseColorValue?: ArrayLike<number>;
  baseColorTexture?: TextureInfoOptions | null;
  /** RGB emissive color, three components. */
  emissiveValue?: ArrayLike<number>;
  emissiveTexture?: TextureInfoOptions | null;
  metallicValue?: number;
  roughnessValue?: number;
  metallicRoughnessTexture?: TextureInfoOptions | null;
  alphaMode?: "opaque" | "mask" | "blend";
  alphaCutoff?: number;
  normalScale?: number;
  normalTexture?: TextureInfoOptions | null;
  occlusionStrength?: number;
  occlusionTexture?: TextureInfoOptions | null;
  doubleSided?: boolean;
}

/**
 * Scene graph backed by WASM memory. Create with `new lagrange.scene.Scene()`
 * or load via IO/serialization. Call {@link delete} when finished.
 * Collection ids are stable, zero-based integers; invalid ids throw.
 */
export interface Scene {
  /** Scene name; not necessarily unique. */
  getName(): string;
  /** Set the scene name. */
  setName(name: string): void;
  /** Number of stored meshes, including meshes with no instances. */
  getNumMeshes(): number;
  /**
   * Return an owned copy-on-write mesh handle. Call its `delete()` when done.
   * Mutations do not affect the scene until passed to {@link setMesh}.
   * The handle survives scene deletion and collection growth.
   */
  getMesh(meshId: number): SurfaceMesh;
  /** Add a copy-on-write mesh copy; does not create a node instance. Returns its id. */
  addMesh(mesh: SurfaceMesh): number;
  /** Replace the stored mesh; all instances of this id use the replacement. */
  setMesh(meshId: number, mesh: SurfaceMesh): void;
  /** Number of transform-graph nodes, including empty groups. */
  getNumNodes(): number;
  /** Owned array of root node ids. */
  getRootNodes(): number[];
  /** Owned node snapshot. Apply edits with {@link setNode}. */
  getNode(nodeId: number): SceneNode;
  /**
   * Create a node and maintain its parent's children or the root list. Returns its id.
   * Omit options to create an unnamed identity-transform root with no mesh instances.
   */
  addNode(opts?: NodeOptions): number;
  /**
   * Atomically patch a node. Reparenting updates both parents and roots without duplicates.
   * Cycles, invalid references, and invalid transforms throw without applying any fields.
   * Existing cameras, lights, and native extensions are preserved.
   */
  setNode(nodeId: number, opts: NodeOptions): void;
  /** Owned column-major local-to-world matrix, accumulated through the parent chain. */
  getGlobalTransform(nodeId: number): Float32Array<ArrayBuffer>;
  /** Number of stored materials. */
  getNumMaterials(): number;
  /** Owned material snapshot. Apply edits with {@link setMaterial}. */
  getMaterial(materialId: number): SceneMaterial;
  /** Create a PBR material. Omit options to use native defaults. Returns its id. */
  addMaterial(opts?: MaterialOptions): number;
  /**
   * Atomically patch a material, preserving unspecified fields and native extensions.
   * Options getters finish before commit; omitted texture subfields retain values at commit time.
   * Scene mutations performed by getters are not rolled back if validation fails.
   */
  setMaterial(materialId: number, opts: MaterialOptions): void;
  /** Free the underlying WASM memory. Do not use the scene afterwards. */
  delete(): void;
}

/** A 3D mesh instance. Newly constructed instances have an identity transform. */
export interface MeshInstance {
  /** Source mesh index. Initially the core module's exported `invalidIndex` sentinel (`0xffffffff`). */
  meshIndex: number;
  /**
   * Copy of the 4×4 affine transform, in column-major order (translation at indices 12–14).
   * Assign a flat array of 16 values to update it; mutating the returned copy has no effect.
   */
  get transform(): Float64Array;
  set transform(values: ArrayLike<number>);
  /** Free an owned instance. Do not call on a borrowed {@link SimpleScene.refInstance} handle. */
  delete(): void;
}

/**
 * A collection of 3D meshes and their instances, without a node hierarchy or materials.
 * Create with `new lagrange.scene.SimpleScene()` and call {@link delete} when finished.
 */
export interface SimpleScene {
  getNumMeshes(): number;
  /** Number of instances of the specified mesh. */
  getNumInstances(meshIndex: number): number;
  /** Total number of instances across all meshes. */
  computeNumInstances(): number;
  /** Independent copy-on-write mesh copy. The caller must delete it. */
  getMesh(meshIndex: number): SurfaceMesh;
  /**
   * Borrow a mutable mesh. Do not delete this handle. It becomes invalid when the scene
   * is deleted or its mesh storage is reallocated (e.g. by addMesh or reserveMeshes).
   */
  refMesh(meshIndex: number): SurfaceMesh;
  /** Independent instance copy. The caller must delete it. */
  getInstance(meshIndex: number, instanceIndex: number): MeshInstance;
  /**
   * Borrow a mutable instance. Do not delete it or change its meshIndex. Conversions reject
   * changed meshIndex values. The handle becomes invalid when the scene is deleted or its
   * instance storage is reallocated.
   */
  refInstance(meshIndex: number, instanceIndex: number): MeshInstance;
  /** Reserve mesh storage without changing the mesh count. */
  reserveMeshes(numMeshes: number): void;
  /** Copy a 3D mesh into the scene (no instances added). Returns its index. */
  addMesh(mesh: SurfaceMesh): number;
  /** Reserve instance storage for one mesh without changing its instance count. */
  reserveInstances(meshIndex: number, numInstances: number): void;
  /** Copy an instance into the scene. Returns its index among instances of its source mesh. */
  addInstance(instance: MeshInstance): number;
  /** Free the underlying WASM memory and invalidate all borrowed handles. */
  delete(): void;
}

/** How a scene's total facet budget is distributed across its source meshes. */
export type FacetAllocationStrategy =
  | "evenSplit"
  | "relativeToMeshArea"
  | "relativeToNumFacets"
  | "synchronized";

/** How a remeshing algorithm treats source meshes with no instances. */
export type UninstantiatedMeshesStrategy = "none" | "skip" | "replaceWithEmpty";

/** Options controlling remeshing of instanced scenes. */
export interface RemeshingOptions {
  /** Facet-budget allocation strategy. Default: `"evenSplit"`. */
  facetAllocationStrategy?: FacetAllocationStrategy;
  /** Minimum target facets per instantiated mesh. Default: backend-specific (`4` for decimation). */
  minFacets?: number;
  /** Behavior for meshes with no instances. Default: `"none"` (backend-specific behavior). */
  uninstantiatedMeshesStrategy?: UninstantiatedMeshesStrategy;
  /**
   * Positive, finite importance factors, ordered by mesh index then local instance index.
   * Provide one value per instance, or an empty array to disable weighting. Default: disabled.
   */
  perInstanceImportance?: ArrayLike<number>;
}

/** Options applied when baking instance transforms into mesh geometry and attributes. */
export interface SceneTransformOptions {
  /** Normalize transformed normals. Default: `true`. */
  normalizeNormals?: boolean;
  /** Normalize transformed tangents and bitangents. Default: `true`. */
  normalizeTangentsBitangents?: boolean;
  /** Flip facets and reorient attributes for negative-determinant transforms. Default: `false`. */
  reorient?: boolean;
}

/** Options for flattening a scene into a combined mesh. */
export interface SceneToMeshOptions extends SceneTransformOptions {
  /** Preserve shared attributes on the combined mesh. Default: `true`. */
  preserveAttributes?: boolean;
}

/** Flattened meshes with their global material indices (indices into the source Scene). */
export interface MeshesAndMaterialsResult {
  /**
   * Owned meshes with world transforms applied, one per instance. Each carries a `material_id`
   * facet attribute indexing the source scene's material list directly, not `materialIds[i]`.
   * Facets without a valid material use the invalid-index sentinel (`0xffffffff`).
   */
  meshes: SurfaceMesh[];
  /**
   * Per-instance material-ID lists, in one-to-one correspondence with `meshes`.
   * Each is copied unchanged from its source instance and indexes the source scene's material
   * list. Lists may contain duplicates or materials unused by any facet; material objects are
   * not returned. Use each mesh's `material_id` facet attribute for exact face assignments.
   */
  materialIds: number[][];
}

/**
 * Construct scenes and convert between {@link SurfaceMesh}, {@link Scene}, and {@link SimpleScene}.
 * Accessible as `lagrange.scene`. Conversion results are owned copies and must be deleted;
 * input meshes and scenes remain unchanged.
 */
export interface SceneModule {
  /** Construct an empty scene graph. */
  Scene: new () => Scene;
  /** Construct an empty 3D instanced-mesh scene. */
  SimpleScene: new () => SimpleScene;
  /** Construct an instance; set meshIndex before adding it to a SimpleScene. */
  MeshInstance: new () => MeshInstance;
  /** Wrap a single mesh in a scene with one identity instance. */
  meshToScene(mesh: SurfaceMesh): Scene;
  /** Wrap meshes in a scene with one identity instance per mesh. */
  meshesToScene(meshes: readonly SurfaceMesh[]): Scene;
  /** Flatten all mesh instances into a combined mesh, baking their world transforms. */
  sceneToMesh(scene: Scene, opts?: SceneToMeshOptions): SurfaceMesh;
  /** One owned mesh per instance with world transforms applied and a `material_id` facet attribute. */
  sceneToMeshes(scene: Scene, opts?: SceneTransformOptions): SurfaceMesh[];
  /**
   * Flatten instances and report their material indices. Source `material_id` facet values are
   * local slots in each instance's material list and are remapped to global scene material IDs.
   * Without facet assignments, all faces use the first listed material (or the invalid sentinel).
   * Meshes without instances are omitted. Delete each returned mesh when finished.
   */
  sceneToMeshesAndMaterials(scene: Scene, opts?: SceneTransformOptions): MeshesAndMaterialsResult;
  /** Wrap a 3D mesh in a SimpleScene with one identity instance. */
  meshToSimpleScene(mesh: SurfaceMesh): SimpleScene;
  /** Wrap 3D meshes in a SimpleScene with one identity instance per mesh. */
  meshesToSimpleScene(meshes: readonly SurfaceMesh[]): SimpleScene;
  /** Flatten all instances into a combined mesh, baking their transforms. */
  simpleSceneToMesh(scene: SimpleScene, opts?: SceneToMeshOptions): SurfaceMesh;
  /** One owned mesh per instance with its transform applied. */
  simpleSceneToMeshes(scene: SimpleScene, opts?: SceneTransformOptions): SurfaceMesh[];
  /**
   * Flatten the node hierarchy into instances with accumulated world transforms.
   * Requires every source mesh to be 3D, including meshes without instances.
   * Materials and other metadata (images, textures, cameras, lights) are discarded.
   */
  sceneToSimpleScene(scene: Scene): SimpleScene;
  /** Each instance becomes a child of a single root node. No materials or other metadata are added. */
  simpleSceneToScene(scene: SimpleScene): Scene;
}

export const sceneModuleKeys = [
  "Scene",
  "SimpleScene",
  "MeshInstance",
  "meshToScene",
  "meshesToScene",
  "sceneToMesh",
  "sceneToMeshes",
  "sceneToMeshesAndMaterials",
  "meshToSimpleScene",
  "meshesToSimpleScene",
  "simpleSceneToMesh",
  "simpleSceneToMeshes",
  "sceneToSimpleScene",
  "simpleSceneToScene",
] as const satisfies readonly (keyof SceneModule)[];
