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
#include <lagrange/raycasting/RayCaster.h>

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast.h>
#include <lagrange/scene/cast.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/build.h>
#include <lagrange/utils/point_triangle_squared_distance.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/views.h>

#ifdef LAGRANGE_WITH_EMBREE_3
    #include <embree3/rtcore.h>
    #include <embree3/rtcore_geometry.h>
    #include <embree3/rtcore_ray.h>
#else
    #include <embree4/rtcore.h>
    #include <embree4/rtcore_geometry.h>
    #include <embree4/rtcore_ray.h>
#endif

#if LAGRANGE_TARGET_PLATFORM(x86_64)
    #include <pmmintrin.h>
    #include <xmmintrin.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>
#include <variant>
#include <vector>

RTC_NAMESPACE_USE

namespace lagrange::raycasting {

// ============================================================================
// Helpers
// ============================================================================

namespace {

using SimpleScene32f = scene::SimpleScene32f3;
using MeshInstance = SimpleScene32f::InstanceType;

template <size_t N>
using PointNf = Eigen::Matrix<float, N, 3, Eigen::RowMajor>;
template <size_t N>
using DirectionNf = Eigen::Matrix<float, N, 3, Eigen::RowMajor>;
template <size_t N>
using FloatN = Eigen::Vector<float, N>;
template <size_t N>
using MaskN = Eigen::Vector<bool, N>;
template <size_t N>
using RTCRayHitN =
    std::conditional_t<N <= 4, RTCRayHit4, std::conditional_t<N <= 8, RTCRayHit8, RTCRayHit16>>;
template <size_t N>
using RTCRayN = std::conditional_t<N <= 4, RTCRay4, std::conditional_t<N <= 8, RTCRay8, RTCRay16>>;
template <size_t N>
using RTCPointQueryN = std::conditional_t<
    N <= 4,
    RTCPointQuery4,
    std::conditional_t<N <= 8, RTCPointQuery8, RTCPointQuery16>>;

void check_errors_runtime(const RTCDevice& device)
{
    auto err = rtcGetDeviceError(device);
    switch (err) {
    case RTC_ERROR_NONE: return;
    case RTC_ERROR_UNKNOWN: throw Error("Embree: unknown error");
    case RTC_ERROR_INVALID_ARGUMENT: throw Error("Embree: invalid argument");
    case RTC_ERROR_INVALID_OPERATION: throw Error("Embree: invalid operation");
    case RTC_ERROR_OUT_OF_MEMORY: throw Error("Embree: out of memory");
    case RTC_ERROR_UNSUPPORTED_CPU: throw Error("Embree: your CPU does not support SSE2");
    case RTC_ERROR_CANCELLED: throw Error("Embree: cancelled");
    default:
        throw Error(
            fmt::format(
                "Embree: unknown error code: {}",
                static_cast<std::underlying_type_t<RTCError>>(err)));
    }
}

void check_errors_debug([[maybe_unused]] const RTCDevice& device)
{
#if LAGRANGE_TARGET_BUILD_TYPE(DEBUG)
    check_errors_runtime(device);
#endif
}

inline float to_embree_tfar(float tmax)
{
    return std::isinf(tmax) ? std::numeric_limits<float>::max() : tmax;
}

constexpr RTCSceneFlags to_embree_scene_flags(BitField<SceneFlags> flags)
{
    RTCSceneFlags embree_flags(RTC_SCENE_FLAG_NONE);
    if (flags & SceneFlags::Dynamic) {
        embree_flags = embree_flags | RTC_SCENE_FLAG_DYNAMIC;
    }
    if (flags & SceneFlags::Compact) {
        embree_flags = embree_flags | RTC_SCENE_FLAG_COMPACT;
    }
    if (flags & SceneFlags::Robust) {
        embree_flags = embree_flags | RTC_SCENE_FLAG_ROBUST;
    }
    if (flags & SceneFlags::Filter) {
#if LAGRANGE_WITH_EMBREE_3
        embree_flags = embree_flags | RTC_SCENE_FLAG_CONTEXT_FILTER_FUNCTION;
#else
        embree_flags = embree_flags | RTC_SCENE_FLAG_FILTER_FUNCTION_IN_ARGUMENTS;
#endif
    }
    return embree_flags;
}

constexpr bool is_embree_flag_set(RTCSceneFlags embree_flags, SceneFlags flag)
{
    switch (flag) {
    case SceneFlags::Dynamic: return (embree_flags & RTC_SCENE_FLAG_DYNAMIC) != 0;
    case SceneFlags::Compact: return (embree_flags & RTC_SCENE_FLAG_COMPACT) != 0;
    case SceneFlags::Robust: return (embree_flags & RTC_SCENE_FLAG_ROBUST) != 0;
    case SceneFlags::Filter:
#if LAGRANGE_WITH_EMBREE_3
        return (embree_flags & RTC_SCENE_FLAG_CONTEXT_FILTER_FUNCTION) != 0;
#else
        return (embree_flags & RTC_SCENE_FLAG_FILTER_FUNCTION_IN_ARGUMENTS) != 0;
#endif
    default: la_runtime_assert(false, "Invalid SceneFlags value."); return false;
    }
}

constexpr RTCBuildQuality to_embree_build_quality(BuildQuality quality)
{
    switch (quality) {
    case BuildQuality::Low: return RTC_BUILD_QUALITY_LOW;
    case BuildQuality::Medium: return RTC_BUILD_QUALITY_MEDIUM;
    case BuildQuality::High: return RTC_BUILD_QUALITY_HIGH;
    default: la_runtime_assert(false, "Invalid build quality."); return RTC_BUILD_QUALITY_MEDIUM;
    }
}

/// Convert a bool-typed Eigen mask to an int32_t array for Embree.
/// Embree expects 0 (inactive) / -1 (active, i.e. 0xFFFFFFFF) as int32_t.
template <int N>
void mask_to_embree(const Eigen::Matrix<bool, N, 1>& mask, std::array<int, N>& out)
{
    for (int i = 0; i < N; ++i) {
        out[i] = mask[i] ? -1 : 0;
    }
}

/// Build an Embree int32_t mask from a count: the first `count` lanes are active.
template <int N>
void count_to_embree_mask(size_t count, std::array<int, N>& out)
{
    for (int i = 0; i < N; ++i) {
        out[i] = (static_cast<size_t>(i) < count) ? -1 : 0;
    }
}

/// Resolve a std::variant<MaskN, size_t> into an Embree-ready int32_t mask.
template <int N>
void resolve_active_mask(
    const std::variant<Eigen::Matrix<bool, N, 1>, size_t>& active,
    std::array<int, N>& out)
{
    if (auto* m = std::get_if<Eigen::Matrix<bool, N, 1>>(&active)) {
        mask_to_embree<N>(*m, out);
    } else {
        count_to_embree_mask<N>(std::get<size_t>(active), out);
    }
}

/// Mapping from Embree instance geometry ID to user-facing mesh/instance indices.
struct InstanceIndices
{
    uint32_t mesh_index;
    uint32_t instance_index;
};

/// User data passed to the Embree point query callback for closest-point queries on an instanced
/// scene. Holds a pointer to the scene so that the callback can retrieve triangle vertices.
struct ClosestPointUserData
{
    const SimpleScene32f* scene = nullptr;
    bool snap_to_vertex = false;

