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

#include <lagrange/primitive/generate_disc.h>
#include <lagrange/primitive/generate_icosahedron.h>
#include <lagrange/primitive/generate_octahedron.h>
#include <lagrange/primitive/generate_rounded_cone.h>
#include <lagrange/primitive/generate_rounded_cube.h>
#include <lagrange/primitive/generate_rounded_plane.h>
#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/primitive/generate_subdivided_sphere.h>
#include <lagrange/primitive/generate_swept_surface.h>
#include <lagrange/primitive/generate_torus.h>

#include <emscripten/bind.h>

#include <vector>

namespace {

using namespace emscripten;
using lagrange::js::bind::apply_opt;

/// Apply common PrimitiveOptions fields from a JS object.
/// Attribute name fields (normal_attribute_name, uv_attribute_name, semantic_label_attribute_name)
/// are omitted because they are std::string_view and cannot safely bind to transient JS strings.
void apply_primitive_opts(const val& opts, lagrange::primitive::PrimitiveOptions& o)
{
    // JS default: triangulate = true (C++ default is false).
    // Set unconditionally, then allow opts to override.
    o.triangulate = true;
    if (opts.isUndefined()) return;
    auto center = opts["center"];
    if (!center.isUndefined()) {
        o.center[0] = center[0].as<float>();
        o.center[1] = center[1].as<float>();
        o.center[2] = center[2].as<float>();
    }
    apply_opt(opts, "withTopCap", o.with_top_cap);
    apply_opt(opts, "withBottomCap", o.with_bottom_cap);
    apply_opt(opts, "withCrossSection", o.with_cross_section);
    apply_opt(opts, "fixedUv", o.fixed_uv);
    apply_opt(opts, "distThreshold", o.dist_threshold);
    apply_opt(opts, "angleThreshold", o.angle_threshold);
    apply_opt(opts, "epsilon", o.epsilon);
    apply_opt(opts, "uvPadding", o.uv_padding);
    apply_opt(opts, "triangulate", o.triangulate);
}

} // namespace

