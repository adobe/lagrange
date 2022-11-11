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
#include <lagrange/AttributeTypes.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/utils/copy_on_write_ptr.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <map>
#include <sstream>
#include <unordered_map>

namespace {

template <typename ValueType>
std::string_view type_name();

#define LA_X_type_name(_, ValueType)        \
    template <>                             \
    std::string_view type_name<ValueType>() \
    {                                       \
        return #ValueType;                  \
    }
LA_ATTRIBUTE_X(type_name, 0)

template <typename ValueType>
std::string_view attr_type_name(const lagrange::Attribute<ValueType>&)
{
    return type_name<ValueType>();
}

bool starts_with(std::string_view str, std::string_view prefix)
{
    return (str.rfind(prefix, 0) == 0);
}

std::vector<std::string> string_split(const std::string& s, char delimiter)
{
    std::istringstream iss(s);
    std::vector<std::string> words;
    std::string word;
    while (std::getline(iss, word, delimiter)) {
        words.push_back(word);
    }
    return words;
}

template <typename Scalar, typename Index_>
void test_foreach_attribute()
{
    using namespace lagrange;
    SurfaceMesh<Scalar, Index_> mesh;
    mesh.add_vertices(10);
    mesh.add_triangles(5);
    mesh.add_quads(6);

    auto attribute_elements = {
        AttributeElement::Vertex,
        AttributeElement::Facet,
        AttributeElement::Corner,
        AttributeElement::Value,
        AttributeElement::Indexed,
    };

#define LA_NON_INDEXED_X(mode, data)                                                       \
    LA_X_##mode(data, AttributeElement::Vertex) LA_X_##mode(data, AttributeElement::Facet) \
        LA_X_##mode(data, AttributeElement::Corner) LA_X_##mode(data, AttributeElement::Value)

    std::atomic_int cnt = 0;
    for (auto elem : attribute_elements) {
        std::string name = fmt::format("attr_{}", cnt++);
        mesh.template create_attribute<double>(name, elem);
        for (size_t i = 0; i < 50; ++i) {
            mesh.duplicate_attribute(name, fmt::format("attr_{}", cnt++));
        }
    }

    // Basic attribute iteration
    seq_foreach_attribute_read(mesh, [](auto&& attr) {
        // One can retrieve the scalar type within the lambda
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        logger().info("Attribute scalar size: {}", sizeof(ValueType));
    });

    // Filtering attribute types at compile time will limit the generic lambda instantiations
    seq_foreach_named_attribute_read<~AttributeElement::Indexed>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            logger().info("Attribute named '{}' with {} elements", name, attr.get_num_elements());
        });

    // Read Seq
    {
        bool has_vertices = false;
        bool has_facets = false;
        bool has_offsets = false;
        seq_foreach_named_attribute_read<~AttributeElement::Indexed>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                REQUIRE(mesh.has_attribute(name));
                lagrange::logger().info(
                    "Mesh attribute '{}' of type {}",
                    name,
                    attr_type_name(attr));
                has_vertices |= (name == mesh.attr_name_vertex_to_position());
                has_facets |= (name == mesh.attr_name_corner_to_vertex());
                has_offsets |= (name == mesh.attr_name_facet_to_first_corner());
            });
        REQUIRE(has_vertices);
        REQUIRE(has_facets);
        REQUIRE(has_offsets);

        seq_foreach_attribute_read<AttributeElement::Indexed>(mesh, [](auto&& attr) {
            logger().info("Attribute with {} channels", attr.get_num_channels());
        });
        seq_foreach_attribute_read<AttributeElement::Indexed>(mesh, [](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            using Index = typename AttributeType::Index;
            lagrange::logger().info(
                "Mesh indexed attribute of type {}, value type size {}, index size {}",
                attr_type_name(attr.values()),
                sizeof(ValueType),
                sizeof(Index));
        });

#define LA_X_attr(_, Element)                                                                  \
    seq_foreach_named_attribute_read<Element>(mesh, [&](std::string_view name, auto&& attr) {  \
        REQUIRE(attr.get_element_type() == Element);                                           \
        REQUIRE(mesh.has_attribute(name));                                                     \
        lagrange::logger().info("Mesh attribute '{}' of type {}", name, attr_type_name(attr)); \
    });                                                                                        \
    seq_foreach_attribute_read<Element>(mesh, [&](auto&& attr) {                               \
        REQUIRE(attr.get_element_type() == Element);                                           \
        lagrange::logger().info("Mesh attribute of type {}", attr_type_name(attr));            \
    });
        LA_NON_INDEXED_X(attr, 0)
