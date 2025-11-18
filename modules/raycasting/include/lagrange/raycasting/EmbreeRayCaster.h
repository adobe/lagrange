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

#ifdef LAGRANGE_WITH_EMBREE_4
    #include <embree4/rtcore.h>
    #include <embree4/rtcore_geometry.h>
    #include <embree4/rtcore_ray.h>
#else
    #include <embree3/rtcore.h>
    #include <embree3/rtcore_geometry.h>
    #include <embree3/rtcore_ray.h>
#endif

#include <lagrange/common.h>
#include <lagrange/raycasting/ClosestPointResult.h>
#include <lagrange/raycasting/EmbreeHelper.h>
#include <lagrange/raycasting/RayCasterMesh.h>
#include <lagrange/raycasting/embree_closest_point.h>
#include <lagrange/utils/safe_cast.h>

#include <cassert>
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

RTC_NAMESPACE_USE

namespace lagrange {
namespace raycasting {

/**
 * A wrapper for Embree's raycasting API to compute ray intersections with (instances of) meshes.
 * Supports intersection and occlusion queries on single rays and ray packets (currently only
 * packets of size at most 4 are supported). Filters may be specified (per mesh, not per instance)
 * to process each individual hit event during any of these queries.
 */
template <typename ScalarType>
class EmbreeRayCaster
{
public:
    using Scalar = ScalarType;
    using Transform = Eigen::Matrix<Scalar, 4, 4>;
    using Point = Eigen::Matrix<Scalar, 3, 1>;
    using Direction = Eigen::Matrix<Scalar, 3, 1>;
    using Index = size_t;
    using ClosestPoint = ClosestPointResult<Scalar>;
    using TransformVector = std::vector<Transform>;

    using Point4 = Eigen::Matrix<Scalar, 4, 3>;
    using Direction4 = Eigen::Matrix<Scalar, 4, 3>;
    using Index4 = Eigen::Matrix<size_t, 4, 1>;
    using Scalar4 = Eigen::Matrix<Scalar, 4, 1>;
    using Mask4 = Eigen::Matrix<std::int32_t, 4, 1>;

    using FloatData = std::vector<float>;
    using IntData = std::vector<unsigned>;

    /**
     * Interface for a hit filter function. Most information in `RTCFilterFunctionNArguments` maps
     * directly to elements of the EmbreeRayCaster class, but the mesh and instance IDs need special
     * conversion. `mesh_index` is an array of `args->N` EmbreeRayCaster mesh indices, and
     * `instance_index` is an array of `args->N` EmbreeRayCaster instance indices, one for each
     * ray/hit. For the other elements of `args`, the mappings are:
     *
     * @code
     * facet_index <-- primID
     * ray_depth <-- tfar
     * barycentric_coord <-- [u, v, 1 - u - v]
     * normal <-- Ng
     * @endcode
     */
    using FilterFunction = std::function<void(
        const EmbreeRayCaster* obj,
        const Index* mesh_index,
        const Index* instance_index,
        const RTCFilterFunctionNArguments* args)>;

private:
    /** Select between two sets of filters stored in the object. */
    enum { FILTER_INTERSECT = 0, FILTER_OCCLUDED = 1 };

public:
    /** Constructor. */
    EmbreeRayCaster(
        RTCSceneFlags scene_flags = RTC_SCENE_FLAG_DYNAMIC,
        RTCBuildQuality build_quality = RTC_BUILD_QUALITY_LOW)
    {
// Embree strongly recommend to have the Flush to Zero and
// Denormals are Zero mode of the MXCSR control and status
// register enabled for each thread before calling the
// rtcIntersect and rtcOccluded functions.
#ifdef _MM_SET_FLUSH_ZERO_MODE
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
#ifdef _MM_SET_DENORMALS_ZERO_MODE
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

        m_scene_flags = scene_flags;
        m_build_quality = build_quality;
        m_device = rtcNewDevice(NULL);
        ensure_no_errors_internal();
        m_embree_world_scene = rtcNewScene(m_device);
        ensure_no_errors_internal();
        m_instance_index_ranges.push_back(safe_cast<Index>(0));

        m_need_rebuild = true;
        m_need_commit = false;
    }

    /** Destructor. */
    virtual ~EmbreeRayCaster()
    {
        release_scenes();
        rtcReleaseDevice(m_device);
    }

    EmbreeRayCaster(const EmbreeRayCaster&) = delete;
    void operator=(const EmbreeRayCaster&) = delete;

public:
    /** Get the total number of meshes (not instances). */
    Index get_num_meshes() const { return safe_cast<Index>(m_meshes.size()); }

    /** Get the total number of mesh instances. */
    Index get_num_instances() const { return m_instance_index_ranges.back(); }

    /** Get the number of instances of a particular mesh. */
    Index get_num_instances(Index mesh_index) const
    {
        la_debug_assert(mesh_index + 1 < safe_cast<Index>(m_instance_index_ranges.size()));
        return safe_cast<Index>(
            m_instance_index_ranges[mesh_index + 1] - m_instance_index_ranges[mesh_index]);
    }

    /**
     * Get the mesh with a given index. Requires the caller to know the original type of the mesh
     * (@a MeshType) in advance.
     */
    template <typename MeshType>
    std::shared_ptr<MeshType> get_mesh(Index index) const
    {
        la_runtime_assert(index < safe_cast<Index>(m_meshes.size()));
        la_runtime_assert(m_meshes[index] != nullptr);
        return dynamic_cast<RaycasterMeshDerived<MeshType>&>(*m_meshes[index]).get_mesh_ptr();
    }

    /**
     * Get the index of the mesh corresponding to a given instance, where the instances are indexed
     * sequentially starting from the instances of the first mesh, then the instances of the second
     * mesh, and so on. Use get_mesh() to map the returned index to an actual mesh.
     *
     * @param cumulative_instance_index An integer in the range `0` to `get_num_instances() - 1`
     *     (both inclusive).
     */
    Index get_mesh_for_instance(Index cumulative_instance_index) const
    {
        la_runtime_assert(cumulative_instance_index < get_num_instances());
        return m_instance_to_user_mesh[cumulative_instance_index];
    }

