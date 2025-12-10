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

#include <lagrange/orient_outward.h>

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_components.h>
#include <lagrange/views.h>

#include <stack>

namespace lagrange {

namespace {

template <typename T>
bool contains(const std::vector<T>& v, const T& x)
{
    return std::find(v.begin(), v.end(), x) != v.end();
}

template <typename Scalar, typename Index>
std::vector<bool> bfs_orient(const lagrange::SurfaceMesh<Scalar, Index>& mesh)
{
    // Mesh traversal to orient facets consistently.

    // 1. Find oriented patches, i.e. connected components obtained by traversing oriented edges.
    std::vector<Index> facet_to_patch(mesh.get_num_facets(), 0);
    std::vector<std::vector<Index>> adj_patches;
    Index num_patches = 0;
    {
        std::vector<bool> marked(mesh.get_num_facets(), false);
        std::stack<Index> pending;
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            if (marked[f]) {
                continue;
            }
            marked[f] = true;
            pending.push(f);
            facet_to_patch[f] = num_patches;
            adj_patches.push_back({});
            while (!pending.empty()) {
                Index fi = pending.top();
                pending.pop();
                const Index pi = num_patches;
                for (Index ci = mesh.get_facet_corner_begin(fi);
                     ci != mesh.get_facet_corner_end(fi);
                     ++ci) {
                    const Index vi = mesh.get_corner_vertex(ci);
                    const Index e = mesh.get_corner_edge(ci);
                    mesh.foreach_corner_around_edge(e, [&](Index cj) {
                        const Index fj = mesh.get_corner_facet(cj);
                        const Index vj = mesh.get_corner_vertex(cj);
                        if (!marked[fj] && vi != vj) {
                            // TODO: Assert that vertex(ci + 1) == vj and vertex(cj + i) == vi
                            la_debug_assert(fi != fj);
                            marked[fj] = true;
                            pending.push(fj);
                            facet_to_patch[fj] = pi;
                        }
                        // Adjacent facets with different patch numbers are connected through
                        // non-oriented edges. Make them adjacent in our "patch graph".
                        if (marked[fj] && facet_to_patch[fj] != pi) {
                            const Index pj = facet_to_patch[fj];
                            if (!contains(adj_patches[pi], pj)) {
                                adj_patches[pi].push_back(pj);
                                adj_patches[pj].push_back(pi);
                            }
                        }
                    });
                }
            }
            ++num_patches;
        }
    }

    // 2. Traverse the patch graph and greedily flip adjacent oriented patches. If the mesh is
    // orientable, this will result in an consistently oriented mesh. Otherwise (e.g. klein bottle
    // or mobius strip), we just get... something.
    std::vector<bool> should_flip_patch(num_patches, false);
    {
        std::vector<bool> marked(num_patches, false);
        std::stack<Index> pending;
        for (Index p = 0; p < num_patches; ++p) {
            if (marked[p]) {
                continue;
            }
            marked[p] = true;
            pending.push(p);
            while (!pending.empty()) {
                Index pi = pending.top();
                pending.pop();
                for (Index pj : adj_patches[pi]) {
                    if (!marked[pj]) {
                        marked[pj] = true;
                        pending.push(pj);
                        should_flip_patch[pj] = !should_flip_patch[pi];
                    }
                }
            }
        }
    }

    // 3. Mark facets as needing to be flipped based on patch information
    std::vector<bool> should_flip(mesh.get_num_facets(), false);
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        should_flip[f] = should_flip_patch[facet_to_patch[f]];
    }

    return should_flip;
}

} // namespace

template <typename Scalar, typename Index>
void orient_outward(lagrange::SurfaceMesh<Scalar, Index>& mesh, const OrientOptions& options)
{
    la_runtime_assert(mesh.get_dimension() == 3);

    bool had_edges = mesh.has_edges();
    mesh.initialize_edges();

    // Orient facets consistently within a connected component (if possible)
    auto should_flip = bfs_orient(mesh);

    // Compute connected components
    ComponentOptions component_options;
    component_options.output_attribute_name = "@__component_id__";
    auto num_components = compute_components(mesh, component_options);
    auto components =
        mesh.template get_attribute<Index>(component_options.output_attribute_name).get_all();

    // Compute signed volumes per component
    auto tetra_signed_volume = [](const Eigen::RowVector3d& p1,
                                  const Eigen::RowVector3d& p2,
                                  const Eigen::RowVector3d& p3,
                                  const Eigen::RowVector3d& p4) {
        return (p2 - p1).dot((p3 - p1).cross(p4 - p1)) / 6.0;
    };

    std::vector<Scalar> signed_volumes(num_components, 0);
    {
        auto vertices = vertex_view(mesh).template leftCols<3>().template cast<double>();
        Eigen::RowVector3d zero = Eigen::RowVector3d::Zero();
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            auto facet = mesh.get_facet_vertices(f);
            Index nv = mesh.get_facet_size(f);
            Scalar sign = (should_flip[f] ? -1 : 1);
            if (nv < 3) {
                continue;
            } else if (nv == 3) {
                signed_volumes[components[f]] -= sign * tetra_signed_volume(
                                                            vertices.row(facet[0]),
                                                            vertices.row(facet[1]),
                                                            vertices.row(facet[2]),
                                                            zero);
            } else {
                Eigen::RowVector3d bary = zero;
                for (Index lv = 0; lv < nv; ++lv) {
                    bary += vertices.row(facet[lv]);
                }
                bary /= nv;
                for (Index lv = 0; lv < nv; ++lv) {
                    signed_volumes[components[f]] -= sign * tetra_signed_volume(
                                                                vertices.row(facet[lv]),
                                                                vertices.row(facet[(lv + 1) % nv]),
                                                                bary,
                                                                zero);
                }
            }
        }
    }

    // Reverse orientation of each facet whose component's signed volume doesn't match the desired
    // orientation
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        // signbit is true if positive, false if negative
        if (std::signbit(signed_volumes[components[f]]) != !options.positive) {
            should_flip[f] = !should_flip[f];
        }
    }

    // Flip facets in each component with incorrect volume sign
    if (!had_edges) {
        mesh.clear_edges();
    }
    mesh.delete_attribute(component_options.output_attribute_name);
    mesh.flip_facets([&](Index f) { return should_flip[f]; }, AttributeReorientPolicy::Reorient);
}

#define LA_X_orient_outward(_, Scalar, Index) \
    template LA_CORE_API void orient_outward( \
        SurfaceMesh<Scalar, Index>& mesh,     \
        const OrientOptions& options);
LA_SURFACE_MESH_X(orient_outward, 0)

} // namespace lagrange
