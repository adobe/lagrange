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
 * Fast binary round-trip for a {@link SurfaceMesh}, {@link SimpleScene}, or {@link Scene}, including
 * all attached attributes. Prefer this over `obj`/`ply`/`gltf` when storing
 * data in IndexedDB, caching in blob storage, or sending over the network —
 * it round-trips every attribute exactly and is significantly smaller than
 * the text-based formats.
 *
 * Use `lagrange.io` when you need interoperability with other 3D tools.
 *
 * Omitted option fields fall back to library defaults.
 */

import type { SurfaceMesh } from "./core.js";
import type { Scene, SimpleScene } from "./scene.js";

export interface SerializeOptions {
  /** Apply zstd compression. Disable for faster write at the cost of size. Default: `true`. */
  compress?: boolean;
  /** zstd compression level in `[1, 22]`. Higher = smaller but slower. Default: `3`. */
  compressionLevel?: number;
  /** Worker thread count. `0` = automatic, `1` = single-threaded. Default: `0`. */
  numThreads?: number;
}

export interface DeserializeOptions {
  /**
   * Allow converting between serialized meshes, simple scenes, and full scenes.
   * Default: `false`.
   */
  allowSceneConversion?: boolean;
  /**
   * Allow casting between scalar / index types if they don't match the
   * target representation. Default: `false`.
   */
  allowTypeCast?: boolean;
  /** Suppress warnings printed to the console. Default: `false`. */
  quiet?: boolean;
}

/**
 * Binary serialization of Lagrange meshes and scenes. Accessible as `lagrange.serialization`.
 */
export interface SerializationModule {
  /** Serialize the mesh (with all attributes) into a compact binary blob. */
  serializeMesh(mesh: SurfaceMesh, opts?: SerializeOptions): Uint8Array<ArrayBuffer>;
  /** Restore a mesh previously produced by {@link serializeMesh}. */
  deserializeMesh(data: Uint8Array, opts?: DeserializeOptions): SurfaceMesh;
  /** Serialize meshes, their attributes, and instance transforms into an independent binary blob. */
  serializeSimpleScene(scene: SimpleScene, opts?: SerializeOptions): Uint8Array<ArrayBuffer>;
  /** Restore an owned simple scene; call its delete() method when finished. */
  deserializeSimpleScene(data: Uint8Array, opts?: DeserializeOptions): SimpleScene;
  /** Serialize the scene (meshes, materials, nodes) into a compact binary blob. */
  serializeScene(scene: Scene, opts?: SerializeOptions): Uint8Array<ArrayBuffer>;
  /** Restore a scene previously produced by {@link serializeScene}. */
  deserializeScene(data: Uint8Array, opts?: DeserializeOptions): Scene;
}

export const serializationModuleKeys = [
  "serializeMesh",
  "deserializeMesh",
  "serializeSimpleScene",
  "deserializeSimpleScene",
  "serializeScene",
  "deserializeScene",
] as const satisfies readonly (keyof SerializationModule)[];