    /**
     * Add an instance of a mesh to the scene, with a given transformation. If another instance of
     * the same mesh has been previously added to the scene, the two instances will NOT be
     * considered to share the same mesh, but will be treated as separate instances of separate
     * meshes. To add multiple instances of the same mesh, use add_meshes().
     */
    template <typename MeshType>
    Index add_mesh(
        std::shared_ptr<MeshType> mesh,
        const Transform& trans = Transform::Identity(),
        RTCBuildQuality build_quality = RTC_BUILD_QUALITY_MEDIUM)
    {
        return add_raycasting_mesh(
            std::make_unique<RaycasterMeshDerived<MeshType>>(mesh),
            trans,
            build_quality);
    }

    /**
     * Add multiple instances of a single mesh to the scene, with given transformations. If another
     * instance of the same mesh has been previously added to the scene, the new instances will NOT
     * be considered to share the same mesh as the old instance, but will be treated as instances of
     * a new mesh. Add all instances in a single add_meshes() call if you want to avoid this.
     */
    template <typename MeshType>
    Index add_meshes(
        std::shared_ptr<MeshType> mesh,
        const TransformVector& trans_vector,
        RTCBuildQuality build_quality = RTC_BUILD_QUALITY_MEDIUM)
    {
        m_meshes.push_back(std::move(std::make_unique<RaycasterMeshDerived<MeshType>>(mesh)));
        m_transforms.insert(m_transforms.end(), trans_vector.begin(), trans_vector.end());
        m_mesh_build_qualities.push_back(build_quality);
        m_visibility.resize(m_visibility.size() + trans_vector.size(), true);
        for (auto& f : m_filters) { // per-mesh, not per-instance
            f.push_back(nullptr);
        }
        Index mesh_index = safe_cast<Index>(m_meshes.size() - 1);
        la_runtime_assert(m_instance_index_ranges.size() > 0);
        Index instance_index = m_instance_index_ranges.back();
        la_runtime_assert(instance_index == safe_cast<Index>(m_instance_to_user_mesh.size()));
        Index new_instance_size = instance_index + trans_vector.size();
        m_instance_index_ranges.push_back(new_instance_size);
        m_instance_to_user_mesh.resize(new_instance_size, mesh_index);
        m_need_rebuild = true;
        return mesh_index;
    }

    /**
     * Update a particular mesh with a new mesh object. All its instances will be affected.
     *
     * @note If you have changed the vertices of a mesh already in the scene, and just want the
     *   object to reflect that, then call update_mesh_vertices() instead.
     */
    template <typename MeshType>
    void update_mesh(
        Index index,
        std::shared_ptr<MeshType> mesh,
        RTCBuildQuality build_quality = RTC_BUILD_QUALITY_MEDIUM)
    {
        update_raycasting_mesh(
            index,
            std::make_unique<RaycasterMeshDerived<MeshType>>(mesh),
            build_quality);
    }

    /**
     * Update the object to reflect external changes to the vertices of a particular mesh which is
     * already in the scene. All its instances will be affected. The number of vertices in the mesh,
     * and their order in the vertex array, must not change.
     */
    void update_mesh_vertices(Index index)
    {
        la_runtime_assert(index < safe_cast<Index>(m_meshes.size()));
        if (m_need_rebuild) return;

        la_runtime_assert(index < safe_cast<Index>(m_embree_mesh_scenes.size()));
        auto geom = rtcGetGeometry(m_embree_mesh_scenes[index], 0);

        // Update the vertex buffer in Embree
        auto const& mesh = m_meshes[index];
        la_runtime_assert(
            safe_cast<Index>(mesh->get_num_vertices()) == m_mesh_vertex_counts[index]);

        auto vbuf =
            reinterpret_cast<float*>(rtcGetGeometryBufferData(geom, RTC_BUFFER_TYPE_VERTEX, 0));
        la_runtime_assert(vbuf);
        mesh->vertices_to_float(vbuf);
        rtcUpdateGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0);

        // Re-commit the mesh geometry and scene
        rtcCommitGeometry(geom);
        rtcCommitScene(m_embree_mesh_scenes[index]);

        // Re-commit every instance of the mesh
        for (Index instance_index = m_instance_index_ranges[index];
             instance_index < m_instance_index_ranges[index + 1];
             ++instance_index) {
            Index rtc_inst_id = m_instance_index_ranges[index] + instance_index;
            auto geom_inst =
                rtcGetGeometry(m_embree_world_scene, static_cast<unsigned>(rtc_inst_id));
            rtcCommitGeometry(geom_inst);
        }

        // Mark the world scene as needing a re-commit (will be called lazily)
        m_need_commit = true;
    }

    /** Get the transform applied to a given mesh instance. */
    Transform get_transform(Index mesh_index, Index instance_index) const
    {
        la_runtime_assert(mesh_index + 1 < safe_cast<Index>(m_instance_index_ranges.size()));
        Index index = m_instance_index_ranges[mesh_index] + instance_index;
        la_runtime_assert(index < m_instance_index_ranges[mesh_index + 1]);
        la_runtime_assert(index < safe_cast<Index>(m_transforms.size()));
        return m_transforms[index];
    }

    /** Update the transform applied to a given mesh instance. */
    void update_transformation(Index mesh_index, Index instance_index, const Transform& trans)
    {
        la_runtime_assert(mesh_index + 1 < safe_cast<Index>(m_instance_index_ranges.size()));
        Index index = m_instance_index_ranges[mesh_index] + instance_index;
        la_runtime_assert(index < m_instance_index_ranges[mesh_index + 1]);
        la_runtime_assert(index < safe_cast<Index>(m_transforms.size()));
        m_transforms[index] = trans;
        if (!m_need_rebuild) {
            auto geom = rtcGetGeometry(m_embree_world_scene, static_cast<unsigned>(index));
            Eigen::Matrix<float, 4, 4> T = trans.template cast<float>();
            rtcSetGeometryTransform(geom, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, T.eval().data());
            rtcCommitGeometry(geom);
            m_need_commit = true;
        }
    }