    // Current best result
    float distance = std::numeric_limits<float>::infinity();
    uint32_t mesh_index = invalid<uint32_t>();
    uint32_t instance_index = invalid<uint32_t>();
    uint32_t facet_index = invalid<uint32_t>();
    Eigen::Vector3f closest_point = Eigen::Vector3f::Zero();
    Eigen::Vector2f barycentric_coord = Eigen::Vector2f::Zero();

    // Mapping from Embree instance geometry ID to (mesh_index, instance_index).
    const std::vector<InstanceIndices>* instance_indices = nullptr;
};

// Embree point query callback for closest-point queries on an instanced scene.
//
// Adapted from: https://github.com/RenderKit/embree/tree/master/tutorials/closest_point
// SPDX-License-Identifier: Apache-2.0
//
// This function has been modified by Adobe.
//
// All modifications are Copyright 2020 Adobe.
bool embree_closest_point_callback(RTCPointQueryFunctionArguments* args)
{
    using Map4f = Eigen::Map<Eigen::Matrix4f, Eigen::Aligned16>;

    la_debug_assert(args->userPtr);
    auto* data = reinterpret_cast<ClosestPointUserData*>(args->userPtr);

    const unsigned int prim_id = args->primID;

    RTCPointQueryContext* context = args->context;
    const unsigned int stack_size = args->context->instStackSize;
    const unsigned int stack_ptr = stack_size - 1;

    // In an instanced scene, stack_size > 0 and instID gives us the instance geometry ID in the
    // world scene, from which we can recover the mesh and instance indices.
    la_debug_assert(stack_size > 0);
    unsigned int inst_geom_id = context->instID[stack_ptr];

    // TODO: Can we avoid this lookup?
    auto& indices = (*data->instance_indices)[inst_geom_id];
    uint32_t mesh_idx = indices.mesh_index;

    Eigen::Affine3f inst2world(Map4f(context->inst2world[stack_ptr]));

    // Query position in world space
    Eigen::Vector3f q(args->query->x, args->query->y, args->query->z);

    // Get triangle vertices from the stored mesh in local (instance) space
    const auto& mesh = data->scene->get_mesh(mesh_idx);
    la_debug_assert(mesh.is_triangle_mesh());
    la_debug_assert(prim_id < mesh.get_num_facets());

    // TODO: Is there any overhead to calling get_corner_to_vertex().get_all() here? Should we cache
    // this?
    auto facets = mesh.get_corner_to_vertex().get_all();
    uint32_t i0 = facets[prim_id * 3 + 0];
    uint32_t i1 = facets[prim_id * 3 + 1];
    uint32_t i2 = facets[prim_id * 3 + 2];

    auto positions = mesh.get_vertex_to_position().get_all();
    Eigen::Vector3f v0(positions[i0 * 3], positions[i0 * 3 + 1], positions[i0 * 3 + 2]);
    Eigen::Vector3f v1(positions[i1 * 3], positions[i1 * 3 + 1], positions[i1 * 3 + 2]);
    Eigen::Vector3f v2(positions[i2 * 3], positions[i2 * 3 + 1], positions[i2 * 3 + 2]);

    // Bring query and primitive data in the same space if necessary.
    if (stack_size > 0 && args->similarityScale > 0) {
        // Instance transform is a similarity transform, therefore we
        // can compute distance information in instance space. Therefore,
        // transform query position into local instance space.
        Eigen::Affine3f world2inst(Map4f(context->world2inst[stack_ptr]));
        q = world2inst * q;
    } else if (stack_size > 0) {
        // Instance transform is not a similarity transform. We have to transform the
        // primitive data into world space and perform distance computations in
        // world space to ensure correctness.
        v0 = inst2world * v0;
        v1 = inst2world * v1;
        v2 = inst2world * v2;
    }

    // Determine distance to closest point on triangle, and transform in
    // world space if necessary.
    Eigen::Vector3f p;
    float l1, l2, l3, d;
    if (data->snap_to_vertex) {
        // If snapping to vertex, compute distances to vertices and pick the closest one.
        float d0 = (q - v0).squaredNorm();
        float d1 = (q - v1).squaredNorm();
        float d2 = (q - v2).squaredNorm();
        if (d0 < d1 && d0 < d2) {
            p = v0;
            l1 = 1.0f;
            l2 = 0.0f;
            l3 = 0.0f;
            d = std::sqrt(d0);
        } else if (d1 < d2) {
            p = v1;
            l1 = 0.0f;
            l2 = 1.0f;
            l3 = 0.0f;
            d = std::sqrt(d1);
        } else {
            p = v2;
            l1 = 0.0f;
            l2 = 0.0f;
            l3 = 1.0f;
            d = std::sqrt(d2);
        }
    } else {
        float d2 = point_triangle_squared_distance(q, v0, v1, v2, p, l1, l2, l3);
        d = std::sqrt(d2);
    }
    if (args->similarityScale > 0) {
        d = d / args->similarityScale;
    }

    // Store result and update the query radius if we found a closer point.
    if (d < args->query->radius) {
        args->query->radius = d;
        data->distance = d;
        data->closest_point = (args->similarityScale > 0 ? (inst2world * p).eval() : p);
        data->mesh_index = mesh_idx;
        data->instance_index = indices.instance_index;
        data->facet_index = prim_id;
        // Store (u, v) barycentric coordinates: l2 corresponds to u and l3 corresponds to v
        data->barycentric_coord[0] = l2;
        data->barycentric_coord[1] = l3;
        return true;
    }

    return false;
}

// ============================================================================
// Impl
// ============================================================================

struct RayCasterImpl
{
    RTCSceneFlags m_scene_flags = to_embree_scene_flags(SceneFlags::None);
    RTCBuildQuality m_build_quality = to_embree_build_quality(BuildQuality::Medium);

    RTCDevice m_device;
    RTCScene m_world_scene;

    // Whether the world scene has changed and needs to be committed.
    bool m_need_commit = true;

    bool m_has_intersection_filter = false;
    bool m_has_occlusion_filter = false;

    // Per-mesh data ----------------------------------------------------------
    struct MeshData
    {
        unsigned mesh_geometry_id;
        RTCScene mesh_scene;
        std::function<bool(uint32_t instance_index, uint32_t facet_index)> intersection_filter;
        std::function<bool(uint32_t instance_index, uint32_t facet_index)> occlusion_filter;
    };

    // Extra data per source mesh
    std::vector<MeshData> m_meshes;

    // Per-instance data ------------------------------------------------------
    struct InstanceData
    {
        unsigned instance_geometry_id;
        bool visible = true;
    };

    // Extra data per instance
    std::vector<std::vector<InstanceData>> m_instances;

    // Mapping from Embree instance geometry ID to mesh/instance indices
    std::vector<InstanceIndices> m_instance_indices;

    // Scene data (buffers shared with the BVH)
    SimpleScene32f m_scene;

