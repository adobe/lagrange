/**
 * Fast binary round-trip for a {@link SurfaceMesh} or {@link Scene}, including
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
import type { Scene } from "./scene.js";

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
   * Accept input that was serialized as a scene (single-mesh scenes are
   * unwrapped automatically). Default: `false`.
   */
  allowSceneConversion?: boolean;
  /**
   * Allow casting between scalar / index types if they don't match the
   * target mesh. Default: `false`.
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
  /** Serialize the scene (meshes, materials, nodes) into a compact binary blob. */
  serializeScene(scene: Scene, opts?: SerializeOptions): Uint8Array<ArrayBuffer>;
  /** Restore a scene previously produced by {@link serializeScene}. */
  deserializeScene(data: Uint8Array, opts?: DeserializeOptions): Scene;
}

export const serializationModuleKeys = [
  "serializeMesh",
  "deserializeMesh",
  "serializeScene",
  "deserializeScene",
] as const satisfies readonly (keyof SerializationModule)[];
