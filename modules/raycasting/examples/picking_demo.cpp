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

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/polyscope/register_mesh.h>
#include <lagrange/polyscope/register_point_cloud.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/views.h>

#include <CLI/CLI.hpp>
#include <Eigen/Core>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/polyscope.h>
#include <polyscope/surface_mesh.h>
#include <imgui.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <optional>

using SurfaceMesh = lagrange::SurfaceMesh32f;
using Scalar = SurfaceMesh::Scalar;
using Index = SurfaceMesh::Index;

/// Struct to store information about a picked point on the mesh surface
struct PickedPoint
{
    uint32_t mesh_index = 0;
    uint32_t instance_index = 0;
    uint32_t facet_index = 0;
    Eigen::Vector3f position = Eigen::Vector3f::Zero();
    Eigen::Vector3f normal = Eigen::Vector3f::Zero();
    Eigen::Vector2f barycentric_coord = Eigen::Vector2f::Zero();
    float ray_depth = 0.0f;
};

/// Global state for the demo
struct DemoState
{
    // Mesh and raycaster
    SurfaceMesh mesh;
    lagrange::raycasting::RayCaster ray_caster;
    uint32_t mesh_index = 0;
    std::optional<lagrange::AttributeId> selected_facet;

    // Polyscope objects
    polyscope::SurfaceMesh* ps_mesh = nullptr;
    polyscope::SurfaceMeshQuantity* ps_selected_facet = nullptr;

    // Picking state
    std::optional<PickedPoint> picked_point;

    // UI state
    bool show_normals = true;
    float normal_length = 0.1f;
};

/// Convert screen coordinates to a ray in world space
void screen_to_ray(
    float screen_x,
    float screen_y,
    Eigen::Vector3f& origin,
    Eigen::Vector3f& direction)
{
    glm::vec2 screen_coords{screen_x, screen_y};
    glm::vec3 world_ray = polyscope::view::screenCoordsToWorldRay(screen_coords);
    glm::vec3 world_pos = polyscope::view::getCameraWorldPosition();

    origin = Eigen::Vector3f(world_pos.x, world_pos.y, world_pos.z);
    direction = Eigen::Vector3f(world_ray.x, world_ray.y, world_ray.z);
}

/// Perform ray casting and update picked point
void perform_picking(DemoState& state, float screen_x, float screen_y)
{
    // Get ray from screen coordinates
    Eigen::Vector3f ray_origin, ray_direction;
    screen_to_ray(screen_x, screen_y, ray_origin, ray_direction);

    // Cast the ray
    auto hit = state.ray_caster.cast(ray_origin, ray_direction);

    if (hit) {
        // Store the picked point
        PickedPoint point;
        point.mesh_index = hit->mesh_index;
        point.instance_index = hit->instance_index;
        point.facet_index = hit->facet_index;
        point.position = hit->position;
        point.normal = hit->normal.normalized();
        point.barycentric_coord = hit->barycentric_coord;
        point.ray_depth = hit->ray_depth;

        if (!state.selected_facet.has_value()) {
            state.selected_facet = state.mesh.create_attribute<float>(
                "picked facet",
                lagrange::AttributeElement::Facet,
                lagrange::AttributeUsage::Scalar,
                1);
        } else {
            la_runtime_assert(state.picked_point.has_value());
        }
        auto selected_facets =
            state.mesh.ref_attribute<float>(state.selected_facet.value()).ref_all();
        if (state.picked_point.has_value()) {
            selected_facets[state.picked_point->facet_index] = 0.0f;
        }
        selected_facets[point.facet_index] = 1.0f;

        state.picked_point = point;

        lagrange::logger().info(
            "Picked facet {} at position ({:.3f}, {:.3f}, {:.3f}), distance: {:.3f}",
            point.facet_index,
            point.position.x(),
            point.position.y(),
            point.position.z(),
            point.ray_depth);
    } else {
        lagrange::logger().info("No intersection found");
    }
}

/// Update visualization of picked points
void update_visualization(DemoState& state)
{
    // Visualize current picked point
    if (state.picked_point && state.ps_mesh) {
        // Create a point cloud for the picked location
        std::vector<std::array<float, 3>> points;
        points.push_back(
            {state.picked_point->position.x(),
             state.picked_point->position.y(),
             state.picked_point->position.z()});

        auto* ps_pick = polyscope::registerPointCloud("Picked Point", points);
        ps_pick->setPointColor(glm::vec3{1.0f, 0.0f, 0.0f});

        // Show normal vector
        if (state.show_normals) {
            std::vector<std::array<float, 3>> normals;
            normals.push_back(
                {state.picked_point->normal.x() * state.normal_length,
                 state.picked_point->normal.y() * state.normal_length,
                 state.picked_point->normal.z() * state.normal_length});
            auto* ps_normal =
                ps_pick->addVectorQuantity("Normal", normals, polyscope::VectorType::AMBIENT);
            ps_normal->setEnabled(true);
            ps_normal->setVectorColor(glm::vec3{0.0f, 0.0f, 1.0f});
        }

        // Highlight the picked facet
        auto* ps_facet = lagrange::polyscope::register_attribute(
            *state.ps_mesh,
            "picked facet",
            state.mesh.get_attribute<float>(state.selected_facet.value()));
        ps_facet->setEnabled(true);
        if (auto ps_scalar = dynamic_cast<polyscope::SurfaceScalarQuantity*>(ps_facet)) {
            ps_scalar->setColorMap("blues");
        }
    }
}