    // -----------------------------------------------------------------------
    RayCasterImpl()
    {
#if LAGRANGE_TARGET_PLATFORM(x86_64)
        // Embree strongly recommends having FTZ and DAZ enabled.
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

        m_device = rtcNewDevice(nullptr);
        check_errors_runtime(m_device);
        m_world_scene = rtcNewScene(m_device);
        check_errors_runtime(m_device);
        rtcSetSceneFlags(m_world_scene, m_scene_flags);
        check_errors_runtime(m_device);
        rtcSetSceneBuildQuality(m_world_scene, m_build_quality);
        check_errors_runtime(m_device);
    }

    ~RayCasterImpl()
    {
        for (auto& m : m_meshes) {
            rtcReleaseScene(m.mesh_scene);
        }
        rtcReleaseScene(m_world_scene);
        rtcReleaseDevice(m_device);
    }

    // TODO: Support other geometry types:
    // - RTC_GEOMETRY_TYPE_QUAD
    // - RTC_GEOMETRY_TYPE_USER for general hybrid/polygonal meshes?
    // - Subdiv meshes
    MeshData create_embree_mesh(SurfaceMesh32f& mesh, RTCScene mesh_scene = nullptr)
    {
        la_runtime_assert(mesh.is_triangle_mesh());
        MeshData data;

        // Create a new sub-scene for this mesh (needed for instancing).
        if (mesh_scene) {
            data.mesh_scene = mesh_scene;
        } else {
            data.mesh_scene = rtcNewScene(m_device);
            rtcSetSceneFlags(data.mesh_scene, m_scene_flags);
            rtcSetSceneBuildQuality(data.mesh_scene, m_build_quality);
            check_errors_runtime(m_device);
        }

        // Create a new geometry object.
        RTCGeometry geom = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_TRIANGLE);
        rtcSetGeometryBuildQuality(geom, m_build_quality);
        check_errors_runtime(m_device);

        // Set shared vertex buffer, padded to allow 16-byte SSE memory loads.
        // https://github.com/embree/embree/issues/124
        //
        // TODO: We should do this by default in the SurfaceMesh class, to avoid copying buffers
        mesh.ref_vertex_to_position().reserve_entries(mesh.get_num_vertices() * 3 + 1);
        rtcSetSharedGeometryBuffer(
            geom,
            RTC_BUFFER_TYPE_VERTEX,
            0,
            RTC_FORMAT_FLOAT3,
            mesh.get_vertex_to_position().get_all_with_padding().data(),
            0,
            sizeof(float) * 3,
            mesh.get_num_vertices());
        check_errors_runtime(m_device);

        // Set shared triangle buffer, padded to allow 16-byte SSE memory loads.
        // https://github.com/embree/embree/issues/124
        //
        // TODO: We should do this by default in the SurfaceMesh class, to avoid copying buffers
        mesh.ref_corner_to_vertex(AttributeRefPolicy::Force)
            .reserve_entries(mesh.get_num_facets() * 3 + 1);
        rtcSetSharedGeometryBuffer(
            geom,
            RTC_BUFFER_TYPE_INDEX,
            0,
            RTC_FORMAT_UINT3,
            mesh.get_corner_to_vertex().get_all_with_padding().data(),
            0,
            sizeof(uint32_t) * 3,
            mesh.get_num_facets());
        check_errors_runtime(m_device);

        rtcCommitGeometry(geom);
        check_errors_runtime(m_device);
        data.mesh_geometry_id = rtcAttachGeometry(data.mesh_scene, geom);
        check_errors_runtime(m_device);
        rtcReleaseGeometry(geom);
        check_errors_runtime(m_device);

        rtcCommitScene(data.mesh_scene);
        check_errors_runtime(m_device);

        return data;
    }

    uint32_t add_mesh(SurfaceMesh32f mesh)
    {
        uint32_t mesh_index = m_scene.add_mesh(std::move(mesh));
        la_debug_assert(mesh_index == m_meshes.size());
        m_meshes.push_back(create_embree_mesh(m_scene.ref_mesh(mesh_index)));
        m_instances.emplace_back();
        m_need_commit = true;
        return mesh_index;
    }

    InstanceData create_embree_instance(const RTCScene& mesh_scene, const MeshInstance& instance)
    {
        InstanceData data;

        RTCGeometry geom_inst = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(geom_inst, mesh_scene);
        rtcSetGeometryTimeStepCount(geom_inst, 1);

        rtcSetGeometryTransform(
            geom_inst,
            0,
            RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR,
            instance.transform.matrix().data());

        rtcCommitGeometry(geom_inst);
        data.instance_geometry_id = rtcAttachGeometry(m_world_scene, geom_inst);
        rtcReleaseGeometry(geom_inst);
        check_errors_runtime(m_device);

        m_need_commit = true;

        return data;
    }

    uint32_t add_instance(const MeshInstance& instance)
    {
        const auto& mesh_scene = m_meshes[instance.mesh_index].mesh_scene;
        uint32_t instance_index = m_scene.add_instance(instance);
        la_debug_assert(instance_index == m_instances[instance.mesh_index].size());
        m_instances[instance.mesh_index].emplace_back(create_embree_instance(mesh_scene, instance));
        auto& ist = m_instances[instance.mesh_index].back();
        m_instance_indices.resize(ist.instance_geometry_id + 1);
        m_instance_indices[ist.instance_geometry_id] = {instance.mesh_index, instance_index};
        return instance_index;
    }

    void add_scene(SimpleScene32f scene)
    {
        la_debug_assert(m_meshes.size() == m_instances.size());
        const uint32_t index_offset = static_cast<uint32_t>(m_meshes.size());

        m_meshes.reserve(m_meshes.size() + scene.get_num_meshes());
        m_instances.reserve(m_instances.size() + scene.get_num_meshes());

        for (uint32_t local_idx = 0; local_idx < scene.get_num_meshes(); ++local_idx) {
            add_mesh(std::move(scene.ref_mesh(local_idx)));
        }

        scene.foreach_instances([&](const auto& local_instance) {
            auto global_instance = local_instance;
            global_instance.mesh_index += index_offset;
            add_instance(global_instance);
        });

        m_need_commit = true;
    }

    void commit_updates_if_needed()
    {
        if (m_need_commit) {
            rtcCommitScene(m_world_scene);
            check_errors_runtime(m_device);
            m_need_commit = false;
        }
    }

    void check_no_pending_updates() const
    {
        if (m_need_commit) {
            throw Error("Scene changes not committed. Call commit_updates() first.");
        }
    }