EMSCRIPTEN_BINDINGS(lagrange_primitive)
{
    using namespace emscripten;
    using namespace lagrange;

    function(
        "generateSphere",
        +[](val opts) -> js::bind::MeshType {
            primitive::SphereOptions o;
            apply_primitive_opts(opts, o);
            apply_opt(opts, "radius", o.radius);
            apply_opt(opts, "numLongitudeSections", o.num_longitude_sections);
            apply_opt(opts, "numLatitudeSections", o.num_latitude_sections);
            apply_opt(opts, "startSweepAngle", o.start_sweep_angle);
            apply_opt(opts, "endSweepAngle", o.end_sweep_angle);
            return primitive::generate_sphere<js::bind::Scalar, js::bind::Index>(std::move(o));
        });

    function(
        "generateTorus",
        +[](val opts) -> js::bind::MeshType {
            primitive::TorusOptions o;
            apply_primitive_opts(opts, o);
            apply_opt(opts, "majorRadius", o.major_radius);
            apply_opt(opts, "minorRadius", o.minor_radius);
            apply_opt(opts, "ringSegments", o.ring_segments);
            apply_opt(opts, "pipeSegments", o.pipe_segments);
            apply_opt(opts, "startSweepAngle", o.start_sweep_angle);
            apply_opt(opts, "endSweepAngle", o.end_sweep_angle);
            return primitive::generate_torus<js::bind::Scalar, js::bind::Index>(std::move(o));
        });

    function(
        "generateRoundedCube",
        +[](val opts) -> js::bind::MeshType {
            primitive::RoundedCubeOptions o;
            apply_primitive_opts(opts, o);
            apply_opt(opts, "width", o.width);
            apply_opt(opts, "height", o.height);
            apply_opt(opts, "depth", o.depth);
            apply_opt(opts, "widthSegments", o.width_segments);
            apply_opt(opts, "heightSegments", o.height_segments);
            apply_opt(opts, "depthSegments", o.depth_segments);
            apply_opt(opts, "bevelRadius", o.bevel_radius);
            apply_opt(opts, "bevelSegments", o.bevel_segments);
            return primitive::generate_rounded_cube<js::bind::Scalar, js::bind::Index>(
                std::move(o));
        });

    function(
        "generateRoundedCone",
        +[](val opts) -> js::bind::MeshType {
            primitive::RoundedConeOptions o;
            apply_primitive_opts(opts, o);
            apply_opt(opts, "radiusTop", o.radius_top);
            apply_opt(opts, "radiusBottom", o.radius_bottom);
            apply_opt(opts, "height", o.height);
            apply_opt(opts, "bevelRadiusTop", o.bevel_radius_top);
            apply_opt(opts, "bevelRadiusBottom", o.bevel_radius_bottom);
            apply_opt(opts, "radialSections", o.radial_sections);
            apply_opt(opts, "bevelSegmentsTop", o.bevel_segments_top);
            apply_opt(opts, "bevelSegmentsBottom", o.bevel_segments_bottom);
            apply_opt(opts, "sideSegments", o.side_segments);
            apply_opt(opts, "startSweepAngle", o.start_sweep_angle);
            apply_opt(opts, "endSweepAngle", o.end_sweep_angle);
            return primitive::generate_rounded_cone<js::bind::Scalar, js::bind::Index>(
                std::move(o));
        });

    function(
        "generateDisc",
        +[](val opts) -> js::bind::MeshType {
            primitive::DiscOptions o;
            apply_primitive_opts(opts, o);
            apply_opt(opts, "radius", o.radius);
            apply_opt(opts, "startAngle", o.start_angle);
            apply_opt(opts, "endAngle", o.end_angle);
            apply_opt(opts, "radialSections", o.radial_sections);
            apply_opt(opts, "numRings", o.num_rings);
            return primitive::generate_disc<js::bind::Scalar, js::bind::Index>(std::move(o));
        });

    function(
        "generateOctahedron",
        +[](val opts) -> js::bind::MeshType {
            primitive::OctahedronOptions o;
            apply_primitive_opts(opts, o);
            apply_opt(opts, "radius", o.radius);
            return primitive::generate_octahedron<js::bind::Scalar, js::bind::Index>(std::move(o));
        });

    function(
        "generateIcosahedron",
        +[](val opts) -> js::bind::MeshType {
            primitive::IcosahedronOptions o;
            apply_primitive_opts(opts, o);
            apply_opt(opts, "radius", o.radius);
            return primitive::generate_icosahedron<js::bind::Scalar, js::bind::Index>(std::move(o));
        });

    function(
        "generateRoundedPlane",
        +[](val opts) -> js::bind::MeshType {
            primitive::RoundedPlaneOptions o;
            apply_primitive_opts(opts, o);
            apply_opt(opts, "width", o.width);
            apply_opt(opts, "height", o.height);
            apply_opt(opts, "bevelRadius", o.bevel_radius);
            apply_opt(opts, "widthSegments", o.width_segments);
            apply_opt(opts, "heightSegments", o.height_segments);
            apply_opt(opts, "bevelSegments", o.bevel_segments);
            return primitive::generate_rounded_plane<js::bind::Scalar, js::bind::Index>(
                std::move(o));
        });

    function(
        "generateSubdividedSphere",
        +[](val opts) -> js::bind::MeshType {
            primitive::SubdividedSphereOptions o;
            apply_primitive_opts(opts, o);
            apply_opt(opts, "radius", o.radius);
            apply_opt(opts, "subdivLevel", o.subdiv_level);
            // Generate base icosahedron, then subdivide
            auto base = primitive::generate_icosahedron<js::bind::Scalar, js::bind::Index>({});
            return primitive::generate_subdivided_sphere<js::bind::Scalar, js::bind::Index>(
                base,
                std::move(o));
        });

    function(
        "generateSweptSurface",
        +[](val profile_js, val sweep_js, val opts) -> js::bind::MeshType {
            using Scalar = js::bind::Scalar;
            using Index = js::bind::Index;
            using SweepOpts = primitive::SweepOptions<Scalar>;
            using Point = typename SweepOpts::Point;

            // Parse profile: flat array of [x0,y0, x1,y1, ...] 2D coordinates
            unsigned profile_len = profile_js["length"].as<unsigned>();
            if (profile_len % 2 != 0) {
                throw std::runtime_error(
                    "generateSweptSurface: profile length must be even (flat [x,y] pairs)");
            }
            std::vector<Scalar> profile(profile_len);
            for (unsigned i = 0; i < profile_len; ++i) {
                profile[i] = profile_js[i].as<Scalar>();
            }

            // Build SweepOptions from sweep_js
            bool follow_tangent = true;
            apply_opt(sweep_js, "followTangent", follow_tangent);

            SweepOpts sweep;
            auto type = sweep_js["type"];
            if (!type.isUndefined() && type.as<std::string>() == "linear") {
                auto from_js = sweep_js["from"];
                auto to_js = sweep_js["to"];
                Point from(
                    from_js[0].as<Scalar>(),
                    from_js[1].as<Scalar>(),
                    from_js[2].as<Scalar>());
                Point to(to_js[0].as<Scalar>(), to_js[1].as<Scalar>(), to_js[2].as<Scalar>());
                sweep = SweepOpts::linear_sweep(from, to, follow_tangent);
            } else {
                // Default: circular sweep
                auto point_js = sweep_js["point"];
                auto axis_js = sweep_js["axis"];
                Point point(
                    point_js[0].as<Scalar>(),
                    point_js[1].as<Scalar>(),
                    point_js[2].as<Scalar>());
                Point axis(
                    axis_js[0].as<Scalar>(),
                    axis_js[1].as<Scalar>(),
                    axis_js[2].as<Scalar>());
                Scalar angle = static_cast<Scalar>(2 * lagrange::internal::pi);
                apply_opt(sweep_js, "angle", angle);
                sweep = SweepOpts::circular_sweep(point, axis, angle, follow_tangent);
            }

            // Common sweep config
            size_t num_samples = 16;
            apply_opt(sweep_js, "numSamples", num_samples);
            sweep.set_num_samples(num_samples);

            bool periodic = sweep.is_periodic();
            apply_opt(sweep_js, "periodic", periodic);
            sweep.set_periodic(periodic);

            auto domain_js = sweep_js["domain"];
            if (!domain_js.isUndefined()) {
                sweep.set_domain({
                    domain_js[0].as<Scalar>(),
                    domain_js[1].as<Scalar>(),
                });
            }

            auto pivot_js = sweep_js["pivot"];
            if (!pivot_js.isUndefined()) {
                sweep.set_pivot(Point(
                    pivot_js[0].as<Scalar>(),
                    pivot_js[1].as<Scalar>(),
                    pivot_js[2].as<Scalar>()));
            }

            // Optional JS callback functions
            auto twist_fn = sweep_js["twistFunction"];
            if (!twist_fn.isUndefined()) {
                sweep.set_twist_function(
                    [twist_fn](Scalar t) -> Scalar { return twist_fn(t).as<Scalar>(); });
            }

            auto taper_fn = sweep_js["taperFunction"];
            if (!taper_fn.isUndefined()) {
                sweep.set_taper_function(
                    [taper_fn](Scalar t) -> Scalar { return taper_fn(t).as<Scalar>(); });
            }

            auto offset_fn = sweep_js["offsetFunction"];
            if (!offset_fn.isUndefined()) {
                sweep.set_offset_function(
                    [offset_fn](Scalar t) -> Scalar { return offset_fn(t).as<Scalar>(); });
            }

            // SweptSurfaceOptions
            primitive::SweptSurfaceOptions surface_opts;
            apply_primitive_opts(opts, surface_opts);
            apply_opt(opts, "useUAsProfileLength", surface_opts.use_u_as_profile_length);
            apply_opt(opts, "profileAngleThreshold", surface_opts.profile_angle_threshold);
            apply_opt(opts, "maxProfileLength", surface_opts.max_profile_length);

            return primitive::generate_swept_surface<Scalar, Index>(
                lagrange::span<const Scalar>(profile.data(), profile.size()),
                sweep,
                surface_opts);
        });
}
