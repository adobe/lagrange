/*
 * Copyright 2021 Adobe. All rights reserved.
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

namespace lagrange {
namespace ui {

/// IDs of default systems, in order of execution
/// See `Systems` class for manipulation of systems execution
struct DefaultSystems
{
    // Stage: Init
    constexpr static const StringID MakeContextCurrent = "MakeContextCurrent"_hs;
    constexpr static const StringID UpdateTime = "UpdateTime"_hs;
    constexpr static const StringID ProcessInput = "ProcessInput"_hs;
    constexpr static const StringID UpdateSelectionOutlineViewport =
        "UpdateSelectionOutlineViewport"_hs;
    constexpr static const StringID UpdateSelectedRenderLayer = "UpdateSelectedRenderLayer"_hs;
    constexpr static const StringID InitMisc = "InitMisc"_hs;


    // Stage: Interface
    constexpr static const StringID DrawUIPanels = "DrawUIPanels"_hs;
    constexpr static const StringID UpdateMeshHovered = "UpdateMeshHovered"_hs;
    constexpr static const StringID UpdateMeshElementsHovered = "UpdateMeshElementsHovered"_hs;
    constexpr static const StringID RunCurrentTool = "RunCurrentTool"_hs;
    constexpr static const StringID UpdateLights = "UpdateLights"_hs;

    // Stage: Simulation
    constexpr static const StringID UpdateTransformHierarchy = "UpdateTransformHierarchy"_hs;
    constexpr static const StringID UpdateMeshBounds = "UpdateMeshBounds"_hs;
    constexpr static const StringID UpdateSceneBounds = "UpdateSceneBounds"_hs;
    constexpr static const StringID UpdateCameraController = "UpdateCameraController"_hs;
    constexpr static const StringID UpdateCameraTurntable = "UpdateCameraTurntable"_hs;
    constexpr static const StringID UpdateCameraFocusFit = "UpdateCameraFocusFit"_hs;

    // Stage: Render
    constexpr static const StringID UpdateAcceleratedPicking = "UpdateAcceleratedPicking"_hs;
    constexpr static const StringID UpdateMeshBuffers = "UpdateMeshBuffers"_hs;
    constexpr static const StringID SetupVertexData = "SetupVertexData"_hs;
    constexpr static const StringID RenderShadowMaps = "RenderShadowMaps"_hs;
    constexpr static const StringID RenderViewports = "RenderViewports"_hs;

    // Stage: Post
    constexpr static const StringID ClearDirtyFlags = "ClearDirtyFlags"_hs;
};

} // namespace ui
} // namespace lagrange
