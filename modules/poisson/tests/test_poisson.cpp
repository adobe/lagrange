////////////////////////////////////////////////////////////////////////////////
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/poisson/mesh_from_oriented_points.h>
#include <lagrange/testing/common.h>
#include <lagrange/topology.h>
#include <lagrange/views.h>

#include <catch2/catch_test_macros.hpp>
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("PoissonRecon: Simple", "[poisson]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::poisson::ReconstructionOptions recon_options;

    auto input_mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");
    lagrange::compute_vertex_normal(input_mesh);
    input_mesh.clear_facets();

    auto mesh1 = lagrange::poisson::mesh_from_oriented_points(input_mesh, recon_options);
    auto mesh2 = lagrange::poisson::mesh_from_oriented_points(input_mesh, recon_options);

    REQUIRE(mesh1.get_num_facets() > 0);

    REQUIRE(vertex_view(mesh1) == vertex_view(mesh2));
    REQUIRE(facet_view(mesh1) == facet_view(mesh2));
}

TEST_CASE("PoissonRecon: Octree", "[poisson]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::poisson::ReconstructionOptions recon_options;

    auto input_mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");
    lagrange::compute_vertex_normal(input_mesh);
    input_mesh.clear_facets();

    std::vector<Index> expected_nf = {11296, 8, 104, 504, 2056, 8008};
    for (unsigned depth = 0; depth < 6; ++depth) {
        recon_options.octree_depth = depth;
        recon_options.use_dirichlet_boundary = true;
        auto mesh = lagrange::poisson::mesh_from_oriented_points(input_mesh, recon_options);
        REQUIRE(mesh.get_num_facets() == expected_nf[depth]);
        REQUIRE(compute_euler(mesh) == 2);
        REQUIRE(is_manifold(mesh));
    }
}

namespace {

template <typename Scalar, typename Index>
void poisson_recon_with_colors()
{
    lagrange::poisson::ReconstructionOptions recon_options;
    recon_options.interpolated_attribute_name = "Vertex_Color";
    recon_options.output_vertex_depth_attribute_name = "value";

    auto input_mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/poisson/sphere.striped.ply");
    input_mesh.clear_facets();

    auto mesh1 = lagrange::poisson::mesh_from_oriented_points(input_mesh, recon_options);
    auto mesh2 = lagrange::poisson::mesh_from_oriented_points(input_mesh, recon_options);

    REQUIRE(mesh1.has_attribute("Vertex_Color"));
    REQUIRE(mesh1.has_attribute("value"));

    REQUIRE(mesh1.get_num_facets() > 0);
    REQUIRE(compute_euler(mesh1) == 2);
    REQUIRE(is_manifold(mesh1));

    REQUIRE(vertex_view(mesh1) == vertex_view(mesh2));
    REQUIRE(facet_view(mesh1) == facet_view(mesh2));
}

} // namespace

TEST_CASE("PoissonRecon: Colors", "[poisson]")
{
    poisson_recon_with_colors<float, uint32_t>();
    poisson_recon_with_colors<double, uint32_t>();
}
