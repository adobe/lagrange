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

type Scene = InstanceType<Lagrange["scene"]["Scene"]>;
type MaterialOptions = Parameters<Scene["setMaterial"]>[1];
type SurfaceMesh = ReturnType<Lagrange["scene"]["sceneToMesh"]>;

let lagrange: Lagrange;

beforeAll(async () => {
  lagrange = await loadLagrange();
});

const textureFields = [
  "baseColorTexture",
  "emissiveTexture",
  "metallicRoughnessTexture",
  "normalTexture",
  "occlusionTexture",
] as const;

function makeTriangle() {
  const mesh = new lagrange.core.SurfaceMesh(3);
  mesh.addVertex([0, 0, 0]);
  mesh.addVertex([1, 0, 0]);
  mesh.addVertex([0, 1, 0]);
  mesh.addTriangle(0, 1, 2);
  return mesh;
}

/** A complete in-memory glTF with a one-pixel image, one texture, and an unknown extension. */
function loadTexturedMaterial() {
  const png = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=";
  const fixture = {
    asset: { version: "2.0" },
    scene: 0,
    scenes: [{ nodes: [0] }],
    nodes: [{ name: "material fixture" }],
    images: [{ uri: `data:image/png;base64,${png}` }],
    textures: [{ source: 0 }],
    extensionsUsed: ["VENDOR_material_test"],
    materials: [{
      name: "loaded material",
      pbrMetallicRoughness: {
        baseColorFactor: [0.25, 0.5, 0.75, 1],
        metallicFactor: 0.5,
        roughnessFactor: 0.75,
        baseColorTexture: { index: 0, texCoord: 0 },
        metallicRoughnessTexture: { index: 0, texCoord: 2 },
      },
      emissiveFactor: [0.125, 0.25, 0.5],
      emissiveTexture: { index: 0, texCoord: 1 },
      normalTexture: { index: 0, texCoord: 3, scale: 0.5 },
      occlusionTexture: { index: 0, texCoord: 4, strength: 0.75 },
      extensions: {
        VENDOR_material_test: { marker: "preserved", weights: [1, 2, 3] },
      },
    }],
  };
  return lagrange.io.loadSceneFromBuffer(
    new TextEncoder().encode(JSON.stringify(fixture)),
    { quiet: true },
  );
}

function facetMaterialIds(mesh: SurfaceMesh) {
  const attribute = mesh.getAttribute("material_id");
  expect(attribute.element).toBe("facet");
  expect(attribute.numChannels).toBe(1);
  return Array.from(attribute.data);
}

describe("scene material editing", () => {
  test("edits every PBR field without clamping and preserves unspecified values", () => {
    const scene = new lagrange.scene.Scene();
    const baseColor = new Float32Array([0.25, 0.5, 0.75, 1]);
    const emissive = new Float32Array([0.125, 0.25, 0.5]);
    try {
      const id = scene.addMaterial({
        name: "initial",
        baseColorValue: baseColor,
        emissiveValue: emissive,
        metallicValue: 0.5,
        roughnessValue: 0.75,
        alphaMode: "mask",
        alphaCutoff: 0.25,
        normalScale: 2,
        occlusionStrength: 0.5,
        doubleSided: true,
        baseColorTexture: { index: null, texcoord: 2 },
        emissiveTexture: { index: null, texcoord: 3 },
        metallicRoughnessTexture: { index: null, texcoord: 4 },
        normalTexture: { index: null, texcoord: 5 },
        occlusionTexture: { index: null, texcoord: 6 },
      });
      baseColor.fill(9);
      emissive.fill(9);
      const initial = scene.getMaterial(id);
      expect(initial).toEqual({
        name: "initial",
        baseColorValue: new Float32Array([0.25, 0.5, 0.75, 1]),
        emissiveValue: new Float32Array([0.125, 0.25, 0.5]),
        metallicValue: 0.5,
        roughnessValue: 0.75,
        alphaMode: "mask",
        alphaCutoff: 0.25,
        normalScale: 2,
        occlusionStrength: 0.5,
        doubleSided: true,
        baseColorTexture: { index: null, texcoord: 2 },
        emissiveTexture: { index: null, texcoord: 3 },
        metallicRoughnessTexture: { index: null, texcoord: 4 },
        normalTexture: { index: null, texcoord: 5 },
        occlusionTexture: { index: null, texcoord: 6 },
      });

      const patchedBaseColor = new Float32Array([-0.5, 1.5, 2, 0.5]);
      scene.setMaterial(id, {
        name: "edited",
        baseColorValue: patchedBaseColor,
        emissiveValue: [-1, 2, 4],
        metallicValue: -0.5,
        roughnessValue: 1.5,
        alphaMode: "blend",
        alphaCutoff: -0.25,
        normalScale: -2,
        occlusionStrength: 3,
        doubleSided: false,
      });
      patchedBaseColor.fill(8);
      expect(scene.getMaterial(id)).toEqual({
        ...initial,
        name: "edited",
        baseColorValue: new Float32Array([-0.5, 1.5, 2, 0.5]),
        emissiveValue: new Float32Array([-1, 2, 4]),
        metallicValue: -0.5,
        roughnessValue: 1.5,
        alphaMode: "blend",
        alphaCutoff: -0.25,
        normalScale: -2,
        occlusionStrength: 3,
        doubleSided: false,
      });
      const beforeScalarPatch = scene.getMaterial(id);
      scene.setMaterial(id, { roughnessValue: 0.125 });
      expect(scene.getMaterial(id)).toEqual({ ...beforeScalarPatch, roughnessValue: 0.125 });
      for (const alphaMode of ["opaque", "mask", "blend"] as const) {
        scene.setMaterial(id, { alphaMode });
        expect(scene.getMaterial(id).alphaMode).toBe(alphaMode);
      }
    } finally {
      scene.delete();
    }
  });

  test("material updates survive collection growth from scalar getters", () => {
    const scene = new lagrange.scene.Scene();
    try {
      const id = scene.addMaterial({ name: "target", roughnessValue: 0.75 });
      scene.setMaterial(id, {
        get roughnessValue() {
          for (let i = 0; i < 64; i++) scene.addMaterial();
          return 0.5;
        },
      });
      expect(scene.getMaterial(id).roughnessValue).toBe(0.5);
      expect(scene.getMaterial(id).name).toBe("target");
    } finally {
      scene.delete();
    }
  });

  test("texture updates merge into current values after all options getters finish", () => {
    const scene = loadTexturedMaterial();
    try {
      scene.setMaterial(0, {
        get baseColorTexture() {
          for (let i = 0; i < 64; i++) scene.addMaterial();
          return {
            get texcoord() {
              for (let i = 0; i < 64; i++) scene.addMaterial();
              scene.setMaterial(0, { baseColorTexture: { index: 0 } });
              return 7;
            },
          };
        },
        // This later getter also edits a texture that has already been parsed.
        get occlusionTexture() {
          scene.setMaterial(0, { baseColorTexture: { index: null } });
          return { texcoord: 9 };
        },
      });
      expect(scene.getMaterial(0).baseColorTexture).toEqual({ index: null, texcoord: 7 });
      expect(scene.getMaterial(0).occlusionTexture).toEqual({ index: 0, texcoord: 9 });
    } finally {
      scene.delete();
    }
  });

  test("material snapshots own their vectors and all nested texture objects", () => {
    const scene = loadTexturedMaterial();
    try {
      const before = scene.getMaterial(0);
      const snapshot = scene.getMaterial(0);
      expect(snapshot.baseColorValue).toBeInstanceOf(Float32Array);
      expect(snapshot.emissiveValue).toBeInstanceOf(Float32Array);
      snapshot.name = "not a scene edit";
      snapshot.baseColorValue.fill(9);
      snapshot.emissiveValue.fill(8);
      for (const field of textureFields) {
        snapshot[field].index = null;
        snapshot[field].texcoord = 99;
      }
      expect(scene.getMaterial(0)).toEqual(before);
      scene.setMaterial(0, { baseColorValue: [1, 0, 0, 1], normalTexture: null });
      expect(snapshot.baseColorValue).toEqual(new Float32Array([9, 9, 9, 9]));
      expect(snapshot.normalTexture).toEqual({ index: null, texcoord: 99 });
    } finally {
      scene.delete();
    }
  });

  test("loaded texture references support nested partial patches and explicit clearing", () => {
    const scene = loadTexturedMaterial();
    try {
      for (const [i, field] of textureFields.entries()) {
        expect(scene.getMaterial(0)[field]).toEqual({ index: 0, texcoord: i });
        scene.setMaterial(0, { [field]: { texcoord: i + 5 } });
        expect(scene.getMaterial(0)[field]).toEqual({ index: 0, texcoord: i + 5 });
        scene.setMaterial(0, { [field]: { index: null } });
        expect(scene.getMaterial(0)[field]).toEqual({ index: null, texcoord: i + 5 });
        scene.setMaterial(0, { [field]: { index: 0 } });
        expect(scene.getMaterial(0)[field]).toEqual({ index: 0, texcoord: i + 5 });
        scene.setMaterial(0, { [field]: {} });
        expect(scene.getMaterial(0)[field]).toEqual({ index: 0, texcoord: i + 5 });
        scene.setMaterial(0, { [field]: null });
        expect(scene.getMaterial(0)[field]).toEqual({ index: null, texcoord: 0 });
      }
      const added = scene.addMaterial({
        name: "reuses loaded texture",
        baseColorTexture: { index: 0, texcoord: 7 },
      });
      expect(scene.getMaterial(added).baseColorTexture).toEqual({ index: 0, texcoord: 7 });
    } finally {
      scene.delete();
    }
  });

  test("partial patches preserve unknown native material extensions through serialization", () => {
    const scene = loadTexturedMaterial();
    try {
      scene.setMaterial(0, { name: "changed", roughnessValue: 0.25 });
      scene.setMaterial(0, { baseColorValue: [0.5, 0.25, 0.125, 1], normalTexture: null });
      const bytes = lagrange.io.saveSceneToBuffer(scene, "gltf", { quiet: true });
      const serialized = JSON.parse(new TextDecoder().decode(bytes));
      expect(serialized.materials[0].name).toBe("changed");
      expect(serialized.materials[0].pbrMetallicRoughness.roughnessFactor).toBe(0.25);
      expect(serialized.materials[0].extensions.VENDOR_material_test).toEqual({
        marker: "preserved",
        weights: [1, 2, 3],
      });
    } finally {
      scene.delete();
    }
  });

  test("invalid material patches and additions are atomic", () => {
    const scene = loadTexturedMaterial();
    try {
      const before = scene.getMaterial(0);
      const invalidPatches: unknown[] = [
        { name: 12 },
        { baseColorValue: [1, 2, 3] },
        { baseColorValue: [1, 2, 3, 4, 5] },
        { baseColorValue: { 0: 1, 1: 2, 2: 3, 3: 4, length: 2 ** 32 + 4 } },
        { baseColorValue: { 0: 1, 1: 2, 2: 3, 3: 4, length: 4.5 } },
        { baseColorValue: null },
        { baseColorValue: [1, 2, NaN, 1] },
        { emissiveValue: [0, 1] },
        { emissiveValue: [0, Infinity, 1] },
        { emissiveValue: [0, "1", 1] },
        { metallicValue: Infinity },
        { roughnessValue: NaN },
        { roughnessValue: 1e100 },
        { metallicValue: "0.5" },
        { alphaMode: "OPAQUE" },
        { alphaMode: 0 },
        { alphaCutoff: NaN },
        { normalScale: -Infinity },
        { occlusionStrength: Infinity },
        { doubleSided: 1 },
        { baseColorTexture: 0 },
        { normalTexture: { texcoord: -1 } },
        { normalTexture: { texcoord: 0.5 } },
        { normalTexture: { texcoord: NaN } },
        { normalTexture: { texcoord: Infinity } },
        { normalTexture: { texcoord: 2 ** 31 } },
        { normalTexture: { texcoord: "0" } },
      ];
      for (const index of [-1, 0.5, NaN, Infinity, 1, 2 ** 32, "0", true]) {
        for (const field of textureFields) invalidPatches.push({ [field]: { index } });
      }
      for (const patch of invalidPatches) {
        const options = {
          name: "must not commit",
          metallicValue: 0.125,
          ...patch as Record<string, unknown>,
        } as unknown as MaterialOptions;
        expect(() => scene.setMaterial(0, options)).toThrow();
        expect(scene.getMaterial(0)).toEqual(before);
        expect(() => scene.addMaterial(options)).toThrow();
        expect(scene.getNumMaterials()).toBe(1);
        expect(scene.getMaterial(0)).toEqual(before);
      }
      // A valid field staged early must not commit if a later texture field is invalid.
      expect(() => scene.setMaterial(0, {
        baseColorTexture: { texcoord: 42 },
        occlusionTexture: { index: 1 },
      })).toThrow();
      expect(scene.getMaterial(0)).toEqual(before);
      const next = scene.addMaterial({ name: "valid after failures", roughnessValue: 0.25 });
      expect(next).toBe(1);
      expect(scene.getMaterial(next).roughnessValue).toBe(0.25);
    } finally {
      scene.delete();
    }
  });

  test("material IDs cannot coerce, wrap, or address another material", () => {
    const scene = new lagrange.scene.Scene();
    try {
      const id = scene.addMaterial({ name: "protected", metallicValue: 0.25 });
      const before = scene.getMaterial(id);
      for (const invalid of [-1, 0.5, NaN, Infinity, 1, 2 ** 32, "0", null, true]) {
        const invalidId = invalid as unknown as number;
        expect(() => scene.getMaterial(invalidId)).toThrow();
        expect(() => scene.setMaterial(invalidId, { metallicValue: 0.75 })).toThrow();
        expect(scene.getMaterial(id)).toEqual(before);
      }
    } finally {
      scene.delete();
    }
  });
});

