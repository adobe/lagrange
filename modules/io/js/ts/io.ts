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
 * Read and write mesh / scene files from in-memory byte buffers — no
 * filesystem access. To load from a URL, `fetch` it first and pass the
 * resulting `Uint8Array`.
 *
 * Supported formats are auto-detected on load.
 *
 * Omitted option fields fall back to library defaults.
 */

import type { SurfaceMesh } from "./core.js";
import type { Scene } from "./scene.js";

/** Single-mesh output formats. */
export type OutputMeshFormat = "obj" | "ply" | "glb" | "gltf";

/** Scene-graph output formats. */
export type OutputSceneFormat = "glb" | "gltf" | "obj";

/** Knobs for `loadMeshFromBuffer` / `loadSceneFromBuffer`. */
export interface LoadOptions {
  /** Triangulate polygons on load. Default: `false`. */
  triangulate?: boolean;
  /** Import vertex normals if present. Default: `true`. */
  loadNormals?: boolean;
  /** Import tangents and bitangents if present. Default: `true`. */
  loadTangents?: boolean;
  /** Import texture coordinates. Default: `true`. */
  loadUvs?: boolean;
  /** Import skinning joints and weights. Default: `true`. */
  loadWeights?: boolean;
  /** Import per-facet material ids. Default: `true`. */
  loadMaterials?: boolean;
  /** Import per-vertex colors. Default: `true`. */
  loadVertexColors?: boolean;
  /** Import per-facet object ids. Default: `true`. */
  loadObjectIds?: boolean;
  /** Resolve and decode referenced image data. Default: `true`. */
  loadImages?: boolean;
  /** Import line elements (e.g. OBJ polylines) as 2-vertex facets with a `line_id` attribute. Default: `true`. */
  loadLines?: boolean;
  /** Merge coincident boundary vertices while loading. Default: `false`. */
  stitchVertices?: boolean;
  /** Suppress warnings printed to the console. Default: `false`. */
  quiet?: boolean;
}

/** Knobs for `saveMeshToBuffer` / `saveSceneToBuffer`. */
export interface SaveOptions {
  /**
   * Override the default encoding. If unset, `binary` is used for `ply`/`glb`
   * and `ascii` for `obj`/`gltf`.
   */
  encoding?: "binary" | "ascii";
  /** Which attributes to write. Default: `"all"`. */
  outputAttributes?: "all" | "selectedOnly";
  /** Attribute ids to write when {@link outputAttributes} is `"selectedOnly"`. */
  selectedAttributes?: number[];
  /**
   * How to handle attributes whose type the format can't store natively.
   * `"exactMatchOnly"` drops them; `"convertAsNeeded"` casts to a
   * supported type. Default: `"exactMatchOnly"`.
   */
  attributeConversionPolicy?: "exactMatchOnly" | "convertAsNeeded";
  /** Embed image data inside the file where the format supports it (e.g. `glb`). Default: `false`. */
  embedImages?: boolean;
  /** Write materials and textures. Default: `true`. */
  exportMaterials?: boolean;
  /** Suppress warnings printed to the console. Default: `false`. */
  quiet?: boolean;
}

/**
 * Loading / saving. Accessible as `lagrange.io`.
 */
export interface IOModule {
  /** Parse a mesh from the bytes of an `obj`/`ply`/`glb`/`gltf` file. Format is auto-detected. */
  loadMeshFromBuffer(data: Uint8Array, opts?: LoadOptions): SurfaceMesh;
  /** Serialize a mesh to the given file format. Returns the encoded bytes. */
  saveMeshToBuffer(mesh: SurfaceMesh, format: OutputMeshFormat, opts?: SaveOptions): Uint8Array<ArrayBuffer>;
  /** Parse a full scene (multi-mesh, materials, nodes) from file bytes. Format is auto-detected. */
  loadSceneFromBuffer(data: Uint8Array, opts?: LoadOptions): Scene;
  /** Serialize a scene to `glb`, `gltf`, or `obj`. Returns the encoded bytes. */
  saveSceneToBuffer(scene: Scene, format: OutputSceneFormat, opts?: SaveOptions): Uint8Array<ArrayBuffer>;
}

export const ioModuleKeys = [
  "loadMeshFromBuffer",
  "saveMeshToBuffer",
  "loadSceneFromBuffer",
  "saveSceneToBuffer",
] as const satisfies readonly (keyof IOModule)[];
