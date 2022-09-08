/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <type_traits>

namespace lagrange {
namespace MeshTraitHelper {

// It turns out that what we are doing here is pretty standard since
// c++17! https://en.cppreference.com/w/cpp/types/void_t


// This type allows asking for the type void only if certain types exist
// Void<double, int>::type -> void
// Void<IAmATypeThatDontExist>::type -> compiler error
template <typename... Ts>
struct Void
{
    using type = void;
};

// ================ Is mesh

// This block of code defines is_mesh_helper<T, AnyOtherType>
// to be std::false_type.
// But since in this application we only set the AnyOtherType to void,
// let's check it with a static_assert() anyway.
template <typename T, typename _>
struct is_mesh_helper : std::false_type
{
    static_assert(std::is_same<_, void>::value, "second template argument is always void");
};

// This block attemps to override is_mesh_helper<T, void> (i.e,
// special case of AnyOtherType=void). This only succeeds if the
// expression Void<typename T::VertexArray, typename T::FacetArray>::type,
// is not erronous to the compiler (i.e., T::VertexArray and T::FacetArray
// exist).
template <typename MeshType>
struct is_mesh_helper<
    MeshType,
    typename Void<typename MeshType::VertexArray, typename MeshType::FacetArray>::type>
    : public std::true_type
{
};

// The trait to detect if a type T is a mesh.
// We just use  is_mesh_helper<T, void>.
// If the compiler can specialize the second void specialization, we get
// std::true_type. Otherwise, we get std::false_type.
template <typename T>
using is_mesh = is_mesh_helper<T, void>;


// ================ Is mesh smart ptr
// Works very similar to Is mesh

template <typename T, typename _>
struct is_mesh_smart_ptr_helper : std::false_type
{
    static_assert(std::is_same<_, void>::value, "second template argument is always void");
};

template <typename MeshTypePtr>
struct is_mesh_smart_ptr_helper<
    MeshTypePtr,
    typename Void<
        typename MeshTypePtr::element_type,
        typename MeshTypePtr::element_type::VertexArray,
        typename MeshTypePtr::element_type::FacetArray>::type> : public std::true_type
{
};

template <typename T>
using is_mesh_smart_ptr = is_mesh_smart_ptr_helper<T, void>;

// ================ Is mesh raw ptr
// We simply check if the type is ptr and then the type
// it points to is a mesh.

template <typename MeshTypePtr>
using is_mesh_raw_ptr = std::conditional_t<
    std::is_pointer<MeshTypePtr>::value && is_mesh<std::remove_pointer_t<MeshTypePtr>>::value,
    std::true_type,
    std::false_type>;

} // End of namespace MeshTraitHelper


/**
 * MeshTrait class provide compiler check for different mesh types.
 *
 * Usage:
 *     static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not mesh");
 */
template <typename MeshType_>
struct MeshTrait
{
    using MeshType = typename std::remove_reference<typename std::remove_cv<MeshType_>::type>::type;

    static constexpr bool is_mesh() { return lagrange::MeshTraitHelper::is_mesh<MeshType>::value; }

    static constexpr bool is_mesh_smart_ptr()
    {
        return lagrange::MeshTraitHelper::is_mesh_smart_ptr<MeshType>::value;
    }

    static constexpr bool is_mesh_raw_ptr()
    {
        return lagrange::MeshTraitHelper::is_mesh_raw_ptr<MeshType>::value;
    }

    static constexpr bool is_mesh_ptr() { return is_mesh_raw_ptr() || is_mesh_smart_ptr(); }
};

} // namespace lagrange
