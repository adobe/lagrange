/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/BitField.h>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-iterate Attributes iterators
/// @ingroup    group-surfacemesh
///
/// Iterating over mesh attributes. These utility functions use generic lambdas to iterate over mesh
/// attributes of different types. Because a generic lambda is templated, the type of each attribute
/// is accessible at compile type within the lambda function:
///
/// @code
/// seq_foreach_attribute_read(mesh, [](auto&& attr) {
///     using AttributeType = std::decay_t<decltype(attr)>;
///     using ValueType = typename AttributeType::ValueType;
///     lagrange::logger().info("Mesh attribute value type has size {}", sizeof(ValueType));
/// });
/// @endcode
///
/// To iterate over mesh attributes while accessing their names, use the `named_` variants:
///
/// @code
/// seq_foreach_named_attribute_read(mesh, [&](std::string_view name, auto&& attr) {
///     lagrange::logger().info(
///         "Mesh attribute named '{} with {} rows', name, attr.get_num_elements()");
/// });
/// @endcode
///
/// There are read and write variants of the functions above, depending on what type of access is
/// needed over the mesh attributes. Finally, the `par_*` variants call the visitor function in
/// parallel for each mesh attribute.
///
/// @{

/// @cond LA_INTERNAL_DOCS
namespace details {

enum class Ordering { Sequential, Parallel };
enum class Access { Write, Read };

void par_foreach_attribute_id(span<const AttributeId> ids, function_ref<void(AttributeId)> cb);

template <
    std::underlying_type_t<AttributeElement> mask,
    Ordering ordering,
    Access access,
    typename MeshType,
    typename Visitor>
void internal_foreach_named_attribute(
    MeshType& mesh,
    Visitor&& vis,
    span<const AttributeId> ids = {})
{
    auto cb = [&](std::string_view name, AttributeId id) {
        constexpr auto filter = BitField<AttributeElement>(mask);
#define LA_X_match_attribute(_, T)                                             \
    if (mesh.template is_attribute_type<T>(id)) {                              \
        if constexpr (filter.test(AttributeElement::Indexed)) {                \
            if (mesh.is_attribute_indexed(id)) {                               \
                auto& attr = mesh.template get_indexed_attribute<T>(id);       \
                if (filter.test(attr.get_element_type())) {                    \
                    if constexpr (access == Access::Write) {                   \
                        vis(name, mesh.template ref_indexed_attribute<T>(id)); \
                    } else {                                                   \
                        vis(name, attr);                                       \
                    }                                                          \
                }                                                              \
            }                                                                  \
        }                                                                      \
        if constexpr (filter.test_any(~AttributeElement::Indexed)) {           \
            if (!mesh.is_attribute_indexed(id)) {                              \
                auto& attr = mesh.template get_attribute<T>(id);               \
                if (filter.test(attr.get_element_type())) {                    \
                    if constexpr (access == Access::Write) {                   \
                        vis(name, mesh.template ref_attribute<T>(id));         \
                    } else {                                                   \
                        vis(name, attr);                                       \
                    }                                                          \
                }                                                              \
            }                                                                  \
        }                                                                      \
    }
        LA_ATTRIBUTE_X(match_attribute, 0)
#undef LA_X_match_attribute
    };
    if constexpr (ordering == Ordering::Sequential) {
        if (ids.empty()) {
            mesh.seq_foreach_attribute_id(cb);
        } else {
            std::for_each(ids.begin(), ids.end(), [&](AttributeId id) {
                cb(mesh.get_attribute_name(id), id);
            });
        }
    } else {
        if (ids.empty()) {
            mesh.par_foreach_attribute_id(cb);
        } else {
            par_foreach_attribute_id(ids, [&](AttributeId id) {
                cb(mesh.get_attribute_name(id), id);
            });
        }
    }
}

template <
    std::underlying_type_t<AttributeElement> mask,
    Ordering ordering,
    Access access,
    typename MeshType,
    typename Visitor>
void internal_foreach_attribute(MeshType& mesh, Visitor&& vis, span<const AttributeId> ids = {})
{
    auto cb = [&](AttributeId id) {
        constexpr auto filter = BitField<AttributeElement>(mask);
#define LA_X_match_attribute(_, T)                                       \
    if (mesh.template is_attribute_type<T>(id)) {                        \
        if constexpr (filter.test(AttributeElement::Indexed)) {          \
            if (mesh.is_attribute_indexed(id)) {                         \
                auto& attr = mesh.template get_indexed_attribute<T>(id); \
                if (filter.test(attr.get_element_type())) {              \
                    if constexpr (access == Access::Write) {             \
                        vis(mesh.template ref_indexed_attribute<T>(id)); \
                    } else {                                             \
                        vis(attr);                                       \
                    }                                                    \
                }                                                        \
            }                                                            \
        }                                                                \
        if constexpr (filter.test_any(~AttributeElement::Indexed)) {     \
            if (!mesh.is_attribute_indexed(id)) {                        \
                auto& attr = mesh.template get_attribute<T>(id);         \
                if (filter.test(attr.get_element_type())) {              \
                    if constexpr (access == Access::Write) {             \
                        vis(mesh.template ref_attribute<T>(id));         \
                    } else {                                             \
                        vis(attr);                                       \
                    }                                                    \
                }                                                        \
            }                                                            \
        }                                                                \
    }
        LA_ATTRIBUTE_X(match_attribute, 0)
#undef LA_X_match_attribute
    };
    if constexpr (ordering == Ordering::Sequential) {
        if (ids.empty()) {
            mesh.seq_foreach_attribute_id(cb);
        } else {
            std::for_each(ids.begin(), ids.end(), cb);
        }
    } else {
        if (ids.empty()) {
            mesh.par_foreach_attribute_id(cb);
        } else {
            par_foreach_attribute_id(ids, cb);
        }
    }
}

} // namespace details
/// @endcond