    // Decode Embree instance geometry ID into user-facing mesh/instance indices.
    void decode_instance(unsigned rtc_inst_id, uint32_t& mesh_idx, uint32_t& inst_idx) const
    {
        mesh_idx = m_instance_indices[rtc_inst_id].mesh_index;
        inst_idx = m_instance_indices[rtc_inst_id].instance_index;
    }
};

// ============================================================================
// Embree filter callbacks
// ============================================================================

/// Whether the filter is for intersection or occlusion queries.
enum class FilterKind { Intersection, Occlusion };

/// Context struct that extends RTCRayQueryContext (Embree 4) / RTCIntersectContext (Embree 3) with a
/// pointer back to the RayCasterImpl so that the filter callback can look up user filter functions.
struct FilterContext
{
#ifdef LAGRANGE_WITH_EMBREE_3
    RTCIntersectContext embree_ctx;
#else
    RTCRayQueryContext embree_ctx;
#endif
    const RayCasterImpl* impl = nullptr;
    FilterKind kind = FilterKind::Intersection;
};

/// Embree filter callback invoked for each potential hit. Looks up the user-defined filter function
/// on the mesh that was hit and rejects the hit (sets valid[i] = 0) if the filter returns false.
void embree_filter_callback(const RTCFilterFunctionNArguments* args)
{
    // Recover our extended context from the context pointer.
#ifdef LAGRANGE_WITH_EMBREE_3
    auto* fctx = reinterpret_cast<FilterContext*>(args->context);
#else
    auto* fctx = reinterpret_cast<FilterContext*>(args->context);
#endif

    const RayCasterImpl* impl = fctx->impl;
    if (!impl) return;

    const unsigned int N = args->N;

    for (unsigned int i = 0; i < N; ++i) {
        // Skip lanes that are already invalid.
        if (args->valid[i] == 0) continue;

        // Extract instance geometry ID from the hit.
        unsigned int inst_id = RTCHitN_instID(args->hit, N, i, 0);
        if (inst_id == RTC_INVALID_GEOMETRY_ID) continue;

        uint32_t mesh_index = 0;
        uint32_t instance_index = 0;
        impl->decode_instance(inst_id, mesh_index, instance_index);

        unsigned int facet_index = RTCHitN_primID(args->hit, N, i);

        // Look up the user filter for this mesh.
        const auto& filter = (fctx->kind == FilterKind::Intersection)
                                 ? impl->m_meshes[mesh_index].intersection_filter
                                 : impl->m_meshes[mesh_index].occlusion_filter;

        if (filter && !filter(instance_index, facet_index)) {
            // Reject this hit.
            args->valid[i] = 0;
        }
    }
}

/// Returns true if any mesh in the scene has an intersection filter set.
bool has_any_intersection_filter(const RayCasterImpl& impl)
{
    for (const auto& m : impl.m_meshes) {
        if (m.intersection_filter) return true;
    }
    return false;
}

/// Returns true if any mesh in the scene has an occlusion filter set.
bool has_any_occlusion_filter(const RayCasterImpl& impl)
{
    for (const auto& m : impl.m_meshes) {
        if (m.occlusion_filter) return true;
    }
    return false;
}

/// Initialize a FilterContext for intersection queries.
void init_intersection_filter_context(FilterContext& fctx, const RayCasterImpl& impl)
{
#ifdef LAGRANGE_WITH_EMBREE_3
    rtcInitIntersectContext(&fctx.embree_ctx);
    fctx.embree_ctx.filter = embree_filter_callback;
#else
    rtcInitRayQueryContext(&fctx.embree_ctx);
#endif
    fctx.impl = &impl;
    fctx.kind = FilterKind::Intersection;
}

/// Initialize a FilterContext for occlusion queries.
void init_occlusion_filter_context(FilterContext& fctx, const RayCasterImpl& impl)
{
#ifdef LAGRANGE_WITH_EMBREE_3
    rtcInitIntersectContext(&fctx.embree_ctx);
    fctx.embree_ctx.filter = embree_filter_callback;
#else
    rtcInitRayQueryContext(&fctx.embree_ctx);
#endif
    fctx.impl = &impl;
    fctx.kind = FilterKind::Occlusion;
}

} // namespace

struct RayCaster::Impl : public RayCasterImpl
{
};

// ============================================================================
// Construction / destruction
// ============================================================================

RayCaster::RayCaster(BitField<SceneFlags> scene_flags, BuildQuality build_quality)
    : m_impl(make_value_ptr<Impl>())
{
    m_impl->m_scene_flags = to_embree_scene_flags(scene_flags);
    m_impl->m_build_quality = to_embree_build_quality(build_quality);
}

RayCaster::~RayCaster() = default;

RayCaster::RayCaster(RayCaster&& other) noexcept = default;

RayCaster& RayCaster::operator=(RayCaster&& other) noexcept = default;

// ============================================================================
// Scene population
// ============================================================================

template <typename Scalar, typename Index>
uint32_t RayCaster::add_mesh(
    SurfaceMesh<Scalar, Index> mesh,
    const std::optional<Affine<Scalar>>& transform)
{
    uint32_t mesh_index = m_impl->add_mesh(SurfaceMesh32f::stripped_move(std::move(mesh)));

    if (transform.has_value()) {
        MeshInstance instance;
        instance.mesh_index = mesh_index;
        instance.transform = transform.value().template cast<float>();
        m_impl->add_instance(instance);
    }
    return mesh_index;
}

uint32_t RayCaster::add_instance(uint32_t mesh_index, const Eigen::Affine3f& transform)
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_meshes.size(),
        "mesh_index out of range.");

    MeshInstance instance;
    instance.mesh_index = mesh_index;
    instance.transform = transform;
    return m_impl->add_instance(instance);
}

template <typename Scalar, typename Index>
void RayCaster::add_scene(scene::SimpleScene<Scalar, Index, 3> simple_scene)
{
    for (Index i = 0; i < simple_scene.get_num_meshes(); ++i) {
        auto& mesh = simple_scene.ref_mesh(i);
        mesh = SurfaceMesh<Scalar, Index>::stripped_move(std::move(mesh));
    }
    m_impl->add_scene(lagrange::scene::cast<float, uint32_t>(std::move(simple_scene)));
}

void RayCaster::commit_updates()
{
    m_impl->commit_updates_if_needed();
}

// ============================================================================
// Scene modification
// ============================================================================

template <typename Scalar, typename Index>
void RayCaster::update_mesh(uint32_t mesh_index, const SurfaceMesh<Scalar, Index>& mesh)
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_meshes.size(),
        "mesh_index out of range.");

    // Detach the previous mesh geometry from the mesh scene
    rtcDetachGeometry(
        m_impl->m_meshes[mesh_index].mesh_scene,
        m_impl->m_meshes[mesh_index].mesh_geometry_id);

    // Replace the mesh in the scene (cast to float/uint32_t if needed).
    m_impl->m_scene.ref_mesh(mesh_index) = lagrange::cast<float, uint32_t>(mesh);

    // Recreate the Embree geometry for this mesh, preserving filters.
    auto mesh_data = std::move(m_impl->m_meshes[mesh_index]);
    m_impl->m_meshes[mesh_index] =
        m_impl->create_embree_mesh(m_impl->m_scene.ref_mesh(mesh_index), mesh_data.mesh_scene);
    m_impl->m_meshes[mesh_index].intersection_filter = std::move(mesh_data.intersection_filter);
    m_impl->m_meshes[mesh_index].occlusion_filter = std::move(mesh_data.occlusion_filter);

    m_impl->m_need_commit = true;
}

template <typename Scalar, typename Index>
void RayCaster::update_vertices(uint32_t mesh_index, const SurfaceMesh<Scalar, Index>& mesh)
{
    update_vertices(mesh_index, mesh.get_vertex_to_position().get_all());
}