#undef LA_X_attr
    }

    // Write Seq
    {
        seq_foreach_named_attribute_write<~AttributeElement::Indexed>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                REQUIRE(mesh.has_attribute(name));
                using AttributeType = std::decay_t<decltype(attr)>;
                using ValueType = typename AttributeType::ValueType;
                std::fill(attr.ref_all().begin(), attr.ref_all().end(), ValueType(1));
            });

        seq_foreach_attribute_write(mesh, [](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if constexpr (AttributeType::IsIndexed) {
                attr.values().resize_elements(10);
                std::fill(
                    attr.values().ref_all().begin(),
                    attr.values().ref_all().end(),
                    ValueType(1));
            } else {
                std::fill(attr.ref_all().begin(), attr.ref_all().end(), ValueType(1));
            }
        });

        // Use compile-time if to check for indexed attributes
        seq_foreach_named_attribute_read(mesh, [](std::string_view name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            if constexpr (AttributeType::IsIndexed) {
                logger().info(
                    "Indexed attribute '{}' has {} values",
                    name,
                    attr.values().get_num_elements());
            } else {
                logger().info("Attribute '{}' has {} elements", name, attr.get_num_elements());
            }
        });


#define LA_X_attr(_, Element)                                                                  \
    seq_foreach_named_attribute_write<Element>(mesh, [&](std::string_view name, auto&& attr) { \
        REQUIRE(attr.get_element_type() == Element);                                           \
        REQUIRE(mesh.has_attribute(name));                                                     \
        using AttributeType = std::decay_t<decltype(attr)>;                                    \
        using ValueType = typename AttributeType::ValueType;                                   \
        std::fill(attr.ref_all().begin(), attr.ref_all().end(), ValueType(1));                 \
    });                                                                                        \
    seq_foreach_attribute_write<Element>(mesh, [&](auto&& attr) {                              \
        REQUIRE(attr.get_element_type() == Element);                                           \
        using AttributeType = std::decay_t<decltype(attr)>;                                    \
        using ValueType = typename AttributeType::ValueType;                                   \
        std::fill(attr.ref_all().begin(), attr.ref_all().end(), ValueType(1));                 \
    });
        LA_NON_INDEXED_X(attr, 0)
#undef LA_X_attr
    }

    // Read Par
    {
        std::atomic_int has_vertices = 0;
        std::atomic_int has_facets = 0;
        std::atomic_int has_offsets = 0;
        std::atomic_int ok = 1;
        par_foreach_named_attribute_read<~AttributeElement::Indexed>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                ok &= mesh.has_attribute(name);
                lagrange::logger().info(
                    "Mesh attribute '{}' of type {}",
                    name,
                    attr_type_name(attr));
                has_vertices |= (name == mesh.attr_name_vertex_to_position());
                has_facets |= (name == mesh.attr_name_corner_to_vertex());
                has_offsets |= (name == mesh.attr_name_facet_to_first_corner());
            });
        REQUIRE(ok);
        REQUIRE(has_vertices);
        REQUIRE(has_facets);
        REQUIRE(has_offsets);

        par_foreach_attribute_read<~AttributeElement::Indexed>(mesh, [](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            lagrange::logger().info(
                "Mesh attribute of type {}, size {}",
                attr_type_name(attr),
                sizeof(ValueType));
        });

#define LA_X_attr(_, Element)                                                                  \
    par_foreach_named_attribute_read<Element>(mesh, [&](std::string_view name, auto&& attr) {  \
        ok &= (attr.get_element_type() == Element);                                            \
        ok &= mesh.has_attribute(name);                                                        \
        lagrange::logger().info("Mesh attribute '{}' of type {}", name, attr_type_name(attr)); \
    });                                                                                        \
    par_foreach_attribute_read<Element>(mesh, [&](auto&& attr) {                               \
        ok &= (attr.get_element_type() == Element);                                            \
        lagrange::logger().info("Mesh attribute of type {}", attr_type_name(attr));            \
    });
        LA_NON_INDEXED_X(attr, 0)