////////////////////////////////////////////////////////////////////////////////
/// @name Sequential iteration for read operations
/// @{
////////////////////////////////////////////////////////////////////////////////

///
/// Applies a function to each attribute of a mesh. The visitor function iterates over a pair of
/// (name x attribute).
///
/// @param[in]  mesh     Mesh on whose attribute to apply the visitor function.
/// @param[in]  vis      Visitor function to apply to each mesh attribute. Typically a generic
///                      lambda.
///
/// @tparam     mask     Bit field mask to filter attribute based on their element types.
/// @tparam     Visitor  Type of the visitor function, which should be `template<typename T>
///                      void(std::string_view, const Attribute<T> &);`
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
template <
    std::underlying_type_t<AttributeElement> mask = BitField<AttributeElement>::all(),
    typename Visitor,
    typename Scalar,
    typename Index>
void seq_foreach_named_attribute_read(const SurfaceMesh<Scalar, Index>& mesh, Visitor&& vis)
{
    using namespace details;
    internal_foreach_named_attribute<mask, Ordering::Sequential, Access::Read>(
        mesh,
        std::forward<Visitor>(vis));
}

///
/// Applies a function to each attribute of a mesh.
///
/// @param[in]  mesh     Mesh on whose attribute to apply the visitor function.
/// @param[in]  vis      Visitor function to apply to each mesh attribute. Typically a generic
///                      lambda.
///
/// @tparam     mask     Bit field mask to filter attribute based on their element types.
/// @tparam     Visitor  Type of the visitor function, which should be `template<typename T>
///                      void(const Attribute<T> &);`
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
template <
    std::underlying_type_t<AttributeElement> mask = BitField<AttributeElement>::all(),
    typename Visitor,
    typename Scalar,
    typename Index>
