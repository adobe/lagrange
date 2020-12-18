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
#include <lagrange/Logger.h>
#include <lagrange/compute_dijkstra_distance.h>
#include <lagrange/ui/UI.h>


using namespace lagrange::ui;

template <typename Matrix>
void normalize_matrix(Matrix& m)
{
    auto min_val = m.colwise().minCoeff().eval();
    auto max_val = m.colwise().maxCoeff().eval();
    auto mult = (max_val - min_val).cwiseInverse().eval();
    for (auto i = 0; i < m.rows(); i++) {
        m.row(i) = (m.row(i) - min_val).cwiseProduct(mult);
    }
}


struct initialize_attributes_visitor {
    template <typename MeshModelType>
    void operator()(MeshModelType& mesh_model)
    {
        auto mesh_ptr = mesh_model.export_mesh();
        auto& mesh = *mesh_ptr;
        using MeshType = std::remove_reference_t<decltype(mesh)>;
        using AttributeArray = typename MeshType::AttributeArray;

        if (!mesh.is_edge_data_initialized()) {
            try {
                mesh.initialize_edge_data();
            }
            catch (std::exception& ex) {
                lagrange::logger().info(
                    "Failed to initialize edge data, some edge vizualizations may not work: {}",
                    ex.what());
            }
        }

        // Create random random_vertex_attribute
        if (!mesh.has_vertex_attribute("random_vertex_attribute"))
            mesh.add_vertex_attribute("random_vertex_attribute");
        AttributeArray random_vertex_attribute = AttributeArray::Random(mesh.get_num_vertices(), 3);
        mesh.import_vertex_attribute("random_vertex_attribute", random_vertex_attribute);

        try {
            // Initializes "dijkstra_distance" vertex_attribute
            lagrange::compute_dijkstra_distance(
                mesh, 0, Eigen::Matrix<typename MeshType::Scalar, 3, 1>(1, 1, 1));

            // Normalize the values
            AttributeArray d;
            mesh.export_vertex_attribute("dijkstra_distance", d);
            normalize_matrix(d);
            mesh.import_vertex_attribute("dijkstra_distance", d);
        }
        catch (std::exception& ex) {
            lagrange::logger().info("Failed to initialize dijkstra distance: {}", ex.what());
        }

        mesh_model.import_mesh(mesh_ptr);
    }
};
