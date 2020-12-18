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
#include <lagrange/ui/UI.h>

using namespace lagrange::ui;


int main()
{
    /*
        Set up initial window and OpenGL context options
    */

    Viewer::WindowOptions wopt;
    wopt.width = 1920;
    wopt.height = 1080;
    wopt.window_title = "Example Basic";
    wopt.vsync = false;


    /*
        Initialize the viewer
    */
    Viewer viewer(wopt);

    /*
        Check if everything initialized
    */
    if (!viewer.is_initialized()) return 1;

    /*
        Set up your scene with Models and Emitters.
        Models represent geometry.
        MeshModel<MeshType> represents geometry made of lagrange::MeshType.
        Use ModelFactory to create or load models

    */
    Scene& scene = viewer.get_scene();

    /*
        This creates a sphere MeshModel that contains the sphere mesh.
        The type depends on the input MeshType
    */
    auto sphere_model = ModelFactory::make(lagrange::create_sphere(4));


    /*
        Add model to the scene, this makes it available for rendering and interaction
        You can save the pointer for later interaction with the model
    */
    auto sphere_model_ptr = scene.add_model(std::move(sphere_model));

    /*
        Modify position
    */
    sphere_model_ptr->set_transform(Eigen::Translation3f(0, 0.5f * sphere_model_ptr->get_bounds().diagonal().y(),0));

    /*
        Modify material
    */
    {
        auto& material = sphere_model_ptr->get_material();
        material["baseColor"].value = Color(0.3f, 0.4f, 0.3f);
    }

    /*
        Emitters
    */

    // By default, an IBL (Image Based Lighting) environmental map is loaded and provides light
    // To disable it, use wopt.default_ibl = "";
    // To add your own, use make_unique<IBL>(image_path)

    // Creates a point light at position with intensity
    scene.add_emitter(
        std::make_unique<PointLight>(Eigen::Vector3f(0, 3, 3), 20.0f * Eigen::Vector3f(0.8f, 0.5f, 0.2f)));


    // Creates a directional light  with direction and intensity
    scene.add_emitter(
        std::make_unique<DirectionalLight>(Eigen::Vector3f(0, -3, 3), Eigen::Vector3f(0.2f, 0.5f, 0.8f)));

    /*
        Enables infinite ground plane with grid and axes at y = -1
    */
    viewer.enable_ground(true);
    viewer.get_ground().enable_grid(true).enable_axes(true).set_height(-1.0f);

    /*
        Creates a visualization "my viz" (toggle it in "Render Passes" dropdown)
        It takes VERTEX indices, assigns color using the given lambda and renders LINES
    */
    viewer.add_viz(Viz::create_indexed_colormapping(
        "my viz", Viz::Attribute::VERTEX, Viz::Primitive::LINES, [](const Model& model, int index) {
            auto mesh = model.mesh<lagrange::TriangleMesh3D>();
            if (!mesh) return Color::empty();

            auto pos = mesh->get_vertices().row(index).transpose().cast<float>().eval();
            auto bounds = model.get_bounds();
            auto diag = bounds.diagonal();

            return Color((pos - bounds.min()).cwiseProduct(diag.cwiseInverse()), 1.0f);
        }));

    /*
        Runs the main loop
        If you don't need contorl over the main loop,
        you can also use viewer.run();
    */
    while (!viewer.should_close()) {
        viewer.begin_frame();

        ImGui::Begin("Test");
        ImGui::Text("Hello world");
        ImGui::End();

        viewer.end_frame();
    }

    return 0;
}