void seq_foreach_attribute_read(const SurfaceMesh<Scalar, Index>& mesh, Visitor&& vis)
{
    using namespace details;
    internal_foreach_attribute<mask, Ordering::Sequential, Access::Read>(
        mesh,
        std::forward<Visitor>(vis));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
/// @name Sequential iteration for write operations
/// @{
////////////////////////////////////////////////////////////////////////////////

///
/// Applies a function to each attribute of a mesh. The visitor function iterates over a pair of
/// (name x attribute). In this variant, the attribute is passed as a writable reference to the
/// visitor function.
///
/// @param[in]  mesh     Mesh on whose attribute to apply the visitor function.
/// @param[in]  vis      Visitor function to apply to each mesh attribute. Typically a generic
///                      lambda.
///
/// @tparam     mask     Bit field mask to filter attribute based on their element types.
/// @tparam     Visitor  Type of the visitor function, which should be `template<typename T>
///                      void(std::string_view, const Attribute<T> &);`
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
template <
    std::underlying_type_t<AttributeElement> mask = BitField<AttributeElement>::all(),
    typename Visitor,
    typename Scalar,
    typename Index>
void seq_foreach_named_attribute_write(SurfaceMesh<Scalar, Index>& mesh, Visitor&& vis)
{
    using namespace details;
    internal_foreach_named_attribute<mask, Ordering::Sequential, Access::Write>(
        mesh,
        std::forward<Visitor>(vis));
}

///
/// Applies a function to each attribute of a mesh. In this variant, the attribute is passed as a
/// writable reference to the visitor function.
///
/// @param[in,out] mesh     Mesh on whose attribute to apply the visitor function.
/// @param[in]     vis      Visitor function to apply to each mesh attribute. Typically a generic
///                         lambda.
///
/// @tparam        mask     Bit field mask to filter attribute based on their element types.
/// @tparam        Visitor  Type of the visitor function, which should be `template<typename T>
///                         void(Attribute<T> &);`
/// @tparam        Scalar   Mesh scalar type.
/// @tparam        Index    Mesh index type.
///
template <
    std::underlying_type_t<AttributeElement> mask = BitField<AttributeElement>::all(),
    typename Visitor,
    typename Scalar,
    typename Index>
void seq_foreach_attribute_write(SurfaceMesh<Scalar, Index>& mesh, Visitor&& vis)
{
    using namespace details;
    internal_foreach_attribute<mask, Ordering::Sequential, Access::Write>(
        mesh,
        std::forward<Visitor>(vis));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
/// @name Parallel iteration for read operations
/// @{
////////////////////////////////////////////////////////////////////////////////

///
/// Applies a function in parallel to each attribute of a mesh. The visitor function iterates over a
/// pair of (name x attribute).
///
/// @param[in]  mesh     Mesh on whose attribute to apply the visitor function.
/// @param[in]  vis      Visitor function to apply to each mesh attribute. Typically a generic
///                      lambda.
///
/// @tparam     mask     Bit field mask to filter attribute based on their element types.
/// @tparam     Visitor  Type of the visitor function, which should be `template<typename T>
///                      void(std::string_view, const Attribute<T> &);`
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
template <
    std::underlying_type_t<AttributeElement> mask = BitField<AttributeElement>::all(),
    typename Visitor,
    typename Scalar,
    typename Index>
void par_foreach_named_attribute_read(const SurfaceMesh<Scalar, Index>& mesh, Visitor&& vis)
{
    using namespace details;
    internal_foreach_named_attribute<mask, Ordering::Parallel, Access::Read>(
        mesh,
        std::forward<Visitor>(vis));
}

///
/// Applies a function in parallel to each attribute of a mesh.
///
/// @param[in]  mesh     Mesh on whose attribute to apply the visitor function.
/// @param[in]  vis      Visitor function to apply to each mesh attribute. Typically a generic
///                      lambda.
///
/// @tparam     mask     Bit field mask to filter attribute based on their element types.
/// @tparam     Visitor  Type of the visitor function, which should be `template<typename T>
///                      void(const Attribute<T> &);`
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
template <
    std::underlying_type_t<AttributeElement> mask = BitField<AttributeElement>::all(),
    typename Visitor,
    typename Scalar,
    typename Index>
void par_foreach_attribute_read(const SurfaceMesh<Scalar, Index>& mesh, Visitor&& vis)
{
    using namespace details;
    internal_foreach_attribute<mask, Ordering::Parallel, Access::Read>(
        mesh,
        std::forward<Visitor>(vis));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
/// @name Parallel iteration for write operations
/// @{
////////////////////////////////////////////////////////////////////////////////

///
/// Applies a function in parallel to each attribute of a mesh. The visitor function iterates over a
/// pair of (name x attribute). In this variant, the attribute is passed as a writable reference to
/// the visitor function.
///
/// @param[in]  mesh     Mesh on whose attribute to apply the visitor function.
/// @param[in]  vis      Visitor function to apply to each mesh attribute. Typically a generic
///                      lambda.
///
/// @tparam     mask     Bit field mask to filter attribute based on their element types.
/// @tparam     Visitor  Type of the visitor function, which should be `template<typename T>
///                      void(std::string_view, const Attribute<T> &);`
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
template <
    std::underlying_type_t<AttributeElement> mask = BitField<AttributeElement>::all(),
    typename Visitor,
    typename Scalar,
    typename Index>
void par_foreach_named_attribute_write(SurfaceMesh<Scalar, Index>& mesh, Visitor&& vis)
{
    using namespace details;
    internal_foreach_named_attribute<mask, Ordering::Parallel, Access::Write>(
        mesh,
        std::forward<Visitor>(vis));
}

///
/// Applies a function in parallel to each attribute of a mesh. In this variant, the attribute is
/// passed as a writable reference to the visitor function.
///
/// @param[in,out] mesh     Mesh on whose attribute to apply the visitor function.
/// @param[in]     vis      Visitor function to apply to each mesh attribute. Typically a generic
///                         lambda.
///
/// @tparam        mask     Bit field mask to filter attribute based on their element types.
/// @tparam        Visitor  Type of the visitor function, which should be `template<typename T>
///                         void(Attribute<T> &);`
/// @tparam        Scalar   Mesh scalar type.
/// @tparam        Index    Mesh index type.
///
template <
    std::underlying_type_t<AttributeElement> mask = BitField<AttributeElement>::all(),
    typename Visitor,
    typename Scalar,
    typename Index>
void par_foreach_attribute_write(SurfaceMesh<Scalar, Index>& mesh, Visitor&& vis)
{
    using namespace details;
    internal_foreach_attribute<mask, Ordering::Parallel, Access::Write>(
        mesh,
        std::forward<Visitor>(vis));
}

/// @}
/// @}

} // namespace lagrange
