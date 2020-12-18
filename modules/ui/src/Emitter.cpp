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
#include <lagrange/ui/Emitter.h>

namespace lagrange {
namespace ui {

Emitter::Emitter(Eigen::Vector3f intensity)
: m_enabled(true)
, m_intensity(intensity)
{}

bool Emitter::is_enabled() const
{
    return m_enabled;
}

Emitter::~Emitter()
{
    m_callbacks.call<OnDestroy>(*this);
}

void Emitter::set_enabled(bool val)
{
    m_enabled = val;
}

void Emitter::set_intensity(const Eigen::Vector3f& intensity)
{
    m_intensity = intensity;
    m_callbacks.call<OnChange>(*this);
}

Eigen::Vector3f Emitter::get_intensity() const
{
    return m_intensity;
}

}
}