    /** Get the visibility flag of a given mesh instance. */
    bool get_visibility(Index mesh_index, Index instance_index) const
    {
        la_runtime_assert(mesh_index + 1 < safe_cast<Index>(m_instance_index_ranges.size()));
        Index index = m_instance_index_ranges[mesh_index] + instance_index;
        la_runtime_assert(index < m_instance_index_ranges[mesh_index + 1]);
        la_runtime_assert(index < safe_cast<Index>(m_visibility.size()));
        return m_visibility[index];
    }

    /** Update the visibility of a given mesh index (true for visible, false for invisible). */
    void update_visibility(Index mesh_index, Index instance_index, bool visible)
    {
        la_runtime_assert(mesh_index + 1 < safe_cast<Index>(m_instance_index_ranges.size()));
        Index index = m_instance_index_ranges[mesh_index] + instance_index;
        la_runtime_assert(index < m_instance_index_ranges[mesh_index + 1]);
        la_runtime_assert(index < safe_cast<Index>(m_visibility.size()));
        m_visibility[index] = visible;
        if (!m_need_rebuild &&
            rtcGetDeviceProperty(m_device, RTC_DEVICE_PROPERTY_RAY_MASK_SUPPORTED)) {
            // ^^^ else, visibility will be checked by the already bound filter

            auto geom = rtcGetGeometry(m_embree_world_scene, static_cast<unsigned>(index));
            rtcSetGeometryMask(geom, visible ? 0xFFFFFFFF : 0x00000000);
            rtcCommitGeometry(geom);
            m_need_commit = true;
        }
    }

    /**
     * Set an intersection filter that is called for every hit on (every instance of) a mesh during
     * an intersection query. The @a filter function must be callable as:
     *
     * @code
     * void filter(const EmbreeRayCaster* obj, const Index* mesh_index, const Index* instance_index,
     *             const RTCFilterFunctionNArguments* args);
     * @endcode
     *
     * It functions exactly like Embree's `rtcSetGeometryIntersectFilterFunction`, except it also
     * receives a handle to this object, and mesh and instance indices specific to this object. A
     * null @a filter disables intersection filtering for this mesh.
     *
     * @note Embree dictates that filters can be associated only with meshes (raw geometries), not
     *    instances.
     */
    void set_intersection_filter(Index mesh_index, FilterFunction filter)
    {
        la_runtime_assert(mesh_index < m_filters[FILTER_INTERSECT].size());
        m_filters[FILTER_INTERSECT][mesh_index] = filter;
        m_need_rebuild = true;
    }

    /**
     * Get the intersection filter function currently bound to a given mesh.
     *
     * @note Embree dictates that filters can be associated only with meshes (raw geometries), not
     *    instances.
     */
    FilterFunction get_intersection_filter(Index mesh_index) const
    {
        la_runtime_assert(mesh_index < m_filters[FILTER_INTERSECT].size());
        return m_filters[FILTER_INTERSECT][mesh_index];
    }

    /**
     * Set an occlusion filter that is called for every hit on (every instance of) a mesh during an
     * occlusion query. The @a filter function must be callable as:
     *
     * @code
     * void filter(const EmbreeRayCaster* obj, const Index* mesh_index, const Index* instance_index,
     *             const RTCFilterFunctionNArguments* args);
     * @endcode
     *
     * It functions exactly like Embree's `rtcSetGeometryOccludedFilterFunction`, except it also
     * receives a handle to this object, and mesh and instance indices specific to this object. A
     * null @a filter disables occlusion filtering for this mesh.
     *
     * @note Embree dictates that filters can be associated only with meshes (raw geometries), not
     *    instances.
     */
    void set_occlusion_filter(Index mesh_index, FilterFunction filter)
    {
        la_runtime_assert(mesh_index < m_filters[FILTER_OCCLUDED].size());
        m_filters[FILTER_OCCLUDED][mesh_index] = filter;
        m_need_rebuild = true;
    }

    /**
     * Get the occlusion filter function currently bound to a given mesh.
     *
     * @note Embree dictates that filters can be associated only with meshes (raw geometries), not
     *    instances.
     */
    FilterFunction get_occlusion_filter(Index mesh_index) const
    {
        la_runtime_assert(mesh_index < m_filters[FILTER_OCCLUDED].size());
        return m_filters[FILTER_OCCLUDED][mesh_index];
    }

    /**
     * Call `rtcCommitScene()` on the overall scene, if it has been marked as modified.
     *
     * @todo Now that this is automatically called by update_internal() based on a dirty flag, can
     *   we make this a protected/private function? That would break the API so maybe reserve it
     *   for a major version.
     */
    void commit_scene_changes()
    {
        if (!m_need_commit) return;

        rtcCommitScene(m_embree_world_scene);
        m_need_commit = false;
    }

    /** Throw an exception if an Embree error has occurred.*/
    void ensure_no_errors() const { EmbreeHelper::ensure_no_errors(m_device); }

