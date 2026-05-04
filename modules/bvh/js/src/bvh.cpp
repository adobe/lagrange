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

#include "bind_types.h"

#include <lagrange/bvh/compute_mesh_distances.h>
#include <lagrange/bvh/weld_vertices.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

namespace {

using namespace lagrange;
using namespace lagrange::js::bind;
using val = emscripten::val;

MappingPolicy parse_mapping_policy(const std::string& s)
{
    if (s == "keepFirst") return MappingPolicy::KeepFirst;
    if (s == "error") return MappingPolicy::Error;
    return MappingPolicy::Average;
}

} // namespace

EMSCRIPTEN_BINDINGS(lagrange_bvh)
{
    using namespace emscripten;

    function(
        "computeMeshDistances",
        +[](MeshType& source, const MeshType& target, val opts) {
            bvh::MeshDistancesOptions o;
            // output_attribute_name is std::string, safe to bind (unlike string_view)
            if (!opts.isUndefined()) {
                auto name = opts["outputAttributeName"];
                if (!name.isUndefined()) o.output_attribute_name = name.as<std::string>();
            }
            compute_mesh_distances(source, target, o);
        });

    function(
        "computeHausdorff",
        +[](const MeshType& source, const MeshType& target) -> Scalar {
            return bvh::compute_hausdorff(source, target);
        });

    function(
        "computeChamfer",
        +[](const MeshType& source, const MeshType& target) -> Scalar {
            return bvh::compute_chamfer(source, target);
        });

    function(
        "weldVertices",
        +[](MeshType& mesh, val opts) {
            bvh::WeldOptions o;
            apply_opt(opts, "radius", o.radius);
            apply_opt(opts, "boundaryOnly", o.boundary_only);
            if (!opts.isUndefined()) {
                auto cpf = opts["collisionPolicyFloat"];
                if (!cpf.isUndefined())
                    o.collision_policy_float = parse_mapping_policy(cpf.as<std::string>());
                auto cpi = opts["collisionPolicyIntegral"];
                if (!cpi.isUndefined())
                    o.collision_policy_integral = parse_mapping_policy(cpi.as<std::string>());
            }
            bvh::weld_vertices(mesh, std::move(o));
        });
}
