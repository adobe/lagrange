/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <memory>
#include <string>

#include <lagrange/common.h>

namespace lagrange {

class ExactPredicates
{
public:
    ///
    /// Factory method to create an exact predicate engine. Currently only the only engine supported
    /// is "shewchuk", so you might as well create it directly.
    ///
    /// @param[in]  engine  Engine name.
    ///
    /// @return     The created engine.
    ///
    static std::unique_ptr<ExactPredicates> create(const std::string& engine);

public:
    ExactPredicates() = default;
    virtual ~ExactPredicates() = default;

public:
    ///
    /// Tests whether p1, p2, and p3 are collinear in 3D. This works by calling orient2D
    /// successively on the xy, yz and zx projections of p1, p2 p3.
    ///
    /// @param      p1    First 3D point.
    /// @param      p2    Second 3D point.
    /// @param      p3    Third 3D point.
    ///
    /// @return     1 if the points are collinear, 0 otherwise.
    ///
    virtual short collinear3D(double p1[3], double p2[3], double p3[3]) const;

    ///
    /// Exact 2D orientation test.
    ///
    /// @param      p1    First 2D point.
    /// @param      p2    Second 2D point.
    /// @param      p3    Third 2D point.
    ///
    /// @return     Return a positive value if the points p1, p2, and p3 occur in counterclockwise
    ///             order; a negative value if they occur in clockwise order; and zero if they are
    ///             collinear.
    ///
    virtual short orient2D(double p1[2], double p2[2], double p3[2]) const = 0;

    ///
    /// Exact 3D orientation test.
    ///
    /// @param      p1    First 3D point.
    /// @param      p2    Second 3D point.
    /// @param      p3    Third 3D point.
    /// @param      p4    Fourth 3D point.
    ///
    /// @return     Return a positive value if the point pd lies below the plane passing through p1,
    ///             p2, and p3; "below" is defined so that p1, p2, and p3 appear in counterclockwise
    ///             order when viewed from above the plane.  Returns a negative value if pd lies
    ///             above the plane.  Returns zero if the points are coplanar.
    ///
    virtual short orient3D(double p1[3], double p2[3], double p3[3], double p4[3]) const = 0;

    ///
    /// Exact 2D incircle test.
    ///
    /// @param      p1    First 2D point.
    /// @param      p2    Second 2D point.
    /// @param      p3    Third 2D point.
    /// @param      p4    Fourth 3D point.
    ///
    /// @return     Return a positive value if the point pd lies inside the circle passing through
    ///             p1, p2, and p3; a negative value if it lies outside; and zero if the four points
    ///             are cocircular. The points p1, p2, and p3 must be in counterclockwise order, or
    ///             the sign of the result will be reversed.
    ///
    virtual short incircle(double p1[2], double p2[2], double p3[2], double p4[2]) const = 0;

    ///
    /// Exact 3D insphere test.
    ///
    /// @param      p1    First 3D point.
    /// @param      p2    Second 3D point.
    /// @param      p3    Third 3D point.
    /// @param      p4    Fourth 3D point.
    /// @param      p5    Fifth 3D point.
    ///
    /// @return     Return a positive value if the point p5 lies inside the sphere passing through
    ///             p1, p2, p3, and p4; a negative value if it lies outside; and zero if the five
    ///             points are cospherical.  The points p1, p2, p3, and p4 must be ordered so that
    ///             they have a positive orientation (as defined by orient3d()), or the sign of the
    ///             result will be reversed.
    ///
    virtual short insphere(double p1[3], double p2[3], double p3[3], double p4[3], double p5[3])
        const = 0;
};
} // namespace lagrange
