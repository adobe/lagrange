/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/io/load_mesh_ply.h>

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/strings.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <happly.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::io {

std::string_view get_suffix(std::string_view name)
{
    size_t pos = name.rfind("_");
    if (pos == std::string_view::npos) {
        return "";
    } else {
        return name.substr(pos);
    }
}

template <typename Scalar, typename Index, typename ValueType, AttributeElement element>
void extract_normal(
    happly::Element& ply_element,
    const std::string_view name,
    SurfaceMesh<Scalar, Index>& mesh)
{
    std::string_view suffix = get_suffix(name);
    auto nx = ply_element.getProperty<ValueType>(fmt::format("nx{}", suffix));
    auto ny = ply_element.getProperty<ValueType>(fmt::format("ny{}", suffix));
    auto nz = ply_element.getProperty<ValueType>(fmt::format("nz{}", suffix));

    Index num_entries = static_cast<Index>(nx.size());
    auto usage = AttributeUsage::Normal;
    std::string attr_name =
        fmt::format("{}_{}{}", internal::to_string(element), internal::to_string(usage), suffix);

    auto id = mesh.template create_attribute<ValueType>(attr_name, element, usage, 3);
    auto attr = mesh.template ref_attribute<ValueType>(id).ref_all();
    la_runtime_assert(static_cast<Index>(attr.size()) == num_entries * 3);
    for (Index i = 0; i < num_entries; ++i) {
        attr[i * 3 + 0] = nx[i];
        attr[i * 3 + 1] = ny[i];
        attr[i * 3 + 2] = nz[i];
    }
}

template <typename Scalar, typename Index, typename ValueType>
void extract_vertex_uv(
    happly::Element& vertex_element,
    const std::string_view name,
    SurfaceMesh<Scalar, Index>& mesh)
{
    std::string_view suffix = get_suffix(name);
    auto u = vertex_element.getProperty<ValueType>(fmt::format("s{}", suffix));
    auto v = vertex_element.getProperty<ValueType>(fmt::format("t{}", suffix));

    Index num_vertices = static_cast<Index>(u.size());
    auto element = AttributeElement::Vertex;
    auto usage = AttributeUsage::UV;
    std::string attr_name =
        fmt::format("{}_{}{}", internal::to_string(element), internal::to_string(usage), suffix);

    auto id = mesh.template create_attribute<ValueType>(attr_name, element, usage, 2);
    auto attr = mesh.template ref_attribute<ValueType>(id).ref_all();
    for (Index i = 0; i < num_vertices; ++i) {
        attr[i * 2 + 0] = u[i];
        attr[i * 2 + 1] = v[i];
    }
}

template <typename Scalar, typename Index, typename ValueType, AttributeElement element>
void extract_color(
    happly::Element& ply_element,
    const std::string_view name,
    SurfaceMesh<Scalar, Index>& mesh)
{
    std::string_view suffix = get_suffix(name);
    auto red = ply_element.getProperty<ValueType>(fmt::format("red{}", suffix));
    auto green = ply_element.getProperty<ValueType>(fmt::format("green{}", suffix));
    auto blue = ply_element.getProperty<ValueType>(fmt::format("blue{}", suffix));
    bool has_alpha = ply_element.hasPropertyType<ValueType>(fmt::format("alpha{}", suffix));

    Index num_entries = static_cast<Index>(red.size());
    auto usage = AttributeUsage::Color;
    std::string attr_name =
        fmt::format("{}_{}{}", internal::to_string(element), internal::to_string(usage), suffix);
    Index num_channels = has_alpha ? 4 : 3;

    auto id = mesh.template create_attribute<ValueType>(attr_name, element, usage, num_channels);
    auto attr = mesh.template ref_attribute<ValueType>(id).ref_all();
    la_debug_assert(static_cast<Index>(attr.size()) == num_entries * num_channels);
    for (Index i = 0; i < num_entries; ++i) {
        attr[i * num_channels + 0] = red[i];
        attr[i * num_channels + 1] = green[i];
        attr[i * num_channels + 2] = blue[i];
    }

    if (has_alpha) {
        auto alpha = ply_element.getProperty<ValueType>(fmt::format("alpha{}", suffix));
        for (Index i = 0; i < num_entries; ++i) {
            attr[i * num_channels + 3] = alpha[i];
        }
    }
}
namespace {

// Happly's hasPropertyType() doesn't support list properties, so this is a workaround.
template <class T>
bool has_list_property_type(happly::Element& ply_element, const std::string& name)
{
    auto& prop = ply_element.getPropertyPtr(name);
    if (!prop) {
        return false;
    }
    happly::TypedListProperty<T>* casted_prop =
        dynamic_cast<happly::TypedListProperty<T>*>(prop.get());
    if (casted_prop) {
        return true;
    }
    return false;
}

} // namespace

template <AttributeElement element, typename Scalar, typename Index>
void extract_property(
    happly::Element& ply_element,
    const std::string& name,
    SurfaceMesh<Scalar, Index>& mesh)
{
    auto process_property = [&](auto&& data) {
        if (data.empty()) return;
        using ValueType = typename std::decay_t<decltype(data)>::value_type;
        mesh.template create_attribute<ValueType>(
            name,
            element,
            AttributeUsage::Scalar,
            1,
            {data.data(), data.size()});
    };

    const Index expected_num_elements = [&] {
        switch (element) {
        case AttributeElement::Vertex: return mesh.get_num_vertices();
        case AttributeElement::Facet: return mesh.get_num_facets();
        default: return Index(0);
        }
    }();

    auto process_list_property = [&](auto&& data) {
        if (data.empty()) return;
        using ValueType = typename std::decay_t<decltype(data[0])>::value_type;
        la_runtime_assert(static_cast<Index>(data.size()) == expected_num_elements);
        Index num_channels = static_cast<Index>(data[0].size());
        auto id = mesh.template create_attribute<ValueType>(
            name,
            element,
            AttributeUsage::Vector,
            num_channels);
        auto attr = mesh.template ref_attribute<ValueType>(id).ref_all();
        la_runtime_assert(data.size() * num_channels == attr.size());

        const Index num_entries = static_cast<Index>(data.size());
        for (Index i = 0; i < num_entries; i++) {
            la_runtime_assert(static_cast<Index>(data[i].size()) == num_channels);
            for (Index j = 0; j < num_channels; j++) {
                attr[i * num_channels + j] = data[i][j];
            }
        }
    };

    // Try interpret property as single channel property.
#define LA_X_try_ValueType(_, T)                         \
    if (ply_element.template hasPropertyType<T>(name)) { \
        auto data = ply_element.getProperty<T>(name);    \
        process_property(data);                          \
        return;                                          \
    }
    LA_ATTRIBUTE_X(try_ValueType, 0)
#undef LA_X_try_ValueType

    // Try interpret property as multi-channel list property.
#define LA_X_try_ValueType(_, T)                          \
    if (has_list_property_type<T>(ply_element, name)) {   \
        auto data = ply_element.getListProperty<T>(name); \
        process_list_property(data);                      \
        return;                                           \
    }
    LA_ATTRIBUTE_X(try_ValueType, 0)
#undef LA_X_try_ValueType
}

template <typename Scalar, typename Index>
void extract_vertex_properties(
    happly::Element& vertex_element,
    SurfaceMesh<Scalar, Index>& mesh,
    const LoadOptions& options)
{
    for (const auto& name : vertex_element.getPropertyNames()) {
        if (options.load_normals && (name == "nx" || starts_with(name, "nx_"))) {
#define LA_X_try_ValueType(_, T)                          \
    if (vertex_element.template hasPropertyType<T>(name)) \
        extract_normal<Scalar, Index, T, AttributeElement::Vertex>(vertex_element, name, mesh);
            LA_ATTRIBUTE_X(try_ValueType, 0)
#undef LA_X_try_ValueType
        } else if (options.load_vertex_colors && (name == "red" || starts_with(name, "red_"))) {
#define LA_X_try_ValueType(_, T)                          \
    if (vertex_element.template hasPropertyType<T>(name)) \
        extract_color<Scalar, Index, T, AttributeElement::Vertex>(vertex_element, name, mesh);
            LA_ATTRIBUTE_X(try_ValueType, 0)
#undef LA_X_try_ValueType
        } else if (options.load_uvs && (name == "s" || starts_with(name, "s_"))) {
#define LA_X_try_ValueType(_, T)                          \
    if (vertex_element.template hasPropertyType<T>(name)) \
        extract_vertex_uv<Scalar, Index, T>(vertex_element, name, mesh);
            LA_ATTRIBUTE_X(try_ValueType, 0)
#undef LA_X_try_ValueType
        } else {
            // Skip other known channels.
            if (name == "x") continue;
            if (name == "y") continue;
            if (name == "z") continue;
            if (name == "ny" || starts_with(name, "ny_")) continue;
            if (name == "nz" || starts_with(name, "nz_")) continue;
            if (name == "green" || starts_with(name, "green_")) continue;
            if (name == "blue" || starts_with(name, "blue_")) continue;
            if (name == "alpha" || starts_with(name, "alpha_")) continue;
            if (name == "t" || starts_with(name, "t_")) continue;
            extract_property<AttributeElement::Vertex>(vertex_element, name, mesh);
        }
    }
}

template <typename Scalar, typename Index>
void extract_facet_properties(
    happly::Element& facet_element,
    SurfaceMesh<Scalar, Index>& mesh,
    const LoadOptions& options)
{
    for (const auto& name : facet_element.getPropertyNames()) {
        if (options.load_normals && (name == "nx" || starts_with(name, "nx_"))) {
#define LA_X_try_ValueType(_, T)                         \
    if (facet_element.template hasPropertyType<T>(name)) \
        extract_normal<Scalar, Index, T, AttributeElement::Facet>(facet_element, name, mesh);
            LA_ATTRIBUTE_X(try_ValueType, 0)
#undef LA_X_try_ValueType
        } else if (name == "red" || starts_with(name, "red_")) {
#define LA_X_try_ValueType(_, T)                         \
    if (facet_element.template hasPropertyType<T>(name)) \
        extract_color<Scalar, Index, T, AttributeElement::Facet>(facet_element, name, mesh);
            LA_ATTRIBUTE_X(try_ValueType, 0)
#undef LA_X_try_ValueType
        } else {
            // Skip other known channels.
            if (name == "ny" || starts_with(name, "ny_")) continue;
            if (name == "nz" || starts_with(name, "nz_")) continue;
            if (name == "green" || starts_with(name, "green_")) continue;
            if (name == "blue" || starts_with(name, "blue_")) continue;
            if (name == "vertex_indices" || starts_with(name, "vertex_indices_")) continue;
            if (name == "vertex_index" || starts_with(name, "vertex_index_")) continue;
            extract_property<AttributeElement::Facet>(facet_element, name, mesh);
        }
    }
}

template <typename MeshType>
MeshType load_mesh_ply(std::istream& input_stream, const LoadOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    happly::PLYData ply(input_stream);
    ply.validate();

    MeshType mesh;

    happly::Element& vertex_element = ply.getElement("vertex");
    const Index num_vertices = Index(vertex_element.count);
    {
        const std::vector<Scalar>& xPos = vertex_element.getProperty<Scalar>("x");
        const std::vector<Scalar>& yPos = vertex_element.getProperty<Scalar>("y");
        const std::vector<Scalar>& zPos = vertex_element.getProperty<Scalar>("z");
        mesh.add_vertices(num_vertices, [&](Index v, span<Scalar> p) -> void {
            p[0] = xPos[v];
            p[1] = yPos[v];
            p[2] = zPos[v];
        });
    }

    const std::vector<std::vector<Index>>& facets = ply.getFaceIndices<Index>();
    mesh.add_hybrid(
        Index(facets.size()),
        [&](Index f) -> Index { return Index(facets[f].size()); },
        [&](Index f, span<Index> t) -> void {
            const auto& face = facets[f];
            for (size_t i = 0; i < face.size(); ++i) {
                t[i] = Index(face[i]);
            }
        });

    extract_vertex_properties(vertex_element, mesh, options);
    extract_facet_properties(ply.getElement("face"), mesh, options);
    return mesh;
}

template <typename MeshType>
MeshType load_mesh_ply(const fs::path& filename, const LoadOptions& options)
{
    fs::ifstream fin(filename, std::ios::binary);
    la_runtime_assert(fin.good(), fmt::format("Unable to open file {}", filename.string()));
    return load_mesh_ply<MeshType>(fin, options);
}

#define LA_X_load_mesh_ply(_, S, I)                                                      \
    template SurfaceMesh<S, I> load_mesh_ply(std::istream&, const LoadOptions& options); \
    template SurfaceMesh<S, I> load_mesh_ply(const fs::path& filename, const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh_ply, 0)

} // namespace lagrange::io