template <typename Scalar>
void RayCaster::update_vertices(uint32_t mesh_index, span<const Scalar> vertices)
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_meshes.size(),
        "mesh_index out of range.");

    auto& stored_mesh = m_impl->m_scene.ref_mesh(mesh_index);
    const uint32_t num_vertices = stored_mesh.get_num_vertices();
    la_runtime_assert(
        vertices.size() == static_cast<size_t>(num_vertices) * 3,
        "Vertex buffer size mismatch (expected num_vertices * 3).");

    // Update vertex positions in the stored mesh.
    auto dst = stored_mesh.ref_vertex_to_position().ref_all();
    std::transform(vertices.begin(), vertices.end(), dst.begin(), [](auto&& x) {
        return static_cast<float>(x);
    });

    // Mark Embree geometry as modified.
    RTCGeometry geom = rtcGetGeometry(
        m_impl->m_meshes[mesh_index].mesh_scene,
        m_impl->m_meshes[mesh_index].mesh_geometry_id);
    rtcUpdateGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0);
    rtcCommitGeometry(geom);
    rtcCommitScene(m_impl->m_meshes[mesh_index].mesh_scene);
    check_errors_runtime(m_impl->m_device);

    m_impl->m_need_commit = true;
}

Eigen::Affine3f RayCaster::get_transform(uint32_t mesh_index, uint32_t instance_index) const
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_instances.size(),
        "mesh_index out of range.");
    la_runtime_assert(
        static_cast<size_t>(instance_index) < m_impl->m_instances[mesh_index].size(),
        "instance_index out of range.");
    return m_impl->m_scene.get_instance(mesh_index, instance_index).transform;
}

void RayCaster::update_transform(
    uint32_t mesh_index,
    uint32_t instance_index,
    const Eigen::Affine3f& transform)
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_instances.size(),
        "mesh_index out of range.");
    la_runtime_assert(
        static_cast<size_t>(instance_index) < m_impl->m_instances[mesh_index].size(),
        "instance_index out of range.");

    m_impl->m_scene.ref_instance(mesh_index, instance_index).transform = transform;

    // Update the Embree instance geometry.
    unsigned geom_id = m_impl->m_instances[mesh_index][instance_index].instance_geometry_id;
    RTCGeometry geom_inst = rtcGetGeometry(m_impl->m_world_scene, geom_id);
    rtcSetGeometryTransform(
        geom_inst,
        0,
        RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR,
        transform.matrix().data());
    rtcCommitGeometry(geom_inst);
    check_errors_runtime(m_impl->m_device);

    m_impl->m_need_commit = true;
}

bool RayCaster::get_visibility(uint32_t mesh_index, uint32_t instance_index)
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_instances.size(),
        "mesh_index out of range.");
    la_runtime_assert(
        static_cast<size_t>(instance_index) < m_impl->m_instances[mesh_index].size(),
        "instance_index out of range.");
    return m_impl->m_instances[mesh_index][instance_index].visible;
}

void RayCaster::update_visibility(uint32_t mesh_index, uint32_t instance_index, bool visible)
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_instances.size(),
        "mesh_index out of range.");
    la_runtime_assert(
        static_cast<size_t>(instance_index) < m_impl->m_instances[mesh_index].size(),
        "instance_index out of range.");

    auto& inst = m_impl->m_instances[mesh_index][instance_index];
    if (inst.visible != visible) {
        inst.visible = visible;

        unsigned geom_id = inst.instance_geometry_id;
        if (visible) {
            rtcEnableGeometry(rtcGetGeometry(m_impl->m_world_scene, geom_id));
        } else {
            rtcDisableGeometry(rtcGetGeometry(m_impl->m_world_scene, geom_id));
        }
        check_errors_runtime(m_impl->m_device);

        m_impl->m_need_commit = true;
    }
}

// ============================================================================
// Filtering
// ============================================================================

auto RayCaster::get_intersection_filter(uint32_t mesh_index) const
    -> std::function<bool(uint32_t instance_index, uint32_t facet_index)>
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_meshes.size(),
        "mesh_index out of range.");
    return m_impl->m_meshes[mesh_index].intersection_filter;
}

void RayCaster::set_intersection_filter(
    uint32_t mesh_index,
    std::function<bool(uint32_t instance_index, uint32_t facet_index)>&& filter)
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_meshes.size(),
        "mesh_index out of range.");
    la_runtime_assert(
        !filter || is_embree_flag_set(m_impl->m_scene_flags, SceneFlags::Filter),
        "Scene must be created with SceneFlags::Filter to use intersection filters.");
    m_impl->m_meshes[mesh_index].intersection_filter = std::move(filter);
    m_impl->m_has_intersection_filter = has_any_intersection_filter(*m_impl);
}

auto RayCaster::get_occlusion_filter(uint32_t mesh_index) const
    -> std::function<bool(uint32_t instance_index, uint32_t facet_index)>
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_meshes.size(),
        "mesh_index out of range.");
    return m_impl->m_meshes[mesh_index].occlusion_filter;
}

void RayCaster::set_occlusion_filter(
    uint32_t mesh_index,
    std::function<bool(uint32_t instance_index, uint32_t facet_index)>&& filter)
{
    la_runtime_assert(
        static_cast<size_t>(mesh_index) < m_impl->m_meshes.size(),
        "mesh_index out of range.");
    la_runtime_assert(
        !filter || is_embree_flag_set(m_impl->m_scene_flags, SceneFlags::Filter),
        "Scene must be created with SceneFlags::Filter to use occlusion filters.");
    m_impl->m_meshes[mesh_index].occlusion_filter = std::move(filter);
    m_impl->m_has_occlusion_filter = has_any_occlusion_filter(*m_impl);
}

// ============================================================================
// Closest point queries
// ============================================================================

