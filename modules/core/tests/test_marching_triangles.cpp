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

#include <string>
#include <vector>

#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>

#include <lagrange/create_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/marching_triangles.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/warning.h>

namespace {

using namespace lagrange;

// For debugging, write the extracted edges in vtk format
// We can later move it to lagrange core...
void save_edge_network_vtk(
    const std::string fname,
    const std::vector<Eigen::MatrixXf>& vertices,
    const std::vector<Eigen::MatrixXi>& edges,
    const std::vector<Eigen::VectorXf>& edge_attributes)
{
    FILE* fl = fopen(fname.c_str(), "wb");

    if (!fl) {
        printf("File %s was not found \n", fname.c_str());
        return;
    }

    // write the header
    fprintf(fl, "# vtk DataFile Version 2.0\n");
    fprintf(fl, "Shayan's output VTK line file \n");
    fprintf(fl, "ASCII\n");
    fprintf(fl, "DATASET POLYDATA\n");
    fprintf(fl, "\n");


    // write the vertices
    std::vector<int> sum_num_vertices = {0};
    for (auto i : range(vertices.size())) {
        sum_num_vertices.emplace_back(sum_num_vertices.back() + (int)vertices[i].rows());
    }
    fprintf(fl, "POINTS %d float\n", sum_num_vertices.back());
    for (auto i : range(vertices.size())) {
        for (auto j : range(vertices[i].rows())) {
            float x = vertices[i].cols() > 0 ? vertices[i](j, 0) : 0;
            float y = vertices[i].cols() > 1 ? vertices[i](j, 1) : 0;
            float z = vertices[i].cols() > 2 ? vertices[i](j, 2) : 0;
            fprintf(fl, "%.12f %.12f %.12f \n", x, y, z);
        }
    }
    fprintf(fl, "\n");

    //
    // write the lines
    //
    int n_total_lines = 0;
    for (auto i : range(edges.size())) {
        n_total_lines += (int)edges[i].rows();
    }
    fprintf(fl, "LINES %d %d \n", n_total_lines, n_total_lines * 3);
    for (auto i : range(edges.size())) {
        la_runtime_assert(edges[i].cols() == 2);
        for (auto j : range(edges[i].rows())) {
            fprintf(
                fl,
                "%d %d %d \n",
                2,
                (int)edges[i](j, 0) + sum_num_vertices[i],
                (int)edges[i](j, 1) + sum_num_vertices[i]);
        }
    }
    fprintf(fl, "\n");

    if (edge_attributes.size() > 0) {
        fprintf(fl, "CELL_DATA %d \n", n_total_lines);
        fprintf(fl, "SCALARS attrib0 float 1 \n");
        fprintf(fl, "LOOKUP_TABLE default \n");
        for (auto i : range(edge_attributes.size())) {
            la_runtime_assert(edge_attributes[i].rows() == edges[i].rows());
            for (const auto j : range(edge_attributes[i].size())) {
                fprintf(fl, "%f \n", (float)edge_attributes[i](j));
            }
            fprintf(fl, "\n");
        }
    }

    fclose(fl);
}

using Scalar = float;
using Index = uint32_t;
using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor>;
using MeshType = Mesh<VertexArray, FacetArray>;
using AttributeArray = MeshType::AttributeArray;
using MarchingTrianglesOutput = ::lagrange::MarchingTrianglesOutput<MeshType>;

// create a square mesh ([0 1] x [0 1]).
// n and m are the number of vertices in x and y
// num_dims: if 2 creates a 2D mesh. Otherwise a 3-D one.
// delta is for perturbating the mesh slightly
std::unique_ptr<MeshType>
create_square(const int n, const int m, const Index num_dims, const Scalar delta = 0)
{
    la_runtime_assert(num_dims == 2 || num_dims == 3);
    const Index num_vertices = n * m;
    const Index num_facets = (n - 1) * (m - 1) * 2;
    VertexArray vertices(num_vertices, num_dims);
    FacetArray facets(num_facets, 3);
    for (auto i : range(n)) {
        for (auto j : range(m)) {
            const Scalar iscaled = Scalar(i) / (n - 1);
            const Scalar jscaled = Scalar(j) / (m - 1);
LA_IGNORE_ARRAY_BOUNDS_BEGIN
            Eigen::Matrix<Scalar, 1, 3> pt(iscaled, jscaled, 0);
            vertices.row(j * n + i) = pt.head(num_dims);
LA_IGNORE_ARRAY_BOUNDS_END
        }
    }
    for (auto i : range(n - 1)) {
        for (auto j : range(m - 1)) {
            const Index j1 = j + 1;
            const Index i1 = i + 1;
            facets.row((j * (n - 1) + i) * 2 + 0) << j * n + i, j * n + i1, j1 * n + i;
            facets.row((j * (n - 1) + i) * 2 + 1) << j * n + i1, j1 * n + i1, j1 * n + i;
        }
    }

    // Only perturb in x and y
    // otherwise the analytical ellipse perimeter computations will not be accurate
    vertices.leftCols(2) += VertexArray::Random(vertices.rows(), 2) * delta * (1. / std::max(n, m));
    return create_mesh(vertices, facets);
};

void verify_vertex_positions(const MeshType& m, const MarchingTrianglesOutput& o)
{
    const auto& mesh_vertices = m.get_vertices();
    for (auto i : range(o.vertices.rows())) {
        const auto parent_edge = m.get_edge_vertices(o.vertices_parent_edge[i]);
        const auto v1 = o.vertices.row(i).eval();
        const auto vp1 = mesh_vertices.row(parent_edge[0]).eval();
        const auto vp2 = mesh_vertices.row(parent_edge[1]).eval();
        const auto t = o.vertices_parent_param[i];
        const auto v2 = (vp1 * (1 - t) + vp2 * (t)).eval();
        CHECK((v1 - v2).norm() == Catch::Approx(0).margin(1e-5));
    }
}

} // namespace


