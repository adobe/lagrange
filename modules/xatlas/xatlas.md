# lagrange::xatlas

@defgroup module-xatlas Xatlas Module
@brief UV unwrapping and repacking with xatlas.

Open-source UV unwrap and repack module backed by [xatlas](https://github.com/jpcy/xatlas).

## Capabilities

- UV unwrapping for triangle meshes and scenes: [unwrap_mesh](@ref lagrange::xatlas::unwrap_mesh), [unwrap_scene](@ref lagrange::xatlas::unwrap_scene).
- Repacking of existing UV islands: [repack_mesh](@ref lagrange::xatlas::repack_mesh), [repack_scene](@ref lagrange::xatlas::repack_scene).
- Configure charting and packing: [UnwrapOptions](@ref lagrange::xatlas::UnwrapOptions), [RepackOptions](@ref lagrange::xatlas::RepackOptions), [SceneOptions](@ref lagrange::xatlas::SceneOptions).
- Optional progress reporting and cancellation in C++ via the `notification_func` and `cancel` parameters of [unwrap_mesh](@ref lagrange::xatlas::unwrap_mesh) and [repack_mesh](@ref lagrange::xatlas::repack_mesh).

## Limitations

- Triangle meshes only; non-triangle inputs throw `lagrange::Error`.
- Custom `ParameterizeFunc` is not exposed; xatlas's default LSCM is used.
- Progress / cancellation are not exposed in the Python bindings.
