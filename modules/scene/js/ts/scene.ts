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
 * Scene graph: a collection of meshes linked to transform nodes plus
 * materials. Returned by `lagrange.io.loadSceneFromBuffer` when loading
 * formats that carry a hierarchy (`gltf` / `glb`).
 */

import type { SurfaceMesh } from "./core.js";

/**
 * A loaded scene graph. Holds multiple meshes plus the node hierarchy that
 * places them in the world. Backed by WASM memory — call {@link delete}
 * when done to release it.
 */
export interface Scene {
  /** Meshes referenced anywhere in the node tree. */
  getNumMeshes(): number;
  /** Transform-graph nodes (including empty groups). */
  getNumNodes(): number;
  /** Free the underlying WASM memory. Do not use the scene afterwards. */
  delete(): void;
}

/**
 * Convert between {@link SurfaceMesh} and {@link Scene}. Accessible as
 * `lagrange.scene`.
 */
export interface SceneModule {
  /** Wrap a single mesh in a trivial one-node scene. Useful before exporting to `gltf`/`glb`. */
  meshToScene(mesh: SurfaceMesh): Scene;
  /** Flatten every mesh in the scene graph into a single combined mesh. */
  sceneToMesh(scene: Scene): SurfaceMesh;
}

export const sceneModuleKeys = [
  "meshToScene",
  "sceneToMesh",
] as const satisfies readonly (keyof SceneModule)[];
