I/O Module
============

@namespace lagrange::io
@brief Mesh input/output.

@defgroup module-io IO Module
@brief Mesh input/output.

Quick links
-----------

- [load_mesh](@ref lagrange::io::load_mesh)
  - [load_mesh_obj](@ref lagrange::io::load_mesh_obj)
  - [load_mesh_ply](@ref lagrange::io::load_mesh_ply)
- [save_mesh](@ref lagrange::io::save_mesh)
  - [save_mesh_obj](@ref lagrange::io::save_mesh_obj)
  - [save_mesh_ply](@ref lagrange::io::save_mesh_ply)

PCD format support
------------------

PCD input and output are always available in CMake and MetaBuild builds through the public
[pcdio](https://github.com/adobe/pcdio) dependency.
