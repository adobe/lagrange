/*
 * Copyright 2017 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/raycasting/api.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/utils/BitField.h>
#include <lagrange/utils/value_ptr.h>

#include <Eigen/Core>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <variant>

namespace lagrange::raycasting {

///
/// Shared base struct for ray and closest point hits.
///
struct HitBase
{
    /// Index of the mesh that was hit.
    uint32_t mesh_index = invalid<uint32_t>();

    /// Index of the instance that was hit (relative to the source mesh).
    uint32_t instance_index = invalid<uint32_t>();

    /// Index of the facet that was hit.
    uint32_t facet_index = invalid<uint32_t>();

    /// Barycentric coordinates of the hit point within the hit facet. Given a triangle (p1, p2,
    /// p3), the barycentric coordinates (u, v) are such that the surface point is represented by p
    /// = (1 - u - v) * p1 + u * p2 + v * p3.
    Eigen::Vector2f barycentric_coord = Eigen::Vector2f::Zero();

    /// World-space position of the hit point.
    Eigen::Vector3f position = Eigen::Vector3f::Zero();
};

///
/// Result of a single closest point query.
///
struct ClosestPointHit : public HitBase
{
    /// Distance from the query point to the closest point on the surface.
    float distance = std::numeric_limits<float>::infinity();
};

///
/// Result of a single-ray intersection query.
///
struct RayHit : public HitBase
{
    /// Parametric distance along the ray (t value).
    float ray_depth = 0;

    /// Unnormalized geometric normal at the hit point.
    Eigen::Vector3f normal = Eigen::Vector3f::Zero();
};

///
/// Shared base struct for multi-ray and multi-point hits.
///
/// @tparam     N     Number of rays/points in the packet (e.g., 4, 8, or 16).
///
template <size_t N>
struct HitBaseN
{
    /// Bitmask indicating which rays in the packet hit something (1 bit per ray).
    uint32_t valid_mask = 0;

    /// Index of the mesh that was hit.
    Eigen::Vector<uint32_t, N> mesh_indices =
        Eigen::Vector<uint32_t, N>::Constant(invalid<uint32_t>());

    /// Index of the instance that was hit (relative to the source mesh).
    Eigen::Vector<uint32_t, N> instance_indices =
        Eigen::Vector<uint32_t, N>::Constant(invalid<uint32_t>());

    /// Index of the facet that was hit.
    Eigen::Vector<uint32_t, N> facet_indices =
        Eigen::Vector<uint32_t, N>::Constant(invalid<uint32_t>());

    /// Barycentric coordinates of the hit points within the hit facets. Given a triangle (p1, p2,
    /// p3), the barycentric coordinates (u, v) are such that the surface point is represented by p
    /// = (1 - u - v) * p1 + u * p2 + v * p3.
    Eigen::Matrix<float, 2, N> barycentric_coords = Eigen::Matrix<float, 2, N>::Zero();

    /// World-space position of the hit point.
    Eigen::Matrix<float, 3, N> positions = Eigen::Matrix<float, 3, N>::Zero();

    bool is_valid(size_t i) const { return (valid_mask & (1u << i)) != 0; }
};

///
/// Result of a multi-point closest point query.
///
/// @tparam     N     Number of points in the packet (e.g., 4, 8, or 16).
///
template <size_t N>
struct ClosestPointHitN : public HitBaseN<N>
{
    /// Distance from the query points to the closest points on the surface.
    Eigen::Vector<float, N> distances =
        Eigen::Vector<float, N>::Constant(std::numeric_limits<float>::infinity());
};

///
/// Result of a multi-ray intersection query.
///
/// @tparam     N     Number of rays in the packet (e.g., 4, 8, or 16).
///
template <size_t N>
struct RayHitN : public HitBaseN<N>
{
    /// Parametric distance along the ray (t value).
    Eigen::Vector<float, N> ray_depths = Eigen::Vector<float, N>::Zero();

    /// Unnormalized geometric normal at the hit point.
    Eigen::Matrix<float, 3, N> normals = Eigen::Matrix<float, 3, N>::Zero();
};

/// Flags for configuring the ray caster and the underlying Embree scene. These flags are passed to
/// the RayCaster constructor and control how the BVH is built and traversed.
enum class SceneFlags {
    /// No special behavior.
    None = 0,

    /// Indicates that the scene will be updated frequently.
    Dynamic = 1 << 0,

    /// Use a more compact BVH layout that may be faster to build but slower to traverse.
    Compact = 1 << 1,

    /// Use a more robust BVH traversal algorithm that is slower but less likely to miss hits due to
    /// numerical issues.
    Robust = 1 << 2,

    /// Enable user-defined intersection and occlusion filters.
    Filter = 1 << 3,
};

///
/// Quality levels for BVH construction. Higher quality typically results in faster ray queries but
/// longer build times.
///
enum class BuildQuality {
    /// Fastest build time, lowest BVH quality.
    Low = 0,

    /// Moderate build time and BVH quality.
    Medium = 1,

    /// Slowest build time, highest BVH quality.
    High = 2,
};

///
/// A ray caster built on top of Embree that operates directly on SurfaceMesh and SimpleScene
/// objects. Supports single-ray and SIMD ray-packet queries (packets of 4, 8, and 16 rays) for
/// efficient vectorized intersection and occlusion testing.
///
/// @note       All internal computation is performed in single-precision (float32). When a
///             SurfaceMesh<double, ...> is provided, vertex positions and transforms are converted
///             to float, which may result in precision loss for meshes with large coordinates or
///             fine geometric detail.
///
/// @note       Query methods (cast, occluded, closest_point, closest_vertex and their packet
///             variants) are thread-safe and may be called concurrently from multiple threads.
///             However, scene update methods (add_mesh, add_instance, add_scene, update_mesh,
///             update_vertices, update_transform, update_visibility, commit_updates, and filter
///             setters) are **not** thread-safe and must not be called concurrently with each other
///             or with query methods.
///
class RayCaster
{
public:
    /// 3D point type.
    using Pointf = Eigen::Vector3f;

    /// 3D direction type.
    using Directionf = Eigen::Vector3f;

    /// 4x4 affine transform type (column-major).
    template <typename Scalar>
    using Affine = Eigen::Transform<Scalar, 3, Eigen::Affine>;

    /// Hit result type.
    using Hit = RayHit;

    /// @name Batch types for ray packets of size 4.
    /// @{
    using Point4f = Eigen::Matrix<float, 4, 3, Eigen::RowMajor>;
    using Direction4f = Eigen::Matrix<float, 4, 3, Eigen::RowMajor>;
    using Float4 = Eigen::Vector<float, 4>;
    using Mask4 = Eigen::Vector<bool, 4>;
    using RayHit4 = RayHitN<4>;
    using ClosestPointHit4 = ClosestPointHitN<4>;
    /// @}

    /// @name Batch types for ray packets of size 8.
    /// @{
    using Point8f = Eigen::Matrix<float, 8, 3, Eigen::RowMajor>;
    using Direction8f = Eigen::Matrix<float, 8, 3, Eigen::RowMajor>;
    using Float8 = Eigen::Vector<float, 8>;
    using Mask8 = Eigen::Vector<bool, 8>;
    using RayHit8 = RayHitN<8>;
    using ClosestPointHit8 = ClosestPointHitN<8>;
    /// @}

    /// @name Batch types for ray packets of size 16.
    /// @{
    using Point16f = Eigen::Matrix<float, 16, 3, Eigen::RowMajor>;
    using Direction16f = Eigen::Matrix<float, 16, 3, Eigen::RowMajor>;
    using Float16 = Eigen::Vector<float, 16>;
    using Mask16 = Eigen::Vector<bool, 16>;
    using RayHit16 = RayHitN<16>;
    using ClosestPointHit16 = ClosestPointHitN<16>;
    /// @}

public:
    /// @name Construction
    /// @{

    ///
    /// Construct a RayCaster with the given Embree scene configuration.
    ///
    /// @param[in]  scene_flags     Embree scene flags.
    /// @param[in]  build_quality   Embree BVH build quality.
    ///
    explicit RayCaster(
        BitField<SceneFlags> scene_flags = SceneFlags::Robust,
        BuildQuality build_quality = BuildQuality::Medium);

    /// Destructor.
    ~RayCaster();

    /// Move constructor.
    RayCaster(RayCaster&& other) noexcept;

    /// Move assignment.
    RayCaster& operator=(RayCaster&& other) noexcept;

    /// Non-copyable.
    RayCaster(const RayCaster&) = delete;

    /// Non-copyable.
    RayCaster& operator=(const RayCaster&) = delete;

    /// @}

    /// @name Scene population
    /// @{

    ///
    /// Add a single mesh to the scene. The mesh must be a triangle mesh. If the optional transform
    /// is provided, a single instance of the mesh is created with that transform. By default, the
    /// mesh is added with an identity transform (i.e., a single instance with no transformation).
    ///
    /// @note       When using Scalar==float or Index==uint32_t, the RayCaster is able to reuse the
    ///             memory buffers for the BVH construction through copy-on-write semantics. If a
    ///             different scalar or index type is used, a copy will be created internally with
    ///             the correct data type.
    ///
    /// @param[in]  mesh       Triangle mesh to add. The mesh data is moved to the raycaster and
    ///                        buffers are converted to the appropriate types.
    /// @param[in]  transform  Optional affine transformation applied to this instance. If not
    ///                        provided, the mesh is not instantiated.
    ///
    /// @tparam     Scalar     Mesh scalar type.
    /// @tparam     Index      Mesh index type.
    ///
    /// @return     The index of the source mesh in the raycaster scene.
    ///
    template <typename Scalar, typename Index>
    uint32_t add_mesh(
        SurfaceMesh<Scalar, Index> mesh,
        const std::optional<Affine<Scalar>>& transform = Affine<Scalar>::Identity());

    ///
    /// Add a single instance of an existing source mesh to the scene with a given affine transform.
    ///
    /// @note       When using Scalar==float or Index==uint32_t, the RayCaster is able to reuse the
    ///             memory buffers for the BVH construction through copy-on-write semantics. If a
    ///             different scalar or index type is used, a copy will be created internally with
    ///             the correct data type.
    ///
    /// @param[in]  mesh_index  Index of the source mesh.
    /// @param[in]  transform   Affine transformation applied to this instance.
    ///
    /// @return     The index of the local instance in the raycaster scene (relative to other
    ///             instances from the same source mesh).
    ///
    uint32_t add_instance(uint32_t mesh_index, const Eigen::Affine3f& transform);

    ///
    /// Add all meshes and instances from a SimpleScene.
    ///
    /// @param[in]  simple_scene  Scene containing meshes and their instances. The scene data is
    ///                           moved to the raycaster and mesh buffers are converted to the
    ///                           appropriate types.
    ///
    /// @tparam     Scalar        Scene scalar type.
    /// @tparam     Index         Scene index type.
    ///
    template <typename Scalar, typename Index>
    void add_scene(scene::SimpleScene<Scalar, Index, 3> simple_scene);

    ///
    /// Notify the raycaster that all pending updates have been made and the BVH can be rebuilt.
    /// This must be called explicitly by the user before doing any ray-tracing or closest point
    /// queries.
    ///
    void commit_updates();

    /// @}

    /// @name Scene modification
    /// @{

    ///
    /// Replace a mesh in the scene. All instances of the old mesh will reference the new mesh.
    /// Triggers a full BVH rebuild.
    ///
    /// @param[in]  mesh_index  Index of the mesh to replace.
    /// @param[in]  mesh        New triangle mesh.
    ///
    /// @tparam     Scalar      Mesh scalar type.
    /// @tparam     Index       Mesh index type.
    ///
    template <typename Scalar, typename Index>
    void update_mesh(uint32_t mesh_index, const SurfaceMesh<Scalar, Index>& mesh);

    ///
    /// Notify the raycaster that vertices of a mesh have been modified externally. The number and
    /// order of vertices must not change.
    ///
    /// @note       The current API does not allow updating vertex positions without creating a copy
    ///             of the vertices positions (because of copy-on-write and value semantics). If
    ///             this is necessary, we should consider adding a separate method for that, based
    ///             on intended use cases.
    ///
    /// @param[in]  mesh_index  Index of the mesh whose vertices changed.
    /// @param[in]  mesh        The modified mesh with updated vertex positions.
    ///
    /// @tparam     Scalar      Mesh scalar type.
    /// @tparam     Index       Mesh index type.
    ///
    template <typename Scalar, typename Index>
    void update_vertices(uint32_t mesh_index, const SurfaceMesh<Scalar, Index>& mesh);

    ///
    /// Notify the raycaster that vertices of a mesh have been modified externally. The number and
    /// order of vertices must not change.
    ///
    /// @param[in]  mesh_index  Index of the mesh whose vertices changed.
    /// @param[in]  vertices    Updated vertex positions.
    ///
    /// @tparam     Scalar      Positions scalar type.
    ///
    template <typename Scalar>
    void update_vertices(uint32_t mesh_index, span<const Scalar> vertices);

    ///
    /// Get the affine transform of a given mesh instance.
    ///
    /// @param[in]  mesh_index      Index of the source mesh.
    /// @param[in]  instance_index  Local instance index relative to other instances of the same
    ///                             source mesh.
    ///
    /// @return     The current affine transform of the instance.
    ///
    Eigen::Affine3f get_transform(uint32_t mesh_index, uint32_t instance_index) const;

    ///
    /// Update the affine transform of a given mesh instance.
    ///
    /// @param[in]  mesh_index      Index of the source mesh.
    /// @param[in]  instance_index  Local instance index relative to other instances of the same
    ///                             source mesh.
    /// @param[in]  transform       New affine transform to apply.
    ///
    void update_transform(
        uint32_t mesh_index,
        uint32_t instance_index,
        const Eigen::Affine3f& transform);

    ///
    /// Get the visibility flag of a given mesh instance.
    ///
    /// @param[in]  mesh_index      Index of the source mesh.
    /// @param[in]  instance_index  Local instance index relative to the source mesh.
    ///
    /// @return     True if the instance is visible.
    ///
    bool get_visibility(uint32_t mesh_index, uint32_t instance_index);

    ///
    /// Update the visibility of a given mesh instance.
    ///
    /// @param[in]  mesh_index      Index of the source mesh.
    /// @param[in]  instance_index  Local instance index relative to the source mesh.
    /// @param[in]  visible         True to make the instance visible, false to hide it.
    ///
    void update_visibility(uint32_t mesh_index, uint32_t instance_index, bool visible);

    /// @}

    /// @name Filtering
    /// @{

    ///
    /// Get the intersection filter function currently bound to a given mesh.
    ///
    /// @param[in]  mesh_index  Index of the mesh.
    ///
    /// @return     The current intersection filter, or an empty function if none is set.
    ///
    auto get_intersection_filter(uint32_t mesh_index) const
        -> std::function<bool(uint32_t instance_index, uint32_t facet_index)>;

    ///
    /// Set an intersection filter that is called for every hit on (every instance of) a mesh during
    /// an intersection query. The filter should return true to accept the hit, false to reject it.
    ///
    /// @param[in]  mesh_index  Index of the mesh.
    /// @param[in]  filter      Filter function, or an empty function to disable filtering.
    ///
    void set_intersection_filter(
        uint32_t mesh_index,
        std::function<bool(uint32_t instance_index, uint32_t facet_index)>&& filter);

    ///
    /// Get the occlusion filter function currently bound to a given mesh.
    ///
    /// @param[in]  mesh_index  Index of the mesh.
    ///
    /// @return     The current occlusion filter, or an empty function if none is set.
    ///
    auto get_occlusion_filter(uint32_t mesh_index) const
        -> std::function<bool(uint32_t instance_index, uint32_t facet_index)>;

    ///
    /// Set an occlusion filter that is called for every hit on (every instance of) a mesh during an
    /// occlusion query. The filter should return true to accept the hit, false to reject it.
    ///
    /// @param[in]  mesh_index  Index of the mesh.
    /// @param[in]  filter      Filter function, or an empty function to disable filtering.
    ///
    void set_occlusion_filter(
        uint32_t mesh_index,
        std::function<bool(uint32_t instance_index, uint32_t facet_index)>&& filter);

    /// @}

    /// @name Closest point queries
    /// @{

    ///
    /// Find the closest point on the scene to a query point.
    ///
    /// @param[in]  query_point  The query point in world space.
    ///
    /// @return     A hit result if a closest point was found, or std::nullopt otherwise.
    ///
    std::optional<ClosestPointHit> closest_point(const Pointf& query_point) const;

    ///
    /// Find the closest point on the scene for a packet of up to 4 query points.
    ///
    /// @param[in]  query_points  Query points, one per row (4x3).
    /// @param[in]  active        Either a per-point activity mask (true = active, false =
    ///                           inactive), or the number of active points starting from the top
    ///                           (e.g. 3 means points 0, 1, and 2 are active, point 3 and beyond
    ///                           are inactive).
    ///
    /// @return     Closest point results for each query point in the packet, with a bitmask
    ///             indicating valid results.
    ///
    ClosestPointHit4 closest_point4(
        const Point4f& query_points,
        const std::variant<Mask4, size_t>& active) const;

    ///
    /// Find the closest point on the scene for a packet of up to 8 query points.
    ///
    /// @param[in]  query_points  Query points, one per row (8x3).
    /// @param[in]  active        Either a per-point activity mask (true = active, false =
    ///                           inactive), or the number of active points starting from the top
    ///                           (e.g. 3 means points 0, 1, and 2 are active, point 3 and beyond
    ///                           are inactive).
    ///
    /// @return     Closest point results for each query point in the packet, with a bitmask
    ///             indicating valid results.
    ///
    ClosestPointHit8 closest_point8(
        const Point8f& query_points,
        const std::variant<Mask8, size_t>& active) const;

    ///
    /// Find the closest point on the scene for a packet of up to 16 query points.
    ///
    /// @param[in]  query_points  Query points, one per row (16x3).
    /// @param[in]  active        Either a per-point activity mask (true = active, false =
    ///                           inactive), or the number of active points starting from the top
    ///                           (e.g. 3 means points 0, 1, and 2 are active, point 3 and beyond
    ///                           are inactive).
    ///
    /// @return     Closest point results for each query point in the packet, with a bitmask
    ///             indicating valid results.
    ///
    ClosestPointHit16 closest_point16(
        const Point16f& query_points,
        const std::variant<Mask16, size_t>& active) const;

    /// @}

    /// @name Closest vertex queries
    /// @{

    ///
    /// Find the closest vertex on the scene to a query point. Unlike closest_point(), this snaps
    /// the result to the nearest triangle vertex rather than returning the closest surface point.
    ///
    /// @param[in]  query_point  The query point in world space.
    ///
    /// @return     A hit result if a closest vertex was found, or std::nullopt otherwise. The
    ///             position field contains the snapped vertex position, and the barycentric
    ///             coordinates will have exactly one component equal to 1.
    ///
    std::optional<ClosestPointHit> closest_vertex(const Pointf& query_point) const;

    ///
    /// Find the closest vertex on the scene for a packet of up to 4 query points.
    ///
    /// @param[in]  query_points  Query points, one per row (4x3).
    /// @param[in]  active        Either a per-point activity mask (true = active, false =
    ///                           inactive), or the number of active points starting from the top
    ///                           (e.g. 3 means points 0, 1, and 2 are active, point 3 and beyond
    ///                           are inactive).
    ///
    /// @return     Closest vertex results for each query point in the packet, with a bitmask
    ///             indicating valid results.
    ///
    ClosestPointHit4 closest_vertex4(
        const Point4f& query_points,
        const std::variant<Mask4, size_t>& active) const;

    ///
    /// Find the closest vertex on the scene for a packet of up to 8 query points.
    ///
    /// @param[in]  query_points  Query points, one per row (8x3).
    /// @param[in]  active        Either a per-point activity mask (true = active, false =
    ///                           inactive), or the number of active points starting from the top
    ///                           (e.g. 3 means points 0, 1, and 2 are active, point 3 and beyond
    ///                           are inactive).
    ///
    /// @return     Closest vertex results for each query point in the packet, with a bitmask
    ///             indicating valid results.
    ///
    ClosestPointHit8 closest_vertex8(
        const Point8f& query_points,
        const std::variant<Mask8, size_t>& active) const;

    ///
    /// Find the closest vertex on the scene for a packet of up to 16 query points.
    ///
    /// @param[in]  query_points  Query points, one per row (16x3).
    /// @param[in]  active        Either a per-point activity mask (true = active, false =
    ///                           inactive), or the number of active points starting from the top
    ///                           (e.g. 3 means points 0, 1, and 2 are active, point 3 and beyond
    ///                           are inactive).
    ///
    /// @return     Closest vertex results for each query point in the packet, with a bitmask
    ///             indicating valid results.
    ///
    ClosestPointHit16 closest_vertex16(
        const Point16f& query_points,
        const std::variant<Mask16, size_t>& active) const;

    /// @}


    /// @name Single-ray queries
    /// @{

    ///
    /// Cast a single ray and find the closest intersection.
    ///
    /// @param[in]  origin     Ray origin.
    /// @param[in]  direction  Ray direction (does not need to be normalized).
    /// @param[in]  tmin       Minimum parametric distance.
    /// @param[in]  tmax       Maximum parametric distance.
    ///
    /// @return     Hit result if an intersection was found, or std::nullopt otherwise.
    ///
    std::optional<RayHit> cast(
        const Pointf& origin,
        const Directionf& direction,
        float tmin = 0,
        float tmax = std::numeric_limits<float>::infinity()) const;

    ///
    /// Test whether a single ray hits anything in the scene (occlusion query).
    ///
    /// @param[in]  origin     Ray origin.
    /// @param[in]  direction  Ray direction (does not need to be normalized).
    /// @param[in]  tmin       Minimum parametric distance.
    /// @param[in]  tmax       Maximum parametric distance.
    ///
    /// @return     True if the ray is occluded (hits something).
    ///
    bool occluded(
        const Pointf& origin,
        const Directionf& direction,
        float tmin = 0,
        float tmax = std::numeric_limits<float>::infinity()) const;

    /// @}

    /// @name Ray packet queries (4-wide SIMD)
    /// @{

    ///
    /// Cast a packet of up to 4 rays and find the closest intersections.
    ///
    /// @param[in]  origins     Ray origins, one per row (4x3).
    /// @param[in]  directions  Ray directions, one per row (4x3).
    /// @param[in]  active      Either a per-ray activity mask (true = active, false = inactive), or
    ///                         the number of active rays starting from the top (e.g. 3 means rays
    ///                         0, 1, and 2 are active, ray 3 is inactive).
    /// @param[in]  tmin        Per-ray minimum parametric distances.
    /// @param[in]  tmax        Per-ray maximum parametric distances.
    ///
    /// @return     Hit results for each ray in the packet, with a bitmask indicating valid hits.
    ///
    RayHit4 cast4(
        const Point4f& origins,
        const Direction4f& directions,
        const std::variant<Mask4, size_t>& active = size_t(4),
        const Float4& tmin = Float4::Zero(),
        const Float4& tmax = Float4::Constant(std::numeric_limits<float>::infinity())) const;

    ///
    /// Test a packet of up to 4 rays for occlusion.
    ///
    /// @param[in]  origins     Ray origins, one per row (4x3).
    /// @param[in]  directions  Ray directions, one per row (4x3).
    /// @param[in]  active      Either a per-ray activity mask (true = active, false = inactive), or
    ///                         the number of active rays starting from the top (e.g. 3 means rays
    ///                         0, 1, and 2 are active, ray 3 is inactive).
    /// @param[in]  tmin        Per-ray minimum parametric distances.
    /// @param[in]  tmax        Per-ray maximum parametric distances.
    ///
    /// @return     Bitmask of which rays are occluded (bit i set if ray i hit something).
    ///
    uint32_t occluded4(
        const Point4f& origins,
        const Direction4f& directions,
        const std::variant<Mask4, size_t>& active = size_t(4),
        const Float4& tmin = Float4::Zero(),
        const Float4& tmax = Float4::Constant(std::numeric_limits<float>::infinity())) const;

    /// @}

    /// @name Ray packet queries (8-wide SIMD)
    /// @{

    ///
    /// Cast a packet of up to 8 rays and find the closest intersections.
    ///
    /// @param[in]  origins     Ray origins, one per row (8x3).
    /// @param[in]  directions  Ray directions, one per row (8x3).
    /// @param[in]  active      Either a per-ray activity mask (true = active, false = inactive), or
    ///                         the number of active rays starting from the top (e.g. 3 means rays
    ///                         0, 1, and 2 are active, ray 3 and beyond are inactive).
    /// @param[in]  tmin        Per-ray minimum parametric distances.
    /// @param[in]  tmax        Per-ray maximum parametric distances.
    ///
    /// @return     Hit results for each ray in the packet, with a bitmask indicating valid hits.
    ///
    RayHit8 cast8(
        const Point8f& origins,
        const Direction8f& directions,
        const std::variant<Mask8, size_t>& active = size_t(8),
        const Float8& tmin = Float8::Zero(),
        const Float8& tmax = Float8::Constant(std::numeric_limits<float>::infinity())) const;

    ///
    /// Test a packet of up to 8 rays for occlusion.
    ///
    /// @param[in]  origins     Ray origins, one per row (8x3).
    /// @param[in]  directions  Ray directions, one per row (8x3).
    /// @param[in]  active      Either a per-ray activity mask (true = active, false = inactive), or
    ///                         the number of active rays starting from the top (e.g. 3 means rays
    ///                         0, 1, and 2 are active, ray 3 and beyond are inactive).
    /// @param[in]  tmin        Per-ray minimum parametric distances.
    /// @param[in]  tmax        Per-ray maximum parametric distances.
    ///
    /// @return     Bitmask of which rays are occluded (bit i set if ray i hit something).
    ///
    uint32_t occluded8(
        const Point8f& origins,
        const Direction8f& directions,
        const std::variant<Mask8, size_t>& active = size_t(8),
        const Float8& tmin = Float8::Zero(),
        const Float8& tmax = Float8::Constant(std::numeric_limits<float>::infinity())) const;

    /// @}

    /// @name Ray packet queries (16-wide SIMD)
    /// @{

    ///
    /// Cast a packet of up to 16 rays and find the closest intersections.
    ///
    /// @param[in]  origins     Ray origins, one per row (16x3).
    /// @param[in]  directions  Ray directions, one per row (16x3).
    /// @param[in]  active      Either a per-ray activity mask (true = active, false = inactive), or
    ///                         the number of active rays starting from the top (e.g. 3 means rays
    ///                         0, 1, and 2 are active, ray 3 and beyond are inactive).
    /// @param[in]  tmin        Per-ray minimum parametric distances.
    /// @param[in]  tmax        Per-ray maximum parametric distances.
    ///
    /// @return     Hit results for each ray in the packet, with a bitmask indicating valid hits.
    ///
    RayHit16 cast16(
        const Point16f& origins,
        const Direction16f& directions,
        const std::variant<Mask16, size_t>& active = size_t(16),
        const Float16& tmin = Float16::Zero(),
        const Float16& tmax = Float16::Constant(std::numeric_limits<float>::infinity())) const;

    ///
    /// Test a packet of up to 16 rays for occlusion.
    ///
    /// @param[in]  origins     Ray origins, one per row (16x3).
    /// @param[in]  directions  Ray directions, one per row (16x3).
    /// @param[in]  active      Either a per-ray activity mask (true = active, false = inactive), or
    ///                         the number of active rays starting from the top (e.g. 3 means rays
    ///                         0, 1, and 2 are active, ray 3 and beyond are inactive).
    /// @param[in]  tmin        Per-ray minimum parametric distances.
    /// @param[in]  tmax        Per-ray maximum parametric distances.
    ///
    /// @return     Bitmask of which rays are occluded (bit i set if ray i hit something).
    ///
    uint32_t occluded16(
        const Point16f& origins,
        const Direction16f& directions,
        const std::variant<Mask16, size_t>& active = size_t(16),
        const Float16& tmin = Float16::Zero(),
        const Float16& tmax = Float16::Constant(std::numeric_limits<float>::infinity())) const;

    /// @}

private:
    /// @cond LA_INTERNAL_DOCS
    struct Impl;
    value_ptr<Impl> m_impl;
    /// @endcond
};

} // namespace lagrange::raycasting
