BVH Module
================

@namespace lagrange::bvh

@defgroup module-bvh BVH Module
@brief Bounding Volume Hierarchy (BVH) construction and traversal.

- Spatial queries on triangle meshes and edge graphs: [TriangleAABBTree](@ref lagrange::bvh::TriangleAABBTree), [EdgeAABBTree](@ref lagrange::bvh::EdgeAABBTree).
- Mesh distance metrics: [compute_mesh_distances](@ref lagrange::bvh::compute_mesh_distances), [compute_hausdorff](@ref lagrange::bvh::compute_hausdorff), [compute_chamfer](@ref lagrange::bvh::compute_chamfer).
- UV overlap and triangle intersection detection: [compute_uv_overlap](@ref lagrange::bvh::compute_uv_overlap), [compute_intersecting_pairs](@ref lagrange::bvh::compute_intersecting_pairs).
- Mesh cleanup: [weld_vertices](@ref lagrange::bvh::weld_vertices), [remove_interior_shells](@ref lagrange::bvh::remove_interior_shells), [resolve_tjunctions](@ref lagrange::bvh::resolve_tjunctions).
