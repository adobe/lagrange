# lagrange::xatlas

Open-source UV unwrap and repack module backed by [xatlas](https://github.com/jpcy/xatlas).

## Capabilities

- `unwrap_mesh()` / `unwrap_scene()` — segment + parameterize + pack a triangle mesh / scene.
- `repack_mesh()` / `repack_scene()` — repack existing UV islands.
- Optional progress and cancellation via the `notification_func` and `cancel` parameters.

## Limitations

- Triangle meshes only; non-triangle inputs throw `lagrange::Error`.
- Custom `ParameterizeFunc` is not exposed; xatlas's default LSCM is used.
- Progress / cancellation are not exposed in the Python bindings.