    /**
     * Cast a packet of up to 4 rays through the scene, returning full data of the closest
     * intersections including normals and instance indices.
     */
    uint32_t cast4(
        uint32_t batch_size,
        const Point4& origin,
        const Direction4& direction,
        const Mask4& mask,
        Index4& mesh_index,
        Index4& instance_index,
        Index4& facet_index,
        Scalar4& ray_depth,
        Point4& barycentric_coord,
        Point4& normal,
        const Scalar4& tmin = Scalar4::Zero(),
        const Scalar4& tmax = Scalar4::Constant(std::numeric_limits<Scalar>::infinity()))
    {
        la_debug_assert(batch_size <= 4);

        update_internal();

        RTCRayHit4 embree_raypacket;
        for (int i = 0; i < static_cast<int>(batch_size); ++i) {
            // Set ray origins
            embree_raypacket.ray.org_x[i] = static_cast<float>(origin(i, 0));
            embree_raypacket.ray.org_y[i] = static_cast<float>(origin(i, 1));
            embree_raypacket.ray.org_z[i] = static_cast<float>(origin(i, 2));

            // Set ray directions
            embree_raypacket.ray.dir_x[i] = static_cast<float>(direction(i, 0));
            embree_raypacket.ray.dir_y[i] = static_cast<float>(direction(i, 1));
            embree_raypacket.ray.dir_z[i] = static_cast<float>(direction(i, 2));

            // Misc
            embree_raypacket.ray.tnear[i] = static_cast<float>(tmin[i]);
            embree_raypacket.ray.tfar[i] = std::isinf(tmax[i]) ? std::numeric_limits<float>::max()
                                                               : static_cast<float>(tmax[i]);
            embree_raypacket.ray.mask[i] = 0xFFFFFFFF;
            embree_raypacket.ray.id[i] = static_cast<unsigned>(i);
            embree_raypacket.ray.flags[i] = 0;

            // Required initialization of the hit substructure
            embree_raypacket.hit.geomID[i] = RTC_INVALID_GEOMETRY_ID;
            embree_raypacket.hit.primID[i] = RTC_INVALID_GEOMETRY_ID;
            embree_raypacket.hit.instID[0][i] = RTC_INVALID_GEOMETRY_ID;
        }

        // Modify the mask to make 100% sure extra rays in the packet will be ignored
        auto packet_mask = mask;
        for (int i = static_cast<int>(batch_size); i < 4; ++i) packet_mask[i] = 0;

        ensure_no_errors_internal();
#ifdef LAGRANGE_WITH_EMBREE_4
        rtcIntersect4(packet_mask.data(), m_embree_world_scene, &embree_raypacket);
#else
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        rtcIntersect4(packet_mask.data(), m_embree_world_scene, &context, &embree_raypacket);
#endif
        ensure_no_errors_internal();

        uint32_t is_hits = 0;
        for (int i = 0; i < static_cast<int>(batch_size); ++i) {
            if (embree_raypacket.hit.geomID[i] != RTC_INVALID_GEOMETRY_ID) {
                Index rtc_inst_id = embree_raypacket.hit.instID[0][i];
                Index rtc_mesh_id = (rtc_inst_id == RTC_INVALID_GEOMETRY_ID)
                                        ? embree_raypacket.hit.geomID[i]
                                        : rtc_inst_id;
                assert(rtc_mesh_id < m_instance_to_user_mesh.size());
                assert(m_visibility[rtc_mesh_id]);
                mesh_index[i] = m_instance_to_user_mesh[rtc_mesh_id];
                assert(mesh_index[i] + 1 < m_instance_index_ranges.size());
                assert(mesh_index[i] < safe_cast<Index>(m_meshes.size()));
                instance_index[i] = rtc_mesh_id - m_instance_index_ranges[mesh_index[i]];
                facet_index[i] = embree_raypacket.hit.primID[i];
                ray_depth[i] = embree_raypacket.ray.tfar[i];
                barycentric_coord(i, 0) =
                    1.0f - embree_raypacket.hit.u[i] - embree_raypacket.hit.v[i];
                barycentric_coord(i, 1) = embree_raypacket.hit.u[i];
                barycentric_coord(i, 2) = embree_raypacket.hit.v[i];
                normal(i, 0) = embree_raypacket.hit.Ng_x[i];
                normal(i, 1) = embree_raypacket.hit.Ng_y[i];
                normal(i, 2) = embree_raypacket.hit.Ng_z[i];
                is_hits = is_hits | (1 << i);
            }
        }

        return is_hits;
    }

    /**
     * Cast a packet of up to 4 rays through the scene, returning data of the closest intersections
     * excluding normals and instance indices.
     */
    uint32_t cast4(
        uint32_t batch_size,
        const Point4& origin,
        const Direction4& direction,
        const Mask4& mask,
        Index4& mesh_index,
        Index4& facet_index,
        Scalar4& ray_depth,
        Point4& barycentric_coord,
        const Scalar4& tmin = Scalar4::Zero(),
        const Scalar4& tmax = Scalar4::Constant(std::numeric_limits<Scalar>::infinity()))
    {
        Index4 instance_index;
        Point4 normal;
        return cast4(
            batch_size,
            origin,
            direction,
            mask,
            mesh_index,
            instance_index,
            facet_index,
            ray_depth,
            barycentric_coord,
            normal,
            tmin,
            tmax);
    }

