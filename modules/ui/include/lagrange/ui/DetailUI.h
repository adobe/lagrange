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

#include <vector>
#include <string>
#include <memory>
#include <unordered_set>
#include <unordered_map>


#include <lagrange/ui/Callbacks.h>
#include <lagrange/ui/UIPanel.h>
#include <lagrange/ui/Selection.h>
#include <lagrange/ui/UIWidget.h>

namespace lagrange {
namespace ui {

class Model;
class Material;
class Emitter;

/*
    UI Panel for Scene
*/
class DetailUI : public UIPanelBase {

public:
    DetailUI(Viewer* viewer)
        : UIPanelBase(viewer)
    {}

    const char* get_title() const override { return "Detail"; };

    void draw() final;

private:

    void draw(Model& model);
    void draw(Material& material, int index = -1);
    void draw(Emitter& emitter);

    struct Pagination {
        int current_page = 0;
        int per_page = 25;
    };

    std::unordered_map<const void *, PaginatedMatrixWidget> m_paginated_matrices;

};

}
}
