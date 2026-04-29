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

import { describe, test, expect, beforeAll } from "vitest";
import { loadLagrange } from "../src/loadLagrange.node.js";
import type { Lagrange } from "../src/types.js";

var lagrange: Lagrange;

beforeAll(async () => {
  lagrange = await loadLagrange();
});

describe("io mesh round-trip", () => {
  const formats: Array<"obj" | "ply" | "glb" | "gltf"> = ["obj", "ply", "glb", "gltf"];

  for (const format of formats) {
    test(`save + load preserves mesh via ${format}`, () => {
      const mesh = lagrange.primitive.generateSphere({ radius: 1 });
      const numVerts = mesh.getNumVertices();
      const numFacets = mesh.getNumFacets();

      const bytes = lagrange.io.saveMeshToBuffer(mesh, format);
      expect(bytes.length).toBeGreaterThan(0);

      const restored = lagrange.io.loadMeshFromBuffer(bytes, { quiet: true });
      expect(restored.getNumVertices()).toBe(numVerts);
      // Some formats (e.g. obj) may re-triangulate or drop duplicates;
      // facet count may differ. For sphere, all supported formats here
      // preserve facet count.
      expect(restored.getNumFacets()).toBe(numFacets);

      restored.delete();
      mesh.delete();
    });
  }


  test("saveMeshToBuffer rejects unknown format", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    expect(() => lagrange.io.saveMeshToBuffer(mesh, "bogus" as any)).toThrow();
    mesh.delete();
  });

  test("ascii encoding opt overrides default for ply", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const binary = lagrange.io.saveMeshToBuffer(mesh, "ply");
    const ascii = lagrange.io.saveMeshToBuffer(mesh, "ply", { encoding: "ascii" });

    // ascii ply begins with "ply\nformat ascii"
    const head = new TextDecoder().decode(ascii.slice(0, 32));
    expect(head).toContain("format ascii");
    // binary ply header declares binary_little_endian (or big)
    const binHead = new TextDecoder().decode(binary.slice(0, 64));
    expect(binHead).toContain("format binary");

    mesh.delete();
  });
});

describe("io scene round-trip", () => {
  // obj is save-only: stream-based io::load_scene supports Gltf/Fbx only,
  // so loading an obj scene buffer throws. Saving is supported.
  const roundTripFormats: Array<"glb" | "gltf"> = ["glb", "gltf"];

  for (const format of roundTripFormats) {
    test(`save + load preserves scene via ${format}`, () => {
      const mesh = lagrange.primitive.generateSphere({ radius: 1 });
      const scene = lagrange.scene.meshToScene(mesh);
      const numMeshes = scene.getNumMeshes();

      const bytes = lagrange.io.saveSceneToBuffer(scene, format);
      expect(bytes.length).toBeGreaterThan(0);

      const restored = lagrange.io.loadSceneFromBuffer(bytes, { quiet: true });
      expect(restored.getNumMeshes()).toBe(numMeshes);

      restored.delete();
      scene.delete();
      mesh.delete();
    });
  }

  test("save scene to obj produces bytes (load not supported)", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const scene = lagrange.scene.meshToScene(mesh);
    const bytes = lagrange.io.saveSceneToBuffer(scene, "obj");
    expect(bytes.length).toBeGreaterThan(0);
    scene.delete();
    mesh.delete();
  });

  test("saveSceneToBuffer rejects unknown format", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const scene = lagrange.scene.meshToScene(mesh);
    expect(() => lagrange.io.saveSceneToBuffer(scene, "bogus" as any)).toThrow();
    scene.delete();
    mesh.delete();
  });
});