    /**
     * Cast a packet of up to 4 rays through the scene and check whether they hit anything or not.
     */
    uint32_t cast4(
        uint32_t batch_size,
        const Point4& origin,
        const Direction4& direction,
        const Mask4& mask,
        const Scalar4& tmin = Scalar4::Zero(),
        const Scalar4& tmax = Scalar4::Constant(std::numeric_limits<Scalar>::infinity()))
    {
        la_debug_assert(batch_size <= 4);

        update_internal();

        RTCRay4 embree_raypacket;
        for (int i = 0; i < static_cast<int>(batch_size); ++i) {
            // Set ray origins
            embree_raypacket.org_x[i] = static_cast<float>(origin(i, 0));
            embree_raypacket.org_y[i] = static_cast<float>(origin(i, 1));
            embree_raypacket.org_z[i] = static_cast<float>(origin(i, 2));

            // Set ray directions
            embree_raypacket.dir_x[i] = static_cast<float>(direction(i, 0));
            embree_raypacket.dir_y[i] = static_cast<float>(direction(i, 1));
            embree_raypacket.dir_z[i] = static_cast<float>(direction(i, 2));

            // Misc
            embree_raypacket.tnear[i] = static_cast<float>(tmin[i]);
            embree_raypacket.tfar[i] = std::isinf(tmax[i]) ? std::numeric_limits<float>::max()
                                                           : static_cast<float>(tmax[i]);
            embree_raypacket.mask[i] = 0xFFFFFFFF;
            embree_raypacket.id[i] = static_cast<unsigned>(i);
            embree_raypacket.flags[i] = 0;
        }

        // Modify the mask to make 100% sure extra rays in the packet will be ignored
        auto packet_mask = mask;
        for (int i = static_cast<int>(batch_size); i < 4; ++i) packet_mask[i] = 0;

        ensure_no_errors_internal();
#ifdef LAGRANGE_WITH_EMBREE_4
        rtcOccluded4(packet_mask.data(), m_embree_world_scene, &embree_raypacket);
#else
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        rtcOccluded4(packet_mask.data(), m_embree_world_scene, &context, &embree_raypacket);
#endif
        ensure_no_errors_internal();

        // If hit, the tfar field will be set to -inf.
        uint32_t is_hits = 0;
        for (uint32_t i = 0; i < batch_size; ++i)
            if (!std::isfinite(embree_raypacket.tfar[i])) is_hits = is_hits | (1 << i);

        return is_hits;
    }

    /**
     * Cast a single ray through the scene, returning full data of the closest intersection
     * including the normal and the instance index.
     */
    bool cast(
        const Point& origin,
        const Direction& direction,
        Index& mesh_index,
        Index& instance_index,
        Index& facet_index,
        Scalar& ray_depth,
        Point& barycentric_coord,
        Point& normal,
        Scalar tmin = 0,
        Scalar tmax = std::numeric_limits<Scalar>::infinity())
    {
        // Overloaded when specializing tnear and tfar

        update_internal();

        RTCRayHit embree_rayhit;
        embree_rayhit.ray.org_x = static_cast<float>(origin.x());
        embree_rayhit.ray.org_y = static_cast<float>(origin.y());
        embree_rayhit.ray.org_z = static_cast<float>(origin.z());
        embree_rayhit.ray.dir_x = static_cast<float>(direction.x());
        embree_rayhit.ray.dir_y = static_cast<float>(direction.y());
        embree_rayhit.ray.dir_z = static_cast<float>(direction.z());
        embree_rayhit.ray.tnear = static_cast<float>(tmin);
        embree_rayhit.ray.tfar =
            std::isinf(tmax) ? std::numeric_limits<float>::max() : static_cast<float>(tmax);
        embree_rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
        embree_rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;
        embree_rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
        embree_rayhit.ray.mask = 0xFFFFFFFF;
        embree_rayhit.ray.id = 0;
        embree_rayhit.ray.flags = 0;
        ensure_no_errors_internal();
#ifdef LAGRANGE_WITH_EMBREE_4
        rtcIntersect1(m_embree_world_scene, &embree_rayhit);
#else
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        rtcIntersect1(m_embree_world_scene, &context, &embree_rayhit);
#endif
        ensure_no_errors_internal();

        if (embree_rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
            Index rtc_inst_id = embree_rayhit.hit.instID[0];
            Index rtc_mesh_id =
                (rtc_inst_id == RTC_INVALID_GEOMETRY_ID) ? embree_rayhit.hit.geomID : rtc_inst_id;
            assert(rtc_mesh_id < m_instance_to_user_mesh.size());
            assert(m_visibility[rtc_mesh_id]);
            mesh_index = m_instance_to_user_mesh[rtc_mesh_id];
            assert(mesh_index + 1 < m_instance_index_ranges.size());
            assert(mesh_index < safe_cast<Index>(m_meshes.size()));
            instance_index = rtc_mesh_id - m_instance_index_ranges[mesh_index];
            facet_index = embree_rayhit.hit.primID;
            ray_depth = embree_rayhit.ray.tfar;
            barycentric_coord[0] = 1.0f - embree_rayhit.hit.u - embree_rayhit.hit.v;
            barycentric_coord[1] = embree_rayhit.hit.u;
            barycentric_coord[2] = embree_rayhit.hit.v;
            normal[0] = embree_rayhit.hit.Ng_x;
            normal[1] = embree_rayhit.hit.Ng_y;
            normal[2] = embree_rayhit.hit.Ng_z;
            return true;
        } else {
            // Ray missed.
            mesh_index = invalid<Index>();
            instance_index = invalid<Index>();
            facet_index = invalid<Index>();
            return false;
        }
    }

    /**
     * Cast a single ray through the scene, returning data of the closest intersection excluding the
     * normal and the instance index.
     */
    bool cast(
        const Point& origin,
        const Direction& direction,
        Index& mesh_index,
        Index& facet_index,
        Scalar& ray_depth,
        Point& barycentric_coord,
        Scalar tmin = 0,
        Scalar tmax = std::numeric_limits<Scalar>::infinity())
    {
        Index instance_index;
        Point normal;
        return cast(
            origin,
            direction,
            mesh_index,
            instance_index,
            facet_index,
            ray_depth,
            barycentric_coord,
            normal,
            tmin,
            tmax);
    }

    /** Cast a single ray through the scene and check whether it hits anything or not. */
    bool cast(
        const Point& origin,
        const Direction& direction,
        Scalar tmin = 0,
        Scalar tmax = std::numeric_limits<Scalar>::infinity())
    {
        update_internal();

        RTCRay embree_ray;
        embree_ray.org_x = static_cast<float>(origin.x());
        embree_ray.org_y = static_cast<float>(origin.y());
        embree_ray.org_z = static_cast<float>(origin.z());
        embree_ray.dir_x = static_cast<float>(direction.x());
        embree_ray.dir_y = static_cast<float>(direction.y());
        embree_ray.dir_z = static_cast<float>(direction.z());
        embree_ray.tnear = static_cast<float>(tmin);
        embree_ray.tfar =
            std::isinf(tmax) ? std::numeric_limits<float>::max() : static_cast<float>(tmax);
        embree_ray.mask = 0xFFFFFFFF;
        embree_ray.id = 0;
        embree_ray.flags = 0;

        ensure_no_errors_internal();
#ifdef LAGRANGE_WITH_EMBREE_4
        rtcOccluded1(m_embree_world_scene, &embree_ray);
#else
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        rtcOccluded1(m_embree_world_scene, &context, &embree_ray);
#endif
        ensure_no_errors_internal();

        // If hit, the tfar field will be set to -inf.
        return !std::isfinite(embree_ray.tfar);
    }

