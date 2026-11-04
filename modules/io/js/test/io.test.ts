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

describe("io SimpleScene", () => {
  function triangle() {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertices([0, 0, 0, 1, 0, 0, 0, 1, 0]);
    mesh.addTriangle(0, 1, 2);
    return mesh;
  }

  test.each(["glb", "gltf"] as const)("round-trip preserves instances via %s", (format) => {
    const mesh = triangle();
    const scene = lagrange.scene.meshesToSimpleScene([mesh, mesh]);
    const transform = new Float64Array([2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 5, 6, 7, 1]);
    scene.refInstance(0, 0).transform = transform;
    const instance = new lagrange.scene.MeshInstance();
    instance.meshIndex = 0;
    scene.addInstance(instance);
    instance.delete();
    const bytes = lagrange.io.saveSimpleSceneToBuffer(scene, format, { quiet: true, exportMaterials: false });
    expect(bytes).toBeInstanceOf(Uint8Array);
    expect(bytes.length).toBeGreaterThan(0);
    if (format === "glb") expect(new TextDecoder().decode(bytes.subarray(0, 4))).toBe("glTF");
    else expect(JSON.parse(new TextDecoder().decode(bytes)).asset.version).toBe("2.0");
    scene.delete();
    mesh.delete();

    const restored = lagrange.io.loadSimpleSceneFromBuffer(bytes, {
      quiet: true, triangulate: true, loadMaterials: false, loadImages: false,
    });
    expect(restored.getNumMeshes()).toBe(2);
    expect(restored.computeNumInstances()).toBe(3);
    expect([0, 1].map((i) => restored.getNumInstances(i))).toEqual([2, 1]);
    expect(restored.refInstance(0, 0).transform).toEqual(transform);
    expect(restored.refInstance(1, 0).meshIndex).toBe(1);
    expect(restored.refMesh(1).getNumFacets()).toBe(1);
    const flattened = lagrange.scene.simpleSceneToMesh(restored);
    expect(flattened.getNumFacets()).toBe(3);
    flattened.delete();
    restored.delete();
  });

  test("OBJ output bakes instance transforms with default options", () => {
    const mesh = triangle();
    const scene = lagrange.scene.meshToSimpleScene(mesh);
    scene.refInstance(0, 0).transform = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 3, 4, 1];
    const bytes = lagrange.io.saveSimpleSceneToBuffer(scene, "obj");
    const restored = lagrange.io.loadMeshFromBuffer(bytes, { quiet: true });
    expect(restored.getNumFacets()).toBe(1);
    expect(restored.getPosition(0)).toEqual(new Float32Array([2, 3, 4]));
    restored.delete();
    scene.delete();
    mesh.delete();
  });

  test("rejects unsupported formats and malformed input", () => {
    const scene = new lagrange.scene.SimpleScene();
    expect(() => lagrange.io.saveSimpleSceneToBuffer(scene, "ply" as never)).toThrow();
    expect(() => lagrange.io.loadSimpleSceneFromBuffer(new Uint8Array([1, 2, 3, 4]))).toThrow();
    scene.delete();
  });
});
