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

#include <lagrange/ui/BaseObject.h>
#include <lagrange/ui/Callbacks.h>
#include <lagrange/ui/utils/math.h>

namespace lagrange {
namespace ui {

class Emitter : public BaseObject, public CallbacksBase<Emitter> {


public:

    Emitter(Eigen::Vector3f intensity = Eigen::Vector3f::Constant(1.0f));

    using OnChange = UI_CALLBACK(void(Emitter&));
    using OnDestroy = UI_CALLBACK(void(Emitter&));

    enum class Type {
        POINT = 0,
        DIRECTIONAL,
        SPOT,
        IBL
    };

    virtual Type get_type() const = 0;

    bool is_enabled() const;
    void set_enabled (bool val);

    void set_intensity(const Eigen::Vector3f& intensity);

    Eigen::Vector3f get_intensity() const;

    virtual ~Emitter();


private:
    bool m_enabled;

protected:
    Callbacks<OnChange, OnDestroy> m_callbacks;
    Eigen::Vector3f m_intensity;

friend CallbacksBase<Emitter>;
};

}
}