    /** Use the underlying BVH to find the point closest to a query point. */
    ClosestPoint query_closest_point(const Point& p) const;

    /** Add raycasting utilities **/
    Index add_raycasting_mesh(
        std::unique_ptr<RaycasterMesh> mesh,
        const Transform& trans = Transform::Identity(),
        RTCBuildQuality build_quality = RTC_BUILD_QUALITY_MEDIUM)
    {
        m_meshes.push_back(std::move(mesh));
        m_transforms.push_back(trans);
        m_mesh_build_qualities.push_back(build_quality);
        m_visibility.push_back(true);
        for (auto& f : m_filters) { // per-mesh, not per-instance
            f.push_back(nullptr);
        }
        Index mesh_index = safe_cast<Index>(m_meshes.size() - 1);
        la_runtime_assert(m_instance_index_ranges.size() > 0);
        Index instance_index = m_instance_index_ranges.back();
        la_runtime_assert(instance_index == safe_cast<Index>(m_instance_to_user_mesh.size()));
        m_instance_index_ranges.push_back(instance_index + 1);
        m_instance_to_user_mesh.resize(instance_index + 1, mesh_index);
        m_need_rebuild = true;
        return mesh_index;
    }

    void update_raycasting_mesh(
        Index index,
        std::unique_ptr<RaycasterMesh> mesh,
        RTCBuildQuality build_quality = RTC_BUILD_QUALITY_MEDIUM)
    {
        la_runtime_assert(mesh->get_dim() == 3);
        la_runtime_assert(mesh->get_vertex_per_facet() == 3);
        la_runtime_assert(index < safe_cast<Index>(m_meshes.size()));
        m_meshes[index] = std::move(mesh);
        m_mesh_build_qualities[index] = build_quality;
        m_need_rebuild = true; // TODO: Make this more fine-grained so only the affected part of
                               // the Embree scene is updated
    }

protected:
    /** Release internal Embree scenes */
    void release_scenes()
    {
        for (auto& s : m_embree_mesh_scenes) {
            rtcReleaseScene(s);
        }
        rtcReleaseScene(m_embree_world_scene);
    }

    /** Get the Embree scene flags. */
    virtual RTCSceneFlags get_scene_flags() const { return m_scene_flags; }

    /** Get the Embree geometry build quality. */
    virtual RTCBuildQuality get_scene_build_quality() const { return m_build_quality; }

    /** Update all internal structures based on the current dirty flags. */
    void update_internal()
    {
        if (m_need_rebuild)
            generate_scene(); // full rebuild
        else if (m_need_commit)
            commit_scene_changes(); // just call rtcCommitScene()
    }

    /**
     * Build the whole Embree scene from the specified meshes, instances, etc.
     *
     * @todo Make the dirty flags more fine-grained so that only the changed meshes are re-sent to
     *   Embree.
     */
    void generate_scene()
    {
        if (!m_need_rebuild) return;

        // Scene needs to be updated
        release_scenes();
        m_embree_world_scene = rtcNewScene(m_device);
        auto scene_flags = get_scene_flags(); // FIXME: or just m_scene_flags?
        auto scene_build_quality = get_scene_build_quality(); // FIXME: or just m_build_quality?
        rtcSetSceneFlags(
            m_embree_world_scene,
            scene_flags); // TODO: maybe also set RTC_SCENE_FLAG_CONTEXT_FILTER_FUNCTION
        rtcSetSceneBuildQuality(m_embree_world_scene, scene_build_quality);
        m_float_data.clear();
        m_int_data.clear();
        const auto num_meshes = m_meshes.size();
        la_runtime_assert(num_meshes + 1 == m_instance_index_ranges.size());
        m_embree_mesh_scenes.resize(num_meshes);
        m_mesh_vertex_counts.resize(num_meshes);
        ensure_no_errors_internal();

        bool is_mask_supported =
            rtcGetDeviceProperty(m_device, RTC_DEVICE_PROPERTY_RAY_MASK_SUPPORTED);
        for (size_t i = 0; i < num_meshes; i++) {
            // Initialize RTC meshes
            const auto& mesh = m_meshes[i];
            // const auto& vertices = mesh->get_vertices();
            // const auto& facets = mesh->get_facets();
            const Index num_vertices = m_mesh_vertex_counts[i] =
                safe_cast<Index>(mesh->get_num_vertices());
            const Index num_facets = safe_cast<Index>(mesh->get_num_facets());

            auto& embree_mesh_scene = m_embree_mesh_scenes[i];
            embree_mesh_scene = rtcNewScene(m_device);

            rtcSetSceneFlags(embree_mesh_scene, scene_flags);
            rtcSetSceneBuildQuality(embree_mesh_scene, scene_build_quality);
            ensure_no_errors_internal();

            RTCGeometry geom = rtcNewGeometry(
                m_device,
                RTC_GEOMETRY_TYPE_TRIANGLE); // EMBREE_FIXME: check if geometry gets properly
                                             // committed
            rtcSetGeometryBuildQuality(geom, m_mesh_build_qualities[i]);
            // rtcSetGeometryTimeStepCount(geom, 1);

            const float* vertex_data = extract_float_data(*mesh);
            const unsigned* facet_data = extract_int_data(*mesh);

            rtcSetSharedGeometryBuffer(
                geom,
                RTC_BUFFER_TYPE_VERTEX,
                0,
                RTC_FORMAT_FLOAT3,
                vertex_data,
                0,
                sizeof(float) * 3,
                num_vertices);
            rtcSetSharedGeometryBuffer(
                geom,
                RTC_BUFFER_TYPE_INDEX,
                0,
                RTC_FORMAT_UINT3,
                facet_data,
                0,
                sizeof(int) * 3,
                num_facets);

            set_intersection_filter(geom, m_filters[FILTER_INTERSECT][i], is_mask_supported);
            set_occlusion_filter(geom, m_filters[FILTER_OCCLUDED][i], is_mask_supported);

            rtcCommitGeometry(geom);
            rtcAttachGeometry(embree_mesh_scene, geom);
            rtcReleaseGeometry(geom);
            ensure_no_errors_internal();

            // Initialize RTC instances
            for (Index instance_index = m_instance_index_ranges[i];
                 instance_index < m_instance_index_ranges[i + 1];
                 ++instance_index) {
                const auto& trans = m_transforms[instance_index];

                RTCGeometry geom_inst = rtcNewGeometry(
                    m_device,
                    RTC_GEOMETRY_TYPE_INSTANCE); // EMBREE_FIXME: check if geometry gets properly
                                                 // committed
                rtcSetGeometryInstancedScene(geom_inst, embree_mesh_scene);
                rtcSetGeometryTimeStepCount(geom_inst, 1);

                Eigen::Matrix<float, 4, 4> T = trans.template cast<float>();
                rtcSetGeometryTransform(geom_inst, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, T.data());
                ensure_no_errors_internal();

                if (is_mask_supported) {
                    rtcSetGeometryMask(
                        geom_inst,
                        m_visibility[instance_index] ? 0xFFFFFFFF : 0x00000000);
                }
                ensure_no_errors_internal();

                rtcCommitGeometry(geom_inst);
                unsigned rtc_instance_id = rtcAttachGeometry(m_embree_world_scene, geom_inst);
                rtcReleaseGeometry(geom_inst);
                la_runtime_assert(safe_cast<Index>(rtc_instance_id) == instance_index);
                ensure_no_errors_internal();
            }

            rtcCommitScene(embree_mesh_scene);
            ensure_no_errors_internal();
        }
        rtcCommitScene(m_embree_world_scene);
        ensure_no_errors_internal();

        m_need_rebuild = m_need_commit = false;
    }