#undef LA_X_attr
        REQUIRE(ok);
    }

    // Write Par
    {
        std::atomic_int ok = 1;
        par_foreach_named_attribute_write<~AttributeElement::Indexed>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                ok &= mesh.has_attribute(name);
                using AttributeType = std::decay_t<decltype(attr)>;
                using ValueType = typename AttributeType::ValueType;
                std::fill(attr.ref_all().begin(), attr.ref_all().end(), ValueType(1));
            });
        REQUIRE(ok);

        par_foreach_attribute_write<~AttributeElement::Indexed>(mesh, [](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            std::fill(attr.ref_all().begin(), attr.ref_all().end(), ValueType(1));
        });

#define LA_X_attr(_, Element)                                                                  \
    par_foreach_named_attribute_write<Element>(mesh, [&](std::string_view name, auto&& attr) { \
        ok &= (attr.get_element_type() == Element);                                            \
        ok &= mesh.has_attribute(name);                                                        \
        using AttributeType = std::decay_t<decltype(attr)>;                                    \
        using ValueType = typename AttributeType::ValueType;                                   \
        std::fill(attr.ref_all().begin(), attr.ref_all().end(), ValueType(1));                 \
    });                                                                                        \
    par_foreach_attribute_write<Element>(mesh, [&](auto&& attr) {                              \
        ok &= (attr.get_element_type() == Element);                                            \
        using AttributeType = std::decay_t<decltype(attr)>;                                    \
        using ValueType = typename AttributeType::ValueType;                                   \
        std::fill(attr.ref_all().begin(), attr.ref_all().end(), ValueType(1));                 \
    });
        LA_NON_INDEXED_X(attr, 0)
#undef LA_X_attr
        REQUIRE(ok);
    }
}

