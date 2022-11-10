// Source: https://github.com/embree/embree/blob/master/tutorials/closest_point/closest_point.cpp
// SPDX-License-Identifier: Apache-2.0
//
// This file has been modified by Adobe.
//
// All modifications are Copyright 2020 Adobe.

#pragma once

#include <lagrange/raycasting/ClosestPointResult.h>

#include <lagrange/point_triangle_squared_distance.h>

#include <embree3/rtcore.h>
#include <embree3/rtcore_geometry.h>
#include <embree3/rtcore_ray.h>
#include <Eigen/Geometry>

namespace lagrange {
namespace raycasting {

template <typename Scalar>
bool embree_closest_point(RTCPointQueryFunctionArguments* args)
{
    using Point = typename ClosestPointResult<Scalar>::Point;
    using MapType = Eigen::Map<Eigen::Matrix4f, Eigen::Aligned16>;
    using AffineMat = Eigen::Transform<Scalar, 3, Eigen::Affine>;

    assert(args->userPtr);
    auto result = reinterpret_cast<ClosestPointResult<Scalar>*>(args->userPtr);

    const unsigned int geomID = args->geomID;
    const unsigned int primID = args->primID;

    RTCPointQueryContext* context = args->context;
    const unsigned int stack_size = args->context->instStackSize;
    const unsigned int stack_ptr = stack_size - 1;

    AffineMat inst2world =
        (stack_size > 0 ? AffineMat(MapType(context->inst2world[stack_ptr]).template cast<Scalar>())
                        : AffineMat::Identity());

    // Query position in world space
    Point q(args->query->x, args->query->y, args->query->z);

    // Get triangle information in local space
    Point v0, v1, v2;
    assert(result->populate_triangle);
    result->populate_triangle(geomID, primID, v0, v1, v2);

    // Bring query and primitive data in the same space if necessary.
    if (stack_size > 0 && args->similarityScale > 0) {
        // Instance transform is a similarity transform, therefore we
        // can compute distance information in instance space. Therefore,
        // transform query position into local instance space.
        AffineMat world2inst(MapType(context->world2inst[stack_ptr]).template cast<Scalar>());
        q = world2inst * q;
    } else if (stack_size > 0) {
        // Instance transform is not a similarity tranform. We have to transform the
        // primitive data into world space and perform distance computations in
        // world space to ensure correctness.
        v0 = inst2world * v0;
        v1 = inst2world * v1;
        v2 = inst2world * v2;
    } else {
        // Primitive is not instanced, therefore point query and primitive are
        // already in the same space.
    }

    // Determine distance to closest point on triangle, and transform in
    // world space if necessary.
    Point p;
    Scalar l1, l2, l3;
    Scalar d2 = point_triangle_squared_distance(q, v0, v1, v2, p, l1, l2, l3);
    float d = std::sqrt(static_cast<float>(d2));
    if (args->similarityScale > 0) {
        d = d / args->similarityScale;
    }

    // Store result in userPtr and update the query radius if we found a point
    // closer to the query position. This is optional but allows for faster
    // traversal (due to better culling).
    if (d < args->query->radius) {
        args->query->radius = d;
        result->closest_point = (args->similarityScale > 0 ? (inst2world * p).eval() : p);
        result->mesh_index = geomID;
        result->facet_index = primID;
        result->barycentric_coord = Point(l1, l2, l3);
        return true; // Return true to indicate that the query radius changed.
    }

    return false;
}

} // namespace raycasting
} // namespace lagrange
