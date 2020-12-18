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
#include <imgui.h>
#include <lagrange/Logger.h>
#include <lagrange/ui/UI.h>

#include <CLI/CLI.hpp>


const char* obj_cube = R"(
o Cube
v 1.000000 1.000000 -1.000000
v 1.000000 -1.000000 -1.000000
v 1.000000 1.000000 1.000000
v 1.000000 -1.000000 1.000000
v -1.000000 1.000000 -1.000000
v -1.000000 -1.000000 -1.000000
v -1.000000 1.000000 1.000000
v -1.000000 -1.000000 1.000000
vt 0.625000 0.500000
vt 0.875000 0.500000
vt 0.875000 0.750000
vt 0.625000 0.750000
vt 0.375000 0.750000
vt 0.625000 1.000000
vt 0.375000 1.000000
vt 0.375000 0.000000
vt 0.625000 0.000000
vt 0.625000 0.250000
vt 0.375000 0.250000
vt 0.125000 0.500000
vt 0.375000 0.500000
vt 0.125000 0.750000
vn 0.0000 1.0000 0.0000
vn 0.0000 0.0000 1.0000
vn -1.0000 0.0000 0.0000
vn 0.0000 -1.0000 0.0000
vn 1.0000 0.0000 0.0000
vn 0.0000 0.0000 -1.0000
f 1/1/1 5/2/1 7/3/1 3/4/1
f 4/5/2 3/4/2 7/6/2 8/7/2
f 8/8/3 7/9/3 5/10/3 6/11/3
f 6/12/4 2/13/4 4/5/4 8/14/4
f 2/13/5 1/1/5 3/4/5 4/5/5
f 6/11/6 5/10/6 1/1/6 2/13/6
)";

using namespace lagrange::ui;

std::unique_ptr<lagrange::TriangleMesh2Df> create_star(int n = 32, float r0 = 1.0f, float r1 = 0.5f)
{
    n = n % 2 ? n + 1 : n;
    lagrange::Vertices2Df vertices(n + 1, 2);
    lagrange::Triangles triangles((n), 3);
    vertices.row(0) << 0, 0;
    for (auto i = 0; i < n; i++) {
        float angle = (i) / float(n) * lagrange::ui::two_pi();
        float r = (i % 2) ? r0 : r1;
        vertices.row(i + 1) << r * sinf(angle), r * cosf(angle);
    }

    for (auto i = 0; i < n; i++) {
        triangles.row(i) << 0, i + 1, i + 2;
    }
    triangles.row(n - 1) << 0, n, 1;

    return lagrange::create_mesh(vertices, triangles);
}


struct my_mesh_visitor
{
    template <typename MeshType>
    void operator()(MeshType& mesh)
    {
        lagrange::logger().info("\tMeshType = {}", typeid(MeshType).name());
        lagrange::logger().info("\tnum_vertices = {}", mesh.get_num_vertices());
        lagrange::logger().info("\tnum_facets = {}", mesh.get_num_facets());
    }
};

