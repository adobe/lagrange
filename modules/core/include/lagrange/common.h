/*
 * Copyright 2016 Adobe. All rights reserved.
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

#include <lagrange/utils/invalid.h>

#include <limits>
#include <memory>

#include <Eigen/Core>

namespace lagrange {

using Vertices2D = Eigen::Matrix<double, Eigen::Dynamic, 2, Eigen::RowMajor>;
using Vertices3D = Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor>;
using Vertices2Df = Eigen::Matrix<float, Eigen::Dynamic, 2, Eigen::RowMajor>;
using Vertices3Df = Eigen::Matrix<float, Eigen::Dynamic, 3, Eigen::RowMajor>;
using Triangles = Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor>;
using Quads = Eigen::Matrix<int, Eigen::Dynamic, 4, Eigen::RowMajor>;

template <typename VertexArray, typename FacetArray>
class Mesh;
using TriangleMesh3D = Mesh<Vertices3D, Triangles>;
using TriangleMesh2D = Mesh<Vertices2D, Triangles>;
using TriangleMesh3Df = Mesh<Vertices3Df, Triangles>;
using TriangleMesh2Df = Mesh<Vertices2Df, Triangles>;
using QuadMesh3D = Mesh<Vertices3D, Quads>;
using QuadMesh2D = Mesh<Vertices2D, Quads>;
using QuadMesh3Df = Mesh<Vertices3Df, Quads>;
using QuadMesh2Df = Mesh<Vertices2Df, Quads>;

template <class T>
using ScalarOf = typename T::Scalar;

template <class T>
using IndexOf = typename T::Index;

template <class T>
using VertexArrayOf = typename T::VertexArray;

template <class T>
using FacetArrayOf = typename T::FacetArray;

template <class T>
using AttributeArrayOf = typename T::AttributeArray;

/**
 * Move data from one Eigen obj to another.
 * Both objects will be in valid state after the move.
 */
template <typename Derived1, typename Derived2>
void move_data(Eigen::DenseBase<Derived1>& from, Eigen::DenseBase<Derived2>& to)
{
#if 0
    // Uncomment this block of code to assert that the data pointers can be swapped.
    // Currently, we have a couple of examples/tests that fail to compile if we enable this assert.
    // Ideally maybe we'd like to have static_warning that produces an informative message at
    // compilation time...
    enum {
        SwapPointers = Eigen::internal::is_same<Derived1, Derived2>::value &&
                       Eigen::MatrixBase<Derived1>::SizeAtCompileTime == Eigen::Dynamic
    };
    static_assert(SwapPointers == true, "cannot swap matrix data pointers");
#endif
    to.derived() = std::move(from.derived());
    from.derived().resize(0, Eigen::NoChange);
}

/**
 * Helper for automatic type deduction for
 * unique_ptr to shared_ptr conversion.
 */

template <class T>
std::shared_ptr<T> to_shared_ptr(std::unique_ptr<T>&& ptr)
{
    return std::shared_ptr<T>(ptr.release());
}

/**
 * Compilers might complain about static_assert(false, "").
 * If you have a template function, you can use
 * static_assert(StaticAssertableBool<T>::False, "") instead.
 */
template <typename... Args>
struct StaticAssertableBool
{
    constexpr static bool False = false;
    constexpr static bool True = true;
};

} // namespace lagrange