namespace {

std::optional<ClosestPointHit> closest_point_impl(
    const RayCasterImpl& impl,
    const Eigen::Vector3f& query_point,
    const bool snap_to_vertex)
{
    impl.check_no_pending_updates();

    RTCPointQuery query;
    query.x = query_point.x();
    query.y = query_point.y();
    query.z = query_point.z();
    query.radius = std::numeric_limits<float>::max();
    query.time = 0.f;

    ClosestPointUserData data;
    data.scene = &impl.m_scene;
    data.snap_to_vertex = snap_to_vertex;
    data.instance_indices = &impl.m_instance_indices;

    RTCPointQueryContext context;
    rtcInitPointQueryContext(&context);
    rtcPointQuery(
        impl.m_world_scene,
        &query,
        &context,
        &embree_closest_point_callback,
        reinterpret_cast<void*>(&data));
    check_errors_debug(impl.m_device);

    if (data.mesh_index == invalid<uint32_t>()) {
        return std::nullopt;
    }

    ClosestPointHit hit;
    hit.distance = data.distance;
    hit.mesh_index = data.mesh_index;
    hit.instance_index = data.instance_index;
    hit.facet_index = data.facet_index;
    hit.barycentric_coord = data.barycentric_coord;
    hit.position = data.closest_point;
    return hit;
}

template <size_t N>
ClosestPointHitN<N> closest_pointN(
    const RayCasterImpl& impl,
    const PointNf<N>& query_points,
    const std::variant<MaskN<N>, size_t>& active,
    const bool snap_to_vertex)
{
    impl.check_no_pending_updates();

    constexpr size_t kAlign = N <= 4 ? 16 : (N <= 8 ? 32 : 64);
    alignas(kAlign) std::array<int, N> active_mask;
    resolve_active_mask<N>(active, active_mask);

    // Set up per-lane user data and SoA query packet.
    std::array<ClosestPointUserData, N> lane_data;
    std::array<void*, N> user_ptrs;
    RTCPointQueryN<N> query;
    for (size_t i = 0; i < N; ++i) {
        query.x[i] = query_points(i, 0);
        query.y[i] = query_points(i, 1);
        query.z[i] = query_points(i, 2);
        query.time[i] = 0.f;
        query.radius[i] = std::numeric_limits<float>::max();
        lane_data[i].scene = &impl.m_scene;
        lane_data[i].snap_to_vertex = snap_to_vertex;
        lane_data[i].instance_indices = &impl.m_instance_indices;
        user_ptrs[i] = &lane_data[i];
    }

    RTCPointQueryContext context;
    rtcInitPointQueryContext(&context);
    auto rtc_point_query_fn = [] {
        if constexpr (N == 4) {
            return rtcPointQuery4;
        } else if constexpr (N == 8) {
            return rtcPointQuery8;
        } else if constexpr (N == 16) {
            return rtcPointQuery16;
        } else {
            static_assert(N == 4 || N == 8 || N == 16, "Unsupported packet size.");
        }
    }();
    rtc_point_query_fn(
        active_mask.data(),
        impl.m_world_scene,
        &query,
        &context,
        &embree_closest_point_callback,
        user_ptrs.data());
    check_errors_debug(impl.m_device);

    ClosestPointHitN<N> result;
    for (size_t i = 0; i < N; ++i) {
        if (lane_data[i].mesh_index != invalid<uint32_t>()) {
            result.valid_mask |= (1u << i);
            result.distances[i] = lane_data[i].distance;
            result.mesh_indices[i] = lane_data[i].mesh_index;
            result.instance_indices[i] = lane_data[i].instance_index;
            result.facet_indices[i] = lane_data[i].facet_index;
            result.barycentric_coords.col(i) = lane_data[i].barycentric_coord;
            result.positions.col(i) = lane_data[i].closest_point;
        }
    }

    return result;
}

} // namespace

std::optional<ClosestPointHit> RayCaster::closest_point(const Pointf& query_point) const
{
    return closest_point_impl(*m_impl, query_point, false);
}

RayCaster::ClosestPointHit4 RayCaster::closest_point4(
    const Point4f& query_points,
    const std::variant<Mask4, size_t>& active) const
{
    return closest_pointN<4>(*m_impl, query_points, active, false);
}

RayCaster::ClosestPointHit8 RayCaster::closest_point8(
    const Point8f& query_points,
    const std::variant<Mask8, size_t>& active) const
{
    return closest_pointN<8>(*m_impl, query_points, active, false);
}

RayCaster::ClosestPointHit16 RayCaster::closest_point16(
    const Point16f& query_points,
    const std::variant<Mask16, size_t>& active) const
{
    return closest_pointN<16>(*m_impl, query_points, active, false);
}

std::optional<ClosestPointHit> RayCaster::closest_vertex(const Pointf& query_point) const
{
    return closest_point_impl(*m_impl, query_point, true);
}

RayCaster::ClosestPointHit4 RayCaster::closest_vertex4(
    const Point4f& query_points,
    const std::variant<Mask4, size_t>& active) const
{
    return closest_pointN<4>(*m_impl, query_points, active, true);
}

RayCaster::ClosestPointHit8 RayCaster::closest_vertex8(
    const Point8f& query_points,
    const std::variant<Mask8, size_t>& active) const
{
    return closest_pointN<8>(*m_impl, query_points, active, true);
}

RayCaster::ClosestPointHit16 RayCaster::closest_vertex16(
    const Point16f& query_points,
    const std::variant<Mask16, size_t>& active) const
{
    return closest_pointN<16>(*m_impl, query_points, active, true);
}

// ============================================================================
// Single-ray queries
// ============================================================================

std::optional<RayHit>
RayCaster::cast(const Pointf& origin, const Directionf& direction, float tmin, float tmax) const
{
    m_impl->check_no_pending_updates();

    RTCRayHit rayhit;
    rayhit.ray.org_x = origin.x();
    rayhit.ray.org_y = origin.y();
    rayhit.ray.org_z = origin.z();
    rayhit.ray.dir_x = direction.x();
    rayhit.ray.dir_y = direction.y();
    rayhit.ray.dir_z = direction.z();
    rayhit.ray.tnear = tmin;
    rayhit.ray.tfar = to_embree_tfar(tmax);
    rayhit.ray.mask = 0xFFFFFFFF;
    rayhit.ray.id = 0;
    rayhit.ray.flags = 0;
    rayhit.ray.time = 0.f;
    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

#ifdef LAGRANGE_WITH_EMBREE_3
    if (m_impl->m_has_intersection_filter) {
        FilterContext fctx;
        init_intersection_filter_context(fctx, *m_impl);
        rtcIntersect1(m_impl->m_world_scene, &fctx.embree_ctx, &rayhit);
    } else {
        RTCIntersectContext ctx;
        rtcInitIntersectContext(&ctx);
        rtcIntersect1(m_impl->m_world_scene, &ctx, &rayhit);
    }
#else
    if (m_impl->m_has_intersection_filter) {
        FilterContext fctx;
        init_intersection_filter_context(fctx, *m_impl);
        RTCIntersectArguments args;
        rtcInitIntersectArguments(&args);
        args.context = &fctx.embree_ctx;
        args.filter = embree_filter_callback;
        args.flags = RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER;
        rtcIntersect1(m_impl->m_world_scene, &rayhit, &args);
    } else {
        rtcIntersect1(m_impl->m_world_scene, &rayhit);
    }
#endif

    if (rayhit.hit.geomID == RTC_INVALID_GEOMETRY_ID) {
        return std::nullopt;
    }

    RayHit hit;
    unsigned rtc_inst_id = rayhit.hit.instID[0];
    la_debug_assert(rtc_inst_id != RTC_INVALID_GEOMETRY_ID);
    m_impl->decode_instance(rtc_inst_id, hit.mesh_index, hit.instance_index);
    hit.facet_index = rayhit.hit.primID;
    hit.ray_depth = rayhit.ray.tfar;
    hit.barycentric_coord[0] = rayhit.hit.u;
    hit.barycentric_coord[1] = rayhit.hit.v;
    hit.position = origin + direction * hit.ray_depth;
    hit.normal[0] = rayhit.hit.Ng_x;
    hit.normal[1] = rayhit.hit.Ng_y;
    hit.normal[2] = rayhit.hit.Ng_z;
    return hit;
}

