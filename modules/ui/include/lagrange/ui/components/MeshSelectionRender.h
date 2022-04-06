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
#pragma once

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/types/Material.h>
#include <memory>

namespace lagrange {
namespace ui {

/// Singleton component storing materials and temporary entities
/// for selected mesh element visualization
struct MeshSelectionRender
{
    Entity facet_render;
    Entity edge_render;
    Entity vertex_render;

    std::shared_ptr<Material> mat_faces_in_facet_mode;
    std::shared_ptr<Material> mat_faces_default;

    std::shared_ptr<Material> mat_edges_in_facet_mode;
    std::shared_ptr<Material> mat_edges_in_edge_mode;
    std::shared_ptr<Material> mat_edges_in_vertex_mode;

    std::shared_ptr<Material> mat_vertices;
};

} // namespace ui
} // namespace lagrange