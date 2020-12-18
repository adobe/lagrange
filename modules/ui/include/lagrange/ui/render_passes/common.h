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
#include <lagrange/ui/RenderPass.h>
#include <lagrange/ui/default_resources.h>
#include <lagrange/ui/Selection.h>

#include <string>


namespace lagrange {
namespace ui {


class RenderableManager;
class Scene;
class Camera;

/*
    Render Pass Data that get used in most passes
    These resources are global to the pipeline
*/
struct CommonPassData {
    // Constant reference to current scene
    
    const Scene* scene;

    // Constant reference to current camera
    const Camera* camera;

    // Constant reference to current global object selection
    const TwoStateSelection<BaseObject*> * selection_global;

    // Passes that contribute to final image will output to this fbo
    FrameBuffer * final_output_fbo;

    // For layering and avoiding z-fighting
    Resource<int> pass_counter;
};


template <typename Pass>
std::string default_render_pass_name()
{
    return "";
}


} // namespace ui
} // namespace lagrange