bool RayCaster::occluded(const Pointf& origin, const Directionf& direction, float tmin, float tmax)
    const
{
    m_impl->check_no_pending_updates();

    RTCRay ray;
    ray.org_x = origin.x();
    ray.org_y = origin.y();
    ray.org_z = origin.z();
    ray.dir_x = direction.x();
    ray.dir_y = direction.y();
    ray.dir_z = direction.z();
    ray.tnear = tmin;
    ray.tfar = to_embree_tfar(tmax);
    ray.mask = 0xFFFFFFFF;
    ray.id = 0;
    ray.flags = 0;
    ray.time = 0.f;

#ifdef LAGRANGE_WITH_EMBREE_3
    if (m_impl->m_has_occlusion_filter) {
        FilterContext fctx;
        init_occlusion_filter_context(fctx, *m_impl);
        rtcOccluded1(m_impl->m_world_scene, &fctx.embree_ctx, &ray);
    } else {
        RTCIntersectContext ctx;
        rtcInitIntersectContext(&ctx);
        rtcOccluded1(m_impl->m_world_scene, &ctx, &ray);
    }
#else
    if (m_impl->m_has_occlusion_filter) {
        FilterContext fctx;
        init_occlusion_filter_context(fctx, *m_impl);
        RTCOccludedArguments args;
        rtcInitOccludedArguments(&args);
        args.context = &fctx.embree_ctx;
        args.filter = embree_filter_callback;
        args.flags = RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER;
        rtcOccluded1(m_impl->m_world_scene, &ray, &args);
    } else {
        rtcOccluded1(m_impl->m_world_scene, &ray);
    }
#endif

    // If hit, Embree sets tfar to -inf.
    return !std::isfinite(ray.tfar);
}

// ============================================================================
// Ray packet queries - templated
// ============================================================================

namespace {

template <size_t N>
RayHitN<N> castN(
    const RayCasterImpl& impl,
    const PointNf<N>& origins,
    const DirectionNf<N>& directions,
    const std::variant<MaskN<N>, size_t>& active,
    const FloatN<N>& tmin,
    const FloatN<N>& tmax)
{
    impl.check_no_pending_updates();

    constexpr size_t kAlign = N <= 4 ? 16 : (N <= 8 ? 32 : 64);
    alignas(kAlign) std::array<int, N> embree_mask;
    resolve_active_mask<N>(active, embree_mask);

    RTCRayHitN<N> packet;
    for (size_t i = 0; i < N; ++i) {
        // Only copy data for active rays to avoid using potentially uninitialized data.
        // Inactive rays get safe default values.
        if (embree_mask[i]) {
            packet.ray.org_x[i] = origins(i, 0);
            packet.ray.org_y[i] = origins(i, 1);
            packet.ray.org_z[i] = origins(i, 2);
            packet.ray.dir_x[i] = directions(i, 0);
            packet.ray.dir_y[i] = directions(i, 1);
            packet.ray.dir_z[i] = directions(i, 2);
            packet.ray.tnear[i] = tmin[i];
            packet.ray.tfar[i] = to_embree_tfar(tmax[i]);
        } else {
            // Initialize inactive rays with valid default values to avoid NaN/garbage data
            packet.ray.org_x[i] = 0.f;
            packet.ray.org_y[i] = 0.f;
            packet.ray.org_z[i] = 0.f;
            packet.ray.dir_x[i] = 0.f;
            packet.ray.dir_y[i] = 0.f;
            packet.ray.dir_z[i] = 1.f; // Valid non-zero direction
            packet.ray.tnear[i] = 0.f;
            packet.ray.tfar[i] = std::numeric_limits<float>::infinity();
        }
        packet.ray.mask[i] = 0xFFFFFFFF;
        packet.ray.id[i] = static_cast<unsigned>(i);
        packet.ray.flags[i] = 0;
        packet.ray.time[i] = 0.f;
        packet.hit.geomID[i] = RTC_INVALID_GEOMETRY_ID;
        packet.hit.primID[i] = RTC_INVALID_GEOMETRY_ID;
        packet.hit.instID[0][i] = RTC_INVALID_GEOMETRY_ID;
    }

#ifdef LAGRANGE_WITH_EMBREE_3
    if (impl.m_has_intersection_filter) {
        FilterContext fctx;
        init_intersection_filter_context(fctx, impl);
        if constexpr (N <= 4) {
            rtcIntersect4(embree_mask.data(), impl.m_world_scene, &fctx.embree_ctx, &packet);
        } else if constexpr (N <= 8) {
            rtcIntersect8(embree_mask.data(), impl.m_world_scene, &fctx.embree_ctx, &packet);
        } else {
            rtcIntersect16(embree_mask.data(), impl.m_world_scene, &fctx.embree_ctx, &packet);
        }
    } else {
        RTCIntersectContext ctx;
        rtcInitIntersectContext(&ctx);
        if constexpr (N <= 4) {
            rtcIntersect4(embree_mask.data(), impl.m_world_scene, &ctx, &packet);
        } else if constexpr (N <= 8) {
            rtcIntersect8(embree_mask.data(), impl.m_world_scene, &ctx, &packet);
        } else {
            rtcIntersect16(embree_mask.data(), impl.m_world_scene, &ctx, &packet);
        }
    }
#else
    if (impl.m_has_intersection_filter) {
        FilterContext fctx;
        init_intersection_filter_context(fctx, impl);
        RTCIntersectArguments args;
        rtcInitIntersectArguments(&args);
        args.context = &fctx.embree_ctx;
        args.filter = embree_filter_callback;
        args.flags = RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER;
        if constexpr (N <= 4) {
            rtcIntersect4(embree_mask.data(), impl.m_world_scene, &packet, &args);
        } else if constexpr (N <= 8) {
            rtcIntersect8(embree_mask.data(), impl.m_world_scene, &packet, &args);
        } else {
            rtcIntersect16(embree_mask.data(), impl.m_world_scene, &packet, &args);
        }
    } else {
        if constexpr (N <= 4) {
            rtcIntersect4(embree_mask.data(), impl.m_world_scene, &packet);
        } else if constexpr (N <= 8) {
            rtcIntersect8(embree_mask.data(), impl.m_world_scene, &packet);
        } else {
            rtcIntersect16(embree_mask.data(), impl.m_world_scene, &packet);
        }
    }
#endif

    RayHitN<N> result;
    for (size_t i = 0; i < N; ++i) {
        if (packet.hit.geomID[i] != RTC_INVALID_GEOMETRY_ID) {
            result.valid_mask |= (1u << i);
            unsigned rtc_inst_id = packet.hit.instID[0][i];
            impl.decode_instance(rtc_inst_id, result.mesh_indices[i], result.instance_indices[i]);
            result.facet_indices[i] = packet.hit.primID[i];
            result.ray_depths[i] = packet.ray.tfar[i];
            result.barycentric_coords(0, i) = packet.hit.u[i];
            result.barycentric_coords(1, i) = packet.hit.v[i];
            result.positions.col(i) =
                origins.row(i).transpose() + directions.row(i).transpose() * result.ray_depths[i];
            result.normals(0, i) = packet.hit.Ng_x[i];
            result.normals(1, i) = packet.hit.Ng_y[i];
            result.normals(2, i) = packet.hit.Ng_z[i];
        }
    }

    return result;
}

template <size_t N>
uint32_t occludedN(
    const RayCasterImpl& impl,
    const PointNf<N>& origins,
    const DirectionNf<N>& directions,
    const std::variant<MaskN<N>, size_t>& active,
    const FloatN<N>& tmin,
    const FloatN<N>& tmax)
{
    impl.check_no_pending_updates();

    constexpr size_t kAlign = N <= 4 ? 16 : (N <= 8 ? 32 : 64);
    alignas(kAlign) std::array<int, N> embree_mask;
    resolve_active_mask<N>(active, embree_mask);

    RTCRayN<N> packet;
    for (size_t i = 0; i < N; ++i) {
        // Only copy data for active rays to avoid using potentially uninitialized data.
        // Inactive rays get safe default values.
        if (embree_mask[i]) {
            packet.org_x[i] = origins(i, 0);
            packet.org_y[i] = origins(i, 1);
            packet.org_z[i] = origins(i, 2);
            packet.dir_x[i] = directions(i, 0);
            packet.dir_y[i] = directions(i, 1);
            packet.dir_z[i] = directions(i, 2);
            packet.tnear[i] = tmin[i];
            packet.tfar[i] = to_embree_tfar(tmax[i]);
        } else {
            // Initialize inactive rays with valid default values to avoid NaN/garbage data
            packet.org_x[i] = 0.f;
            packet.org_y[i] = 0.f;
            packet.org_z[i] = 0.f;
            packet.dir_x[i] = 0.f;
            packet.dir_y[i] = 0.f;
            packet.dir_z[i] = 1.f; // Valid non-zero direction
            packet.tnear[i] = 0.f;
            packet.tfar[i] = std::numeric_limits<float>::infinity();
        }
        packet.mask[i] = 0xFFFFFFFF;
        packet.id[i] = static_cast<unsigned>(i);
        packet.flags[i] = 0;
        packet.time[i] = 0.f;
    }

#ifdef LAGRANGE_WITH_EMBREE_3
    if (impl.m_has_occlusion_filter) {
        FilterContext fctx;
        init_occlusion_filter_context(fctx, impl);
        if constexpr (N <= 4) {
            rtcOccluded4(embree_mask.data(), impl.m_world_scene, &fctx.embree_ctx, &packet);
        } else if constexpr (N <= 8) {
            rtcOccluded8(embree_mask.data(), impl.m_world_scene, &fctx.embree_ctx, &packet);
        } else {
            rtcOccluded16(embree_mask.data(), impl.m_world_scene, &fctx.embree_ctx, &packet);
        }
    } else {
        RTCIntersectContext ctx;
        rtcInitIntersectContext(&ctx);
        if constexpr (N <= 4) {
            rtcOccluded4(embree_mask.data(), impl.m_world_scene, &ctx, &packet);
        } else if constexpr (N <= 8) {
            rtcOccluded8(embree_mask.data(), impl.m_world_scene, &ctx, &packet);
        } else {
            rtcOccluded16(embree_mask.data(), impl.m_world_scene, &ctx, &packet);
        }
    }
#else
    if (impl.m_has_occlusion_filter) {
        FilterContext fctx;
        init_occlusion_filter_context(fctx, impl);
        RTCOccludedArguments args;
        rtcInitOccludedArguments(&args);
        args.context = &fctx.embree_ctx;
        args.filter = embree_filter_callback;
        args.flags = RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER;
        if constexpr (N <= 4) {
            rtcOccluded4(embree_mask.data(), impl.m_world_scene, &packet, &args);
        } else if constexpr (N <= 8) {
            rtcOccluded8(embree_mask.data(), impl.m_world_scene, &packet, &args);
        } else {
            rtcOccluded16(embree_mask.data(), impl.m_world_scene, &packet, &args);
        }
    } else {
        if constexpr (N <= 4) {
            rtcOccluded4(embree_mask.data(), impl.m_world_scene, &packet);
        } else if constexpr (N <= 8) {
            rtcOccluded8(embree_mask.data(), impl.m_world_scene, &packet);
        } else {
            rtcOccluded16(embree_mask.data(), impl.m_world_scene, &packet);
        }
    }
#endif

    uint32_t hit_mask = 0;
    for (size_t i = 0; i < N; ++i) {
        if (embree_mask[i] && !std::isfinite(packet.tfar[i])) {
            hit_mask |= (1u << i);
        }
    }

    return hit_mask;
}

} // namespace