TEST_CASE("MarchingTriangles_Stress", "[marching_triangles]")
{
    Index num_dims = invalid<Index>();
    Index n = 20;
    Index m = 35;
    Scalar delta = 0.2f;

    SECTION("2D") { num_dims = 2; }
    SECTION("3D") { num_dims = 3; }

    auto mesh = create_square(n, m, num_dims, delta);
    AttributeArray r2 = AttributeArray::Random(mesh->get_num_vertices(), 1) * 1e-10;
    mesh->add_vertex_attribute("random_attribute");
    mesh->set_vertex_attribute("random_attribute", r2);
    const Scalar isovalue = 0;
    auto out = marching_triangles(*mesh, isovalue, "random_attribute");
    verify_vertex_positions(*mesh, out);

    CHECK(out.edges.rows() > 0);
    for (auto i : range(out.vertices.size())) {
        CHECK(!std::isnan(out.vertices(i)));
        CHECK(out.vertices(i) >= -delta);
        CHECK(out.vertices(i) <= 1 + delta);
    }
}

TEST_CASE("MarchingTriangles_PerimeterOfEllipse", "[marching_triangles]")
{
    const bool should_dump_meshes = false;

    Index num_dims = invalid<Index>();
    Index n = 19;
    Index m = 27;
    Scalar delta = 0.3f;
    Scalar a = 1.2f;
    Scalar b = 0.5f;
    Index num_attrib_cols = 1;
    Index attrib_col = 0;
    std::vector<Scalar> isovalues = {0.025f, 0.035f, 0.05f, 0.075f, 0.1f, 0.2f};

    SECTION("2D") { num_dims = 2; }
    SECTION("3D") { num_dims = 3; }

    // Create the mesh, and perturb it a bit.
    auto mesh = create_square(n, m, num_dims, delta);

    // Define a field
    AttributeArray r2(mesh->get_num_vertices(), num_attrib_cols);
    r2.setZero();
    for (auto i : range(mesh->get_num_vertices())) {
        const auto v = mesh->get_vertices().row(i).eval();
        const Scalar x = (v.x() - 0.5f);
        const Scalar y = (v.y() - 0.5f);
        const Scalar x2 = x * x;
        const Scalar y2 = y * y;
        // const Scalar xy = x * y;
        r2(i, attrib_col) = a * x2 + b * y2;
    }

    mesh->add_vertex_attribute("random_attribute");
    mesh->set_vertex_attribute("random_attribute", r2);

    std::vector<Eigen::MatrixXf> vertices;
    std::vector<Eigen::MatrixXi> edges;
    std::vector<Eigen::VectorXf> edge_attribs;
    for (auto i : range(isovalues.size())) {
        // Find the contour
        const Scalar isovalue = isovalues[i];
        auto out = marching_triangles(*mesh, isovalue, "random_attribute", attrib_col);
        verify_vertex_positions(*mesh, out);

        // compute the perimeter
        Scalar perimeter_computed = 0;
        for (auto eid : range(out.edges.rows())) {
            auto e = out.edges.row(eid).eval();
            perimeter_computed += (out.vertices.row(e(0)) - out.vertices.row(e(1))).norm();
        }

        // Find the analytical value of the perimiter
        const Scalar ea = sqrt(isovalue / a);
        const Scalar eb = sqrt(isovalue / b);
        // https://www.mathsisfun.com/geometry/ellipse-perimeter.html
        const Scalar h = (ea - eb) * (ea - eb) / ((ea + eb) * (ea + eb));
        const Scalar perimeter_analytical =
            safe_cast<Scalar>(M_PI * (ea + eb) * (1 + 3 * h / (10 + sqrt(4 - 3 * h))));

        if (should_dump_meshes) {
            std::cout << perimeter_analytical << " ; " << perimeter_computed << "\n";
        }

        // Check the values
        // Only if the ellipse is fully contained in the square mesh
        if (ea < 0.5 && eb < 0.5) {
            CHECK(perimeter_analytical == Catch::Approx(perimeter_computed).epsilon(0.05));
        }

        // Save the contour for dumping if need be
        if (should_dump_meshes) {
            vertices.emplace_back(out.vertices.cast<float>());
            edges.emplace_back(out.edges.cast<int>());
            edge_attribs.emplace_back(Eigen::VectorXf::Constant(out.edges.rows(), isovalue));
        }
    }

    if (should_dump_meshes) {
        save_edge_network_vtk("isovalues.vtk", vertices, edges, edge_attribs);
        io::save_mesh("isovalues_mesh.vtk", *mesh);
    }
}