    /** Get the vertex data of a mesh as an array of floats. */
    const float* extract_float_data(const RaycasterMesh& mesh)
    {
        auto float_data = mesh.vertices_to_float();
        // Due to Embree bug, we have to have at least one more entry
        // after the bound.  Sigh...
        // See https://github.com/embree/embree/issues/124
        float_data.push_back(0.0);
        m_float_data.emplace_back(std::move(float_data));
        return m_float_data.back().data();
    }

    /** Get the index data of a mesh as an array of integers. */
    const unsigned* extract_int_data(const RaycasterMesh& mesh)
    {
        auto int_data = mesh.indices_to_int();
        // Due to Embree bug, we have to have at least one more entry
        // after the bound.  Sigh...
        // See https://github.com/embree/embree/issues/124
        int_data.push_back(0);
        m_int_data.emplace_back(std::move(int_data));
        return m_int_data.back().data();
    }

private:
    /**
     * Helper function for setting intersection filters. Does NOT commit the geometry. The caller
     * must explicitly call `rtcCommitGeometry()` afterwards.
     */
    void set_intersection_filter(RTCGeometry geom, FilterFunction filter, bool is_mask_supported)
    {
        if (is_mask_supported) {
            if (filter) {
                rtcSetGeometryUserData(geom, this);
                rtcSetGeometryIntersectFilterFunction(geom, &wrap_filter<FILTER_INTERSECT>);
            } else {
                rtcSetGeometryIntersectFilterFunction(geom, nullptr);
            }
        } else {
            rtcSetGeometryUserData(geom, this);
            rtcSetGeometryIntersectFilterFunction(geom, &wrap_filter_and_mask<FILTER_INTERSECT>);
        }
    }

    /**
     * Helper function for setting occlusion filters. Does NOT commit the geometry. The caller must
     * explicitly call `rtcCommitGeometry()` afterwards.
     */
    void set_occlusion_filter(RTCGeometry geom, FilterFunction filter, bool is_mask_supported)
    {
        if (is_mask_supported) {
            if (filter) {
                rtcSetGeometryUserData(geom, this);
                rtcSetGeometryOccludedFilterFunction(geom, &wrap_filter<FILTER_OCCLUDED>);
            } else {
                rtcSetGeometryOccludedFilterFunction(geom, nullptr);
            }
        } else {
            rtcSetGeometryUserData(geom, this);
            rtcSetGeometryOccludedFilterFunction(geom, &wrap_filter_and_mask<FILTER_OCCLUDED>);
        }
    }

    /**
     * Embree-compatible callback function that computes indices specific to this object and then
     * delegates to the user-specified filter function.
     */
    template <int IntersectionOrOcclusion> // 0: intersection, 1: occlusion
    static void wrap_filter(const RTCFilterFunctionNArguments* args)
    {
        // Embree never actually calls a filter callback with different geometry or instance IDs
        // So we can assume they are the same for all the hits in this batch. Also, every single
        // mesh in this class is instanced (never used raw), so we can ignore geomID.
        const auto* obj = reinterpret_cast<EmbreeRayCaster*>(args->geometryUserPtr);
        auto rtc_inst_id = RTCHitN_instID(args->hit, args->N, 0, 0);
        assert(rtc_inst_id < obj->m_instance_to_user_mesh.size());

        auto filter = obj->m_filters[IntersectionOrOcclusion][rtc_inst_id];
        if (!filter) {
            return;
        }

        Index mesh_index = obj->m_instance_to_user_mesh[rtc_inst_id];
        assert(mesh_index + 1 < obj->m_instance_index_ranges.size());
        assert(mesh_index < safe_cast<Index>(obj->m_meshes.size()));
        Index instance_index = rtc_inst_id - obj->m_instance_index_ranges[mesh_index];

        // In case Embree's implementation changes in the future, the callback should be written
        // generally, without assuming the single geometry/instance condition above.
        Index4 mesh_index4;
        mesh_index4.fill(mesh_index);
        Index4 instance_index4;
        instance_index4.fill(instance_index);

        // Call the wrapped filter with the indices specific to this object
        filter(obj, mesh_index4.data(), instance_index4.data(), args);
    }

