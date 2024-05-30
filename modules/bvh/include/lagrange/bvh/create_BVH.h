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
#pragma once

#include <exception>
#include <memory>
#include <string>

#include <lagrange/Mesh.h>
#include <lagrange/bvh/AABBIGL.h>
#include <lagrange/bvh/BVH.h>
#include <lagrange/bvh/BVHNanoflann.h>
#include <lagrange/bvh/BVHType.h>

namespace lagrange {
namespace bvh {

/**
 * Usage:
 *
 * auto engine = lagrange::bvh::create_BVH(engine_type, vertices);
 * auto result = engine->query({0.0, 0.0, 0.0});
 */
template <typename VertexArray, typename ElementArray = lagrange::Triangles>
std::unique_ptr<BVH<VertexArray, ElementArray>> create_BVH( //
    const BVHType engine_type,
    const Eigen::MatrixBase<VertexArray>& vertices)
{
    switch (engine_type) {
    case BVHType::NANOFLANN: {
        using BVH_t = BVHNanoflann<VertexArray, ElementArray>;
        auto engine = std::make_unique<BVH_t>();
        engine->build(vertices);
        return engine;
    }
    default: //
        throw std::runtime_error("Unsupported BVH engine type: " + bvhtype_to_string(engine_type));
    }
}

/**
 * Usage:
 *
 * auto engine = lagrange::bvh::create_BVH(engine_type, vertices, elements);
 * auto result = engine->query({0.0, 0.0, 0.0});
 */
template <typename VertexArray, typename ElementArray>
std::unique_ptr<BVH<VertexArray, ElementArray>> create_BVH( //
    const BVHType engine_type,
    const Eigen::MatrixBase<VertexArray>& vertices,
    const Eigen::MatrixBase<ElementArray>& elements)
{
    switch (engine_type) {
    case BVHType::NANOFLANN: {
        using BVH_t = BVHNanoflann<VertexArray, ElementArray>;
        auto engine = std::make_unique<BVH_t>();
        engine->build(vertices);
        return engine;
    }
    case BVHType::IGL: {
        using BVH_t = AABBIGL<VertexArray, ElementArray>;
        auto engine = std::make_unique<BVH_t>();
        engine->build(vertices, elements);
        return engine;
    }
    default: //
        throw std::runtime_error("Unsupported BVH engine: " + bvhtype_to_string(engine_type));
    }

    // Don't complain dear compiler
    return nullptr;
}

/**
 * Usage:
 *
 * auto engine = lagrange::bvh::create_BVH(engine_type, mesh);
 * auto result = engine->query({0.0, 0.0, 0.0});
 */
template <typename VertexArray, typename ElementArray>
std::unique_ptr<BVH<VertexArray, ElementArray>> create_BVH( //
    const BVHType engine_type,
    const lagrange::Mesh<VertexArray, ElementArray>& mesh)
{
    switch (engine_type) {
    case BVHType::NANOFLANN: {
        using BVH_t = BVHNanoflann<VertexArray, ElementArray>;
        auto engine = std::make_unique<BVH_t>();
        engine->build(mesh.get_vertices());
        return engine;
    }
    case BVHType::IGL: {
        using BVH_t = AABBIGL<VertexArray, ElementArray>;
        auto engine = std::make_unique<BVH_t>();
        engine->build(mesh.get_vertices(), mesh.get_facets());
        return engine;
    }
    default: //
        throw std::runtime_error("Unsupported BVH engine: " + bvhtype_to_string(engine_type));
    }

    // Don't complain dear compiler
    return nullptr;
}


} // namespace bvh
} // namespace lagrange