// ============================================================================
// Ray packet queries - 4-wide
// ============================================================================

RayCaster::RayHit4 RayCaster::cast4(
    const Point4f& origins,
    const Direction4f& directions,
    const std::variant<Mask4, size_t>& active,
    const Float4& tmin,
    const Float4& tmax) const
{
    return castN<4>(*m_impl, origins, directions, active, tmin, tmax);
}

uint32_t RayCaster::occluded4(
    const Point4f& origins,
    const Direction4f& directions,
    const std::variant<Mask4, size_t>& active,
    const Float4& tmin,
    const Float4& tmax) const
{
    return occludedN<4>(*m_impl, origins, directions, active, tmin, tmax);
}

// ============================================================================
// Ray packet queries - 8-wide
// ============================================================================

RayCaster::RayHit8 RayCaster::cast8(
    const Point8f& origins,
    const Direction8f& directions,
    const std::variant<Mask8, size_t>& active,
    const Float8& tmin,
    const Float8& tmax) const
{
    return castN<8>(*m_impl, origins, directions, active, tmin, tmax);
}

uint32_t RayCaster::occluded8(
    const Point8f& origins,
    const Direction8f& directions,
    const std::variant<Mask8, size_t>& active,
    const Float8& tmin,
    const Float8& tmax) const
{
    return occludedN<8>(*m_impl, origins, directions, active, tmin, tmax);
}

// ============================================================================
// Ray packet queries - 16-wide
// ============================================================================

RayCaster::RayHit16 RayCaster::cast16(
    const Point16f& origins,
    const Direction16f& directions,
    const std::variant<Mask16, size_t>& active,
    const Float16& tmin,
    const Float16& tmax) const
{
    return castN<16>(*m_impl, origins, directions, active, tmin, tmax);
}

uint32_t RayCaster::occluded16(
    const Point16f& origins,
    const Direction16f& directions,
    const std::variant<Mask16, size_t>& active,
    const Float16& tmin,
    const Float16& tmax) const
{
    return occludedN<16>(*m_impl, origins, directions, active, tmin, tmax);
}

// ============================================================================
// Explicit template instantiation
// ============================================================================

#define LA_X_raycaster_methods(_, Scalar, Index)                               \
    template LA_RAYCASTING_API uint32_t RayCaster::add_mesh<Scalar, Index>(    \
        SurfaceMesh<Scalar, Index>,                                            \
        const std::optional<RayCaster::Affine<Scalar>>&);                      \
    template LA_RAYCASTING_API void RayCaster::add_scene<Scalar, Index>(       \
        scene::SimpleScene<Scalar, Index, 3>);                                 \
    template LA_RAYCASTING_API void RayCaster::update_mesh<Scalar, Index>(     \
        uint32_t,                                                              \
        const SurfaceMesh<Scalar, Index>&);                                    \
    template LA_RAYCASTING_API void RayCaster::update_vertices<Scalar, Index>( \
        uint32_t,                                                              \
        const SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(raycaster_methods, 0)

#define LA_X_raycaster_update_vertices(_, Scalar)                       \
    template LA_RAYCASTING_API void RayCaster::update_vertices<Scalar>( \
        uint32_t,                                                       \
        span<const Scalar>);
LA_SURFACE_MESH_SCALAR_X(raycaster_update_vertices, 0)

} // namespace lagrange::raycasting