describe("scene material assignment", () => {
  test("flattening remaps actual facet slots to global material IDs per instance", () => {
    const mesh = makeTriangle();
    const scene = new lagrange.scene.Scene();
    let flattened: SurfaceMesh | undefined;
    let storedMesh: SurfaceMesh | undefined;
    try {
      mesh.addVertex([1, 1, 0]);
      mesh.addTriangle(1, 3, 2);
      mesh.createAttribute("material_id", {
        element: "facet",
        usage: "scalar",
        data: new Uint32Array([0, 1]),
      });
      const red = scene.addMaterial({ name: "red", baseColorValue: [1, 0, 0, 1] });
      const green = scene.addMaterial({ name: "green", baseColorValue: [0, 1, 0, 1] });
      const blue = scene.addMaterial({ name: "blue", baseColorValue: [0, 0, 1, 1] });
      const meshId = scene.addMesh(mesh);
      scene.addNode({ meshes: [{ mesh: meshId, materials: [blue, red] }] });
      scene.addNode({ meshes: [{ mesh: meshId, materials: [green, blue] }] });
      flattened = lagrange.scene.sceneToMesh(scene);
      expect(flattened.getNumFacets()).toBe(4);
      const ids = facetMaterialIds(flattened);
      expect(ids).toEqual([blue, red, green, blue]);
      expect(ids.map((id) => scene.getMaterial(id).name)).toEqual(["blue", "red", "green", "blue"]);
      expect(facetMaterialIds(mesh)).toEqual([0, 1]);
      storedMesh = scene.getMesh(meshId);
      expect(facetMaterialIds(storedMesh)).toEqual([0, 1]);
    } finally {
      storedMesh?.delete();
      flattened?.delete();
      scene.delete();
      mesh.delete();
    }
  });

  for (const format of ["gltf", "glb"] as const) {
    test(`edited PBR materials and instance assignments survive ${format} round-trips`, () => {
      const mesh = makeTriangle();
      const scene = new lagrange.scene.Scene();
      let restored: Scene | undefined;
      let flattened: SurfaceMesh | undefined;
      try {
        const red = scene.addMaterial({ name: "red", baseColorValue: [1, 0, 0, 1] });
        scene.addMaterial({ name: "unused green", baseColorValue: [0, 1, 0, 1] });
        const blue = scene.addMaterial({ name: "blue", baseColorValue: [0, 0, 1, 1] });
        scene.setMaterial(blue, {
          name: "edited blue",
          baseColorValue: [0.125, 0.25, 0.75, 0.5],
          emissiveValue: [0.25, 0.5, 0.125],
          metallicValue: 0.25,
          roughnessValue: 0.5,
          alphaMode: "mask",
          alphaCutoff: 0.125,
          doubleSided: true,
        });
        const meshId = scene.addMesh(mesh);
        scene.addNode({ name: "blue instance", meshes: [{ mesh: meshId, materials: [blue] }] });
        scene.addNode({ name: "red instance", meshes: [{ mesh: meshId, materials: [red] }] });
        const bytes = lagrange.io.saveSceneToBuffer(scene, format, { quiet: true });
        restored = lagrange.io.loadSceneFromBuffer(bytes, { quiet: true });
        expect(restored.getNumMaterials()).toBe(3);
        expect(restored.getMaterial(blue)).toEqual(scene.getMaterial(blue));
        expect(restored.getMaterial(red)).toEqual(scene.getMaterial(red));
        const assignments: Record<string, string[]> = {};
        for (let id = 0; id < restored.getNumNodes(); ++id) {
          const node = restored.getNode(id);
          assignments[node.name] = node.meshes.flatMap((instance) =>
            instance.materials.map((materialId) => restored!.getMaterial(materialId).name),
          );
        }
        expect(assignments["blue instance"]).toEqual(["edited blue"]);
        expect(assignments["red instance"]).toEqual(["red"]);
        flattened = lagrange.scene.sceneToMesh(restored);
        expect(flattened.getNumFacets()).toBe(2);
        expect(facetMaterialIds(flattened)).toEqual([blue, red]);
      } finally {
        flattened?.delete();
        restored?.delete();
        scene.delete();
        mesh.delete();
      }
    });
  }
});