/// ImGui callback for custom UI
void draw_ui_panel(DemoState& state)
{
    ImGui::PushItemWidth(150);

    if (ImGui::CollapsingHeader("Picking Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (state.picked_point) {
            ImGui::Text("Facet: %u", state.picked_point->facet_index);
            ImGui::Text(
                "Position: (%.3f, %.3f, %.3f)",
                state.picked_point->position.x(),
                state.picked_point->position.y(),
                state.picked_point->position.z());
            ImGui::Text(
                "Normal: (%.3f, %.3f, %.3f)",
                state.picked_point->normal.x(),
                state.picked_point->normal.y(),
                state.picked_point->normal.z());
            ImGui::Text(
                "Barycentric: (%.3f, %.3f)",
                state.picked_point->barycentric_coord.x(),
                state.picked_point->barycentric_coord.y());
            ImGui::Text("Ray Distance: %.3f", state.picked_point->ray_depth);
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No point picked");
        }
    }

    if (ImGui::CollapsingHeader("Visualization", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Show Normals", &state.show_normals)) {
            update_visualization(state);
        }
        if (state.show_normals) {
            if (ImGui::SliderFloat("Normal Length", &state.normal_length, 0.01f, 1.0f, "%.2f")) {
                update_visualization(state);
            }
        }
    }

    if (ImGui::CollapsingHeader("Instructions", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("- Click on the mesh to pick a point");
        ImGui::TextWrapped("- The red sphere shows the current pick");
        ImGui::TextWrapped("- The blue vector shows the surface normal");
        ImGui::TextWrapped("- The highlighted facet shows the picked triangle");
    }

    ImGui::PopItemWidth();
}

/// Polyscope user callback
void user_callback(DemoState& state)
{
    ImGuiIO& io = ImGui::GetIO();

    // Handle mouse clicks
    if (io.MouseClicked[0] && !io.WantCaptureMouse && !io.WantCaptureKeyboard) {
        perform_picking(state, io.MousePos.x, io.MousePos.y);
        update_visualization(state);
    }

    // Draw custom UI panel
    draw_ui_panel(state);
}

int main(int argc, char** argv)
{
    struct Args
    {
        lagrange::fs::path input;
        int log_level = 2; // info
    } args;

    CLI::App app{argv[0]};
    app.add_option("input", args.input, "Input mesh file (OBJ, STL, PLY, etc.)")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("-l,--log-level", args.log_level, "Log level (0 = trace, 6 = off)");
    CLI11_PARSE(app, argc, argv)

    // Configure logging
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    // Initialize Polyscope
    polyscope::init();
    polyscope::options::configureImGuiStyleCallback = []() { ImGui::StyleColorsLight(); };

    // Load mesh
    lagrange::logger().info("Loading mesh: {}", args.input.string());
    lagrange::io::LoadOptions load_options;
    load_options.triangulate = true;
    auto mesh = lagrange::io::load_mesh<SurfaceMesh>(args.input, load_options);

    lagrange::logger().info(
        "Loaded mesh with {} vertices and {} facets",
        mesh.get_num_vertices(),
        mesh.get_num_facets());

    // Initialize demo state
    DemoState state;
    state.mesh = std::move(mesh);

    // Register mesh with Polyscope
    state.ps_mesh = lagrange::polyscope::register_mesh("Input Mesh", state.mesh);
    state.ps_mesh->setEdgeWidth(1.0);

    // Initialize raycaster
    lagrange::logger().info("Building acceleration structure...");
    state.ray_caster = lagrange::raycasting::RayCaster(
        lagrange::raycasting::SceneFlags::Robust,
        lagrange::raycasting::BuildQuality::High);

    // Add mesh to raycaster (creates a single instance with identity transform)
    state.mesh_index = state.ray_caster.add_mesh(
        SurfaceMesh(state.mesh),
        std::make_optional(Eigen::Affine3f::Identity()));

    // Commit updates to build the BVH
    state.ray_caster.commit_updates();
    lagrange::logger().info("Acceleration structure built");

    // Compute scene extent for normal length
    Eigen::AlignedBox3f bbox;
    for (const auto& p : lagrange::vertex_view(state.mesh).rowwise()) {
        bbox.extend(p.transpose());
    }
    state.normal_length = 0.1f * bbox.diagonal().norm();

    // Set up user callback
    polyscope::state::userCallback = [&]() { user_callback(state); };

    lagrange::logger().info("Starting interactive session");
    lagrange::logger().info("Click on the mesh to pick points!");

    // Start Polyscope UI
    polyscope::show();

    return 0;
}
