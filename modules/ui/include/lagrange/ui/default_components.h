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


#include <lagrange/ui/components/AcceleratedPicking.h>
#include <lagrange/ui/components/AttributeRender.h>
#include <lagrange/ui/components/Bounds.h>
#include <lagrange/ui/components/CameraComponents.h>
#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/components/ElementSelection.h>
#include <lagrange/ui/components/EventEmitter.h>
#include <lagrange/ui/components/GLMesh.h>
#include <lagrange/ui/components/IBL.h>
#include <lagrange/ui/components/Input.h>
#include <lagrange/ui/components/Layer.h>
#include <lagrange/ui/components/Light.h>
#include <lagrange/ui/components/MeshData.h>
#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/components/MeshRender.h>
#include <lagrange/ui/components/MeshSelectionRender.h>
#include <lagrange/ui/components/ObjectIDViewport.h>
#include <lagrange/ui/components/RenderContext.h>
#include <lagrange/ui/components/Selection.h>
#include <lagrange/ui/components/SelectionContext.h>
#include <lagrange/ui/components/SelectionViewport.h>
#include <lagrange/ui/components/ShadowMap.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/components/TreeNode.h>
#include <lagrange/ui/components/UIPanel.h>
#include <lagrange/ui/components/VertexData.h>
#include <lagrange/ui/components/Viewport.h>


namespace lagrange {
namespace ui {


/*
    Register UI widgets for all the default components
*/
void register_default_component_widgets();

} // namespace ui
} // namespace lagrange