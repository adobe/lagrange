import { describe, test, expect, beforeAll } from "vitest";
import { loadLagrange } from "../src/loadLagrange.node.js";
import type { Lagrange } from "../src/types.js";

var lagrange: Lagrange;

beforeAll(async () => {
  lagrange = await loadLagrange();
});

describe("sampling", () => {
  test("sampleUniform returns requested number of samples", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const result = lagrange.sampling.sampleUniform(mesh, { numSamples: 100 });
    expect(result.numSamples).toBe(100);
    expect(result.positions.length).toBe(300); // 100 * 3
    expect(result.facetIndices.length).toBe(100);
    mesh.delete();
  });

  test("sampleUniform with seed produces deterministic results", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const a = lagrange.sampling.sampleUniform(mesh, { numSamples: 50, seed: 42 });
    const b = lagrange.sampling.sampleUniform(mesh, { numSamples: 50, seed: 42 });
    expect(a.positions[0]).toBe(b.positions[0]);
    expect(a.positions[1]).toBe(b.positions[1]);
    mesh.delete();
  });

  test("sampleBlueNoise returns samples with densities", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const result = lagrange.sampling.sampleBlueNoise(mesh, { numSamples: 50 });
    expect(result.numSamples).toBeGreaterThan(0);
    expect(result.positions.length).toBe(result.numSamples * 3);
    expect(result.facetIndices.length).toBe(result.numSamples);
    expect(result.densities.length).toBe(result.numSamples);
    mesh.delete();
  });

  test("sampleEpsilon throws for non-positive epsilon", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    expect(() => lagrange.sampling.sampleEpsilon(mesh, "lfs", 0)).toThrow();
    mesh.delete();
  });

  test("sampleEpsilon throws when the lfs attribute is missing", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    expect(() => lagrange.sampling.sampleEpsilon(mesh, "lfs", 0.5)).toThrow();
    mesh.delete();
  });
});
