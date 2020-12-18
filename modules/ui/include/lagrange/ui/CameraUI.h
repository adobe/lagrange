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

#include <lagrange/ui/utils/math.h>
#include <lagrange/ui/Callbacks.h>
#include <lagrange/ui/UIPanel.h>

namespace lagrange {
namespace ui {

class Camera;

/*
    Displays a window showing camera properties
*/
class CameraUI : public UIPanel<Camera>, public CallbacksBase<CameraUI> {

public:
    CameraUI(Viewer* viewer, std::shared_ptr<Camera> camera)
        : UIPanel<Camera>(viewer, std::move(camera))
    {}

    const char* get_title() const override { return "Camera"; }

    void draw() final;
    void update(double dt) final;

private:

    struct {
        bool enabled = false;
        float t = 1.0f;
        float speed = 1.0f / 4.0f;
        Eigen::Vector3f start_pos;
        Eigen::Vector3f axis = Eigen::Vector3f(0,1,0);
    } m_turntable;

    struct {
        bool rotate_active = false;
        float rotate_speed = 4.0f;
        float zoom_speed = 0.15f;
    } m_control;

friend CallbacksBase<CameraUI>;
};

}
}