    /**
     * Embree-compatible callback function that checks if the intersected object is visible or not,
     * computes indices specific to this object, and then delegates to the user-specified filter
     * function.
     */
    template <int IntersectionOrOcclusion> // 0: intersection, 1: occlusion
    static void wrap_filter_and_mask(const RTCFilterFunctionNArguments* args)
    {
        // Embree never actually calls a filter callback with different geometry or instance IDs
        // So we can assume they are the same for all the hits in this batch. Also, every single
        // mesh in this class is instanced (never used raw), so we can ignore geomID.
        const auto* obj = reinterpret_cast<EmbreeRayCaster*>(args->geometryUserPtr);
        auto rtc_inst_id = RTCHitN_instID(args->hit, args->N, 0, 0);
        if (!obj->m_visibility[rtc_inst_id]) {
            // Object is invisible. Make the hits of all the rays with this object invalid.
            std::fill(args->valid, args->valid + args->N, 0);
            return;
        }

        // Delegate to the regular filtering after having checked visibility
        wrap_filter<IntersectionOrOcclusion>(args);
    }

protected:
    RTCSceneFlags m_scene_flags;
    RTCBuildQuality m_build_quality;
    RTCDevice m_device;
    RTCScene m_embree_world_scene;
    bool m_need_rebuild; // full rebuild of the scene?
    bool m_need_commit; // just call rtcCommitScene() on the scene?

    // Data reservoirs for holding temporary/casted/per-geometry data.
    // Length = Number of polygonal meshes
    std::vector<FloatData> m_float_data;
    std::vector<IntData> m_int_data;
    std::vector<std::unique_ptr<RaycasterMesh>> m_meshes;
    std::vector<RTCBuildQuality> m_mesh_build_qualities;
    std::vector<RTCScene> m_embree_mesh_scenes;
    std::vector<Index> m_mesh_vertex_counts; // for bounds-checking of buffer updates
    std::vector<FilterFunction> m_filters[2]; // 0: intersection filters, 1: occlusion filters

    // Ranges of instance indices corresponding to a specific
    // Mesh. For example, in a scenario with 3 meshes each of
    // which has 1, 2, 5 instances, this array would be
    // [0, 1, 3, 8].
    // Length = Number of user meshes + 1
    std::vector<Index> m_instance_index_ranges;

    // Mapping from (RTC-)instanced mesh to user mesh. For
    // example, in a scenario with 3 meshes each of
    // which has 1, 2, 5 instances, this array would be
    // [0, 1, 1, 2, 2, 2, 2, 2]
    // Length = Number of instanced meshes
    // Note: This array is only used internally. We shouldn't
    //       allow the users to access anything with the indices
    //       used in those RTC functions.
    std::vector<Index> m_instance_to_user_mesh;

    // Data reservoirs for holding instanced mesh data
    // Length = Number of (world-space) instanced meshes
    std::vector<Transform> m_transforms;
    std::vector<bool> m_visibility;

    // error checking function used internally
    void ensure_no_errors_internal() const
    {
#ifdef LAGRANGE_EMBREE_DEBUG
        EmbreeHelper::ensure_no_errors(m_device);
#endif
    }
};

////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
////////////////////////////////////////////////////////////////////////////////

template <typename Scalar>
auto EmbreeRayCaster<Scalar>::query_closest_point(const Point& p) const -> ClosestPoint
{
    RTCPointQuery query;
    query.x = (float)(p.x());
    query.y = (float)(p.y());
    query.z = (float)(p.z());
    query.radius = std::numeric_limits<float>::max();
    query.time = 0.f;
    ensure_no_errors_internal();

    ClosestPoint result;

    // Callback to retrieve triangle corner positions
    result.populate_triangle =
        [&](unsigned mesh_index, unsigned facet_index, Point& v0, Point& v1, Point& v2) {
            // TODO: There's no way to call this->get_mesh<> since we need to template the function
            // by the (derived) type, which we don't know here... This means our only choice is so
            // use the float data instead of the (maybe) double point coordinate if available. Note
            // that we could also call rtcSetGeometryPointQueryFunction() when we register our mesh,
            // since we know the derived type at this point.
            const unsigned* face = m_int_data[mesh_index].data() + 3 * facet_index;
            const float* vertices = m_float_data[mesh_index].data();
            v0 = Point(vertices[3 * face[0]], vertices[3 * face[0] + 1], vertices[3 * face[0] + 2]);
            v1 = Point(vertices[3 * face[1]], vertices[3 * face[1] + 1], vertices[3 * face[1] + 2]);
            v2 = Point(vertices[3 * face[2]], vertices[3 * face[2] + 1], vertices[3 * face[2] + 2]);
        };

    {
        RTCPointQueryContext context;
        rtcInitPointQueryContext(&context);
        rtcPointQuery(
            m_embree_world_scene,
            &query,
            &context,
            &embree_closest_point<Scalar>,
            reinterpret_cast<void*>(&result));
        assert(
            result.mesh_index != RTC_INVALID_GEOMETRY_ID ||
            result.facet_index != RTC_INVALID_GEOMETRY_ID);
        assert(
            result.mesh_index != invalid<unsigned>() || result.facet_index != invalid<unsigned>());
    }
    ensure_no_errors_internal();

    return result;
}

} // namespace raycasting
} // namespace lagrange