template <typename Scalar, typename Index_>
void test_foreach_cow()
{
    using namespace lagrange;

    // Sequential Write
    {
        SurfaceMesh<Scalar, Index_> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(5);
        mesh.add_quads(6);

        int N = 30;

        for (int i = 0; i < N; ++i) {
            mesh.template create_attribute<double>(
                fmt::format("attr_{}_1", i),
                AttributeElement::Vertex);
            mesh.duplicate_attribute(fmt::format("attr_{}_1", i), fmt::format("attr_{}_2", i));
        }

        std::map<std::string, std::pair<const void*, const void*>> ptr;
        seq_foreach_named_attribute_read<~AttributeElement::Indexed>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                ptr[std::string(name)].first = attr.get_all().data();
            });
        seq_foreach_attribute_write<~AttributeElement::Indexed>(mesh, [](auto&& attr) {
            attr.ref_all()[0] = 1;
        });
        seq_foreach_named_attribute_read<~AttributeElement::Indexed>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                ptr[std::string(name)].second = attr.get_all().data();
            });
        for (auto kv : ptr) {
            std::string_view name = kv.first;
            const void* before1 = kv.second.first;
            const void* after1 = kv.second.second;
            if (starts_with(name, "attr_")) {
                auto tokens = string_split(std::string(name), '_');
                std::string other =
                    fmt::format("{}_{}_{}", tokens[0], tokens[1], tokens.back() == "1" ? "2" : "1");
                const void* before2 = ptr.at(other).first;
                const void* after2 = ptr.at(other).second;
                CAPTURE(name, before1, after1, before2, after2);
                REQUIRE(before1 == before2);
                REQUIRE(after1 != after2);
                REQUIRE((before1 == after1 || before2 == after2));
            } else {
                REQUIRE(before1 == after1);
            }
        }
    }

    // Parallel Write
    {
        SurfaceMesh<Scalar, Index_> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(5);
        mesh.add_quads(6);

        int N = 30;

        for (int i = 0; i < N; ++i) {
            mesh.template create_attribute<double>(
                fmt::format("attr_{}_1", i),
                AttributeElement::Vertex);
            mesh.duplicate_attribute(fmt::format("attr_{}_1", i), fmt::format("attr_{}_2", i));
        }

        std::map<std::string, std::pair<const void*, const void*>> ptr;
        seq_foreach_named_attribute_read<~AttributeElement::Indexed>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                ptr[std::string(name)].first = attr.get_all().data();
            });
        par_foreach_attribute_write<~AttributeElement::Indexed>(mesh, [](auto&& attr) {
            attr.ref_all()[0] = 1;
        });
        seq_foreach_named_attribute_read<~AttributeElement::Indexed>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                ptr[std::string(name)].second = attr.get_all().data();
            });
        for (auto kv : ptr) {
            std::string_view name = kv.first;
            const void* before1 = kv.second.first;
            const void* after1 = kv.second.second;
            if (starts_with(name, "attr_")) {
                auto tokens = string_split(std::string(name), '_');
                std::string other =
                    fmt::format("{}_{}_{}", tokens[0], tokens[1], tokens.back() == "1" ? "2" : "1");
                const void* before2 = ptr.at(other).first;
                const void* after2 = ptr.at(other).second;
                CAPTURE(name, before1, after1, before2, after2);
                REQUIRE(before1 == before2);
                REQUIRE(after1 != after2);
                // We do not require that (before1 == after1 || before2 == after2). Indeed, a
                // conservative copy may happen both attribute in case of a concurrent write. Since
                // we do not block write operations with a mutex (which is expensive), it is
                // preferable to always copy instead.
            } else {
                REQUIRE(before1 == after1);
            }
        }
    }

    // Adding Attributes
    {
        SurfaceMesh<Scalar, Index_> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(5);
        mesh.add_quads(6);

        int N = 30;

        std::map<std::string, std::pair<const void*, const void*>> ptr_data;
        std::map<std::string, std::pair<const void*, const void*>> ptr_attr;
        seq_foreach_named_attribute_read<~AttributeElement::Indexed>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                ptr_data[std::string(name)].first = attr.get_all().data();
                ptr_attr[std::string(name)].first = &attr;
            });
        for (int i = 0; i < N; ++i) {
            mesh.template create_attribute<double>(
                fmt::format("attr_{}_1", i),
                AttributeElement::Vertex);
            mesh.duplicate_attribute(fmt::format("attr_{}_1", i), fmt::format("attr_{}_2", i));
        }
        seq_foreach_named_attribute_read<~AttributeElement::Indexed>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                if (ptr_data.count(std::string(name))) {
                    ptr_data[std::string(name)].second = attr.get_all().data();
                    ptr_attr[std::string(name)].second = &attr;
                }
            });
        for (auto kv : ptr_data) {
            const void* before1 = kv.second.first;
            const void* after1 = kv.second.second;
            REQUIRE(before1 == after1);
        }
        for (auto kv : ptr_attr) {
            const void* before1 = kv.second.first;
            const void* after1 = kv.second.second;
            REQUIRE(before1 == after1);
        }
    }
}

struct ArrayBase
{
    virtual ~ArrayBase() = default;
};

template <typename Scalar>
struct Array : public ArrayBase
{
    std::vector<Scalar> data;
};

template <typename ValueType>
void test_parallel_cow()
{
    using namespace lagrange;

    int N = 100;

    std::vector<copy_on_write_ptr<ArrayBase>> attrs;
    for (int i = 0; i < N; ++i) {
        auto ptr = internal::make_shared<Array<ValueType>>();
        ptr->data.resize(10);
        attrs.emplace_back(std::move(ptr));
        attrs.push_back(attrs.back());
    }
    tbb::parallel_for(0, N, [&](int i) { attrs[i].static_write<Array<ValueType>>()->data[0] = 1; });
}

} // namespace

TEST_CASE("SurfaceMesh: Foreach Attributes", "[next]")
{
#define LA_X_test_foreach_attribute(_, Scalar, Index) test_foreach_attribute<Scalar, Index>();
    LA_SURFACE_MESH_X(test_foreach_attribute, 0)
}

TEST_CASE("SurfaceMesh: Foreach CoW", "[next]")
{
#define LA_X_test_foreach_cow(_, Scalar, Index) test_foreach_cow<Scalar, Index>();
    LA_SURFACE_MESH_X(test_foreach_cow, 0)
}

TEST_CASE("Simple Parallel CoW", "[next]")
{
    test_parallel_cow<double>();
}
