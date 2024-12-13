/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/find_matching_attributes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/map_attribute.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/polyscope.h>
#include <polyscope/surface_mesh.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <CLI/CLI.hpp>

#include <set>

using SurfaceMesh = lagrange::SurfaceMesh32d;

struct MeshFacetAdapter
{
    const SurfaceMesh& mesh;
};

// Convert mesh facet list of variable size to a standard format for Polyscope
template <class S, class I, class T>
std::tuple<std::vector<S>, std::vector<I>> standardizeNestedList(const MeshFacetAdapter& data)
{
    using Index = SurfaceMesh::Index;
    auto& mesh = data.mesh;
    std::vector<S> entries;
    std::vector<I> start;
    entries.reserve(mesh.get_num_corners());
    start.reserve(mesh.get_num_facets() + 1);
    I last_end = 0;
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        auto facet = mesh.get_facet_vertices(f);
        start.push_back(mesh.get_facet_corner_begin(f));
        entries.insert(entries.end(), facet.begin(), facet.end());
        last_end = start.back() + static_cast<I>(facet.size());
    }
    start.push_back(last_end);
    return {entries, start};
}

// Register a Lagrange mesh with Polyscope
void register_mesh(std::string_view mesh_name, SurfaceMesh mesh)
{
    using Usage = lagrange::AttributeUsage;

    // Attribute usages that can be shown as vectors in Polyscope
    std::set<Usage> show_as_vector = {
        Usage::Vector,
        Usage::Normal,
        Usage::Tangent,
        Usage::Bitangent};

    // Convert integral color attributes to floating point
    auto as_color_matrix = [](auto&& attr) -> decltype(auto) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        return matrix_view(attr)
            .template cast<float>()
            // Int -> float conversion
            .unaryExpr([&](float x) { return std::is_floating_point_v<ValueType> ? x : x / 255.f; })
            // Gamma correction
            .unaryExpr([](float x) {
                float gamma = 2.2f;
                return std::pow(x, gamma);
            });
    };

    // Convert any indexed UV attribute into corner attribute
    {
        lagrange::AttributeMatcher matcher;
        matcher.usages = Usage::UV;
        matcher.element_types = lagrange::AttributeElement::Indexed;
        matcher.num_channels = 2;
        for (auto id : lagrange::find_matching_attributes(mesh, matcher)) {
            map_attribute_in_place(mesh, id, lagrange::AttributeElement::Corner);
        }
    }

    // Register mesh connectivity with Polyscope
    auto ps_mesh = [&] {
        if (mesh.is_regular()) {
            // Simple case when facets can be viewed as a matrix
            return polyscope::registerSurfaceMesh(
                std::string(mesh_name),
                vertex_view(mesh),
                facet_view(mesh));
        } else {
            // Hybrid meshes require going through a custom adapter
            return polyscope::registerSurfaceMesh(
                std::string(mesh_name),
                vertex_view(mesh),
                MeshFacetAdapter{mesh});
        }
    }();

    // Register mesh attributes supported by Polyscope
    seq_foreach_named_attribute_read(mesh, [&](auto name_, auto&& attr) {
        if (mesh.attr_name_is_reserved(name_)) {
            return;
        }
        std::string name(name_); // TODO: Fix Polyscope's function signature to accept string_view
        using AttributeType = std::decay_t<decltype(attr)>;
        bool ok = false;
        if constexpr (!AttributeType::IsIndexed) {
            switch (attr.get_element_type()) {
            case lagrange::AttributeElement::Vertex:
                if (attr.get_usage() == Usage::Scalar) {
                    lagrange::logger().info("Registering scalar vertex attribute: {}", name);
                    ps_mesh->addVertexScalarQuantity(name, vector_view(attr));
                    ok = true;
                } else if (attr.get_num_channels() == 3) {
                    if (show_as_vector.count(attr.get_usage())) {
                        lagrange::logger().info("Registering vector vertex attribute: {}", name);
                        ps_mesh->addVertexVectorQuantity(name, matrix_view(attr));
                        ok = true;
                    } else if (attr.get_usage() == Usage::Color) {
                        lagrange::logger().info("Registering color vertex attribute: {}", name);
                        ps_mesh->addVertexColorQuantity(name, as_color_matrix(attr));
                        ok = true;
                    }
                } else if (attr.get_usage() == Usage::UV && attr.get_num_channels() == 2) {
                    lagrange::logger().info("Registering UV vertex attribute: {}", name);
                    ps_mesh->addVertexParameterizationQuantity(name, matrix_view(attr));
                    ok = true;
                }
                // TODO: If usage == Position, maybe add a toggle to switch between various vertex
                // positions
                break;
            case lagrange::AttributeElement::Facet:
                if (attr.get_usage() == Usage::Scalar) {
                    lagrange::logger().info("Registering scalar facet attribute: {}", name);
                    ps_mesh->addFaceScalarQuantity(name, vector_view(attr));
                    ok = true;
                } else if (attr.get_num_channels() == 3) {
                    if (show_as_vector.count(attr.get_usage())) {
                        lagrange::logger().info("Registering vector facet attribute: {}", name);
                        ps_mesh->addFaceVectorQuantity(name, matrix_view(attr));
                        ok = true;
                    } else if (attr.get_usage() == Usage::Color) {
                        lagrange::logger().info("Registering color facet attribute: {}", name);
                        ps_mesh->addFaceColorQuantity(name, as_color_matrix(attr));
                        ok = true;
                    }
                }
                break;
            case lagrange::AttributeElement::Edge:
                if (attr.get_usage() == Usage::Scalar) {
                    lagrange::logger().info("Registering scalar edge attribute: {}", name);
                    ps_mesh->addEdgeScalarQuantity(name, vector_view(attr));
                    ok = true;
                }
                break;
            case lagrange::AttributeElement::Corner:
                if (attr.get_usage() == Usage::UV && attr.get_num_channels() == 2) {
                    lagrange::logger().info("Registering UV corner attribute: {}", name);
                    ps_mesh->addParameterizationQuantity(name, matrix_view(attr));
                    ok = true;
                }
            default: break;
            }
        }
        if (!ok) {
            lagrange::logger().warn("Skipping unsupported attribute: {}", name);
        }
    });
}

int main(int argc, char** argv)
{
    struct
    {
        std::vector<lagrange::fs::path> inputs;
        int log_level = 2; // normal
    } args;

    CLI::App app{argv[0]};
    app.add_option("inputs", args.inputs, "Input mesh(es).")->required()->check(CLI::ExistingFile);
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");
    CLI11_PARSE(app, argc, argv)

    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    polyscope::options::configureImGuiStyleCallback = []() { ImGui::StyleColorsLight(); };
    polyscope::init();

    polyscope::view::setNavigateStyle(polyscope::NavigateStyle::Free);

    for (auto input : args.inputs) {
        lagrange::logger().info("Loading input mesh: {}", input.string());
        auto mesh = lagrange::io::load_mesh<SurfaceMesh>(input);
        register_mesh(input.stem().string(), std::move(mesh));
    }

    polyscope::show();

    return 0;
}