int main(int argc, char** argv)
{
    std::string user_mesh;
    CLI::App app{argv[0]};
    app.add_option("input", user_mesh, "Input mesh.")->check(CLI::ExistingFile);
    CLI11_PARSE(app, argc, argv)


    Viewer::WindowOptions wopt;
    wopt.width = 1920;
    wopt.height = 1080;
    wopt.window_title = "Example Mesh";

    Viewer viewer(wopt);

    if (!viewer.is_initialized()) return 1;
    Scene& scene = viewer.get_scene();


    /*
        Dynamically created mesh (TriangleMesh3D -> MeshModel<TrianglMesh3D>
    */
    int sphere_subdivision = 2;
    auto sphere_model =
        scene.add_model(ModelFactory::make(lagrange::create_sphere(sphere_subdivision), "Sphere"));
    sphere_model->apply_transform(Eigen::Translation3f(0, 0, -1.0f));


    /*
         Dynamically created 2D mesh, displayed in 3D with z = 0
     */
    int star_vertices = 32;
    auto star_model = scene.add_model(ModelFactory::make(create_star(), "Star"));
    star_model->apply_transform(Eigen::AngleAxisf(lagrange::ui::pi() / 2.0f, Eigen::Vector3f(0.0f, 1.0f, 0.0f)) *
                                Eigen::Translation3f(0, 0, -1.0f));


    /*
        3D Quad mesh loaded from .obj
        Note that load_obj returns a vector of models (in case there are more than in the .obj)
    */

    MeshModel<lagrange::QuadMesh3Df> * obj_model = nullptr;
    if (user_mesh.length() > 0) {
        //Load directly from file, including materials
        auto loaded_models =
            ModelFactory::load_obj<lagrange::QuadMesh3Df>(user_mesh);

        if (loaded_models.size() == 0) {
            lagrange::logger().error("Provided .obj doesn't contain any meshes");
            return -1;
        }

        //Add all loaded objects, remember reference to the first one
        obj_model = scene.add_models(std::move(loaded_models)).front();

    } else {

        //Load .obj from stream
        std::stringstream input_stream;
        input_stream << obj_cube;

        //Use lagrange::io module to load mesh from arbitrary input stream
        auto result = lagrange::io::load_mesh_ext<lagrange::QuadMesh3Df>(input_stream);

        //Create a model using the mesh and add it to scene
        obj_model =
            scene.add_model(ModelFactory::make(std::move(result.meshes.front()), ".obj cube"));
    }

    obj_model->apply_transform(Eigen::Translation3f(0,0,1));

    /*
        Creates a visualization "my viz" (toggle it on/off in "Render Passes" dropdown)
        It takes EDGE indices, assigns color using the given lambda and renders LINES
    */
    viewer.add_viz(Viz::create_indexed_colormapping(
        "my viz", Viz::Attribute::EDGE, Viz::Primitive::LINES, [](const Model&, int index) {
            return colormap_turbo((index % 25) / 25.0f);
        }));

    while (!viewer.should_close()) {
        viewer.begin_frame();


        ImGui::Begin("Example Mesh Window");

        /*
            Import new sphere mesh with different subdivision parameters
            into existing model
        */
        ImGui::Text("Subdivide sphere");
        ImGui::PushID(0);
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(30, 0))) {
            sphere_subdivision += 1;
            sphere_model->import_mesh(lagrange::create_sphere(sphere_subdivision));
        }
        ImGui::SameLine();
        if (ImGui::Button("-", ImVec2(30, 0)) && sphere_subdivision > 1) {
            sphere_subdivision -= 1;
            sphere_model->import_mesh(lagrange::create_sphere(sphere_subdivision));
        }
        ImGui::PopID();

        /*
            Import new star mesh with different parameters
            into existing model
        */
        ImGui::Text("Star vertices");
        ImGui::PushID(1);
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(30, 0))) {
            star_vertices += 2;
            star_model->import_mesh(create_star(star_vertices));
        }
        ImGui::SameLine();
        if (ImGui::Button("-", ImVec2(30, 0)) && star_vertices > 4) {
            star_vertices -= 2;
            star_model->import_mesh(create_star(star_vertices));
        }
        ImGui::PopID();

        /*
            Change existing mesh:
                1. First export the mesh
                2. Modify its data
                3. Import it to model again
        */
        ImGui::Text("Twist .obj");
        ImGui::PushID(2);

        float twist_angle = 0.0f;
        if (ImGui::SliderFloat("", &twist_angle, -1.0f, 1.0f)) {
            // Get original bounds & diagonal
            auto bounds = obj_model->get_bounds().transformed(obj_model->get_inverse_transform());
            auto diag = bounds.diagonal();

            // When modifying mesh, export it first
            auto mesh = obj_model->export_mesh();

            // Export vertices to modify them
            lagrange::Vertices3Df V;
            mesh->export_vertices(V);

            // Apply "twist" operation
            for (auto i : lagrange::range(V.rows())) {
                float rotation_angle_y = twist_angle * std::fmod(V(i, 1), diag.y() / 19.0f);
                float x = V(i, 0);
                float z = V(i, 2);
                V(i, 0) = x * cosf(rotation_angle_y) - z * sinf(rotation_angle_y);
                V(i, 2) = x * sinf(rotation_angle_y) + z * cosf(rotation_angle_y);
            }

            // Import vertices to mesh again
            mesh->import_vertices(V);

            // When done with modifications, import the mesh
            obj_model->import_mesh(mesh);
        }

        ImGui::PopID();


        ImGui::PushID(3);
        static int instance_count = 0;
        if (ImGui::Button("Create sphere instance")) {
            auto mesh_ptr_copy = sphere_model->export_mesh();
            sphere_model->import_mesh(mesh_ptr_copy);
            auto instance = scene.add_model(ModelFactory::make(mesh_ptr_copy, "instance"));
            instance->apply_transform(sphere_model->get_transform() * Eigen::Translation3f(
                float(instance_count + 1) * Eigen::Vector3f(2.0f, 0.0f, 0.0f)));

            instance_count++;
        }
        ImGui::PopID();


        /*
           If you need to access the type of the mesh, use the visitor pattern
        */

        // Either through a generic lambda
        if (ImGui::Button("Visit models through generic lambda")) {
            for (auto& model : scene.get_models()) {
                lagrange::logger().info("{}", model->get_name());

                model->visit_mesh([](auto& mesh) {
                    // If you need to get the specific type, use decltype:
                    using MeshType = std::remove_reference_t<decltype(mesh)>;

                    lagrange::logger().info("\tMeshType = {}", typeid(MeshType).name());

                    // Here you have access to all the mesh methods
                    lagrange::logger().info("\tnum_vertices = {}", mesh.get_num_vertices());
                    lagrange::logger().info("\tnum_facets = {}", mesh.get_num_facets());
                });
            }
        }

        // Or through a functor (i.e., a struct with overloaded operator())
        if (ImGui::Button("Visit models through visitor functor")) {
            for (auto& model : scene.get_models()) {
                lagrange::logger().info("{}", model->get_name());
                model->visit_mesh(my_mesh_visitor());
            }
        }


        ImGui::End();

        viewer.end_frame();
    }

    return 0;
}
