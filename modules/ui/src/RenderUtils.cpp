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
#include <lagrange/ui/IBL.h>
#include <lagrange/ui/Light.h>
#include <lagrange/ui/Material.h>
#include <lagrange/ui/RenderUtils.h>
#include <lagrange/ui/render_passes/ShadowMapPass.h>
#include <lagrange/utils/strings.h>

namespace lagrange {
namespace ui {
namespace utils {
namespace render {

std::string get_shader_light_array_name(Emitter::Type type)
{
    switch (type) {
    case Emitter::Type::SPOT: return "spot_lights";
    case Emitter::Type::DIRECTIONAL: return "directional_lights";
    case Emitter::Type::POINT: return "point_lights";
    default: break;
    }

    return "";
}


bool light_to_shader(Shader& shader,
    const EmitterRenderData& emitter_data,
    std::array<int, 4>& emitter_type_counter,
    int& texture_unit_counter)
{
    if (!emitter_data.emitter) {
        assert(false);
        return false;
    }

    auto& emitter = *emitter_data.emitter;
    if (emitter.get_type() == Emitter::Type::IBL) {
        assert(false);
        return false;
    }

    auto& index = emitter_type_counter[static_cast<int>(emitter.get_type())];

    const auto arr_name = get_shader_light_array_name(emitter_data.emitter->get_type());
    const auto element_name = string_format("{}[{}]", arr_name, index);
    const auto counter_name = arr_name + "_count";


    // Upload specific light properties
    if (emitter.get_type() == Emitter::Type::SPOT) {
        const auto& spot = reinterpret_cast<const SpotLight&>(emitter);
        shader[element_name + ".position"] = spot.get_position();
        shader[element_name + ".direction"] = spot.get_direction();
        shader[element_name + ".cone_angle"] = spot.get_cone_angle();
        shader[element_name + ".PV"] = emitter_data.PV; // todo move to spot light?
    } else if (emitter.get_type() == Emitter::Type::DIRECTIONAL) {
        const auto& dir = reinterpret_cast<const DirectionalLight&>(emitter);
        shader[element_name + ".direction"] = dir.get_direction();
        shader[element_name + ".PV"] = emitter_data.PV; // todo move to directional light?
    } else if (emitter.get_type() == Emitter::Type::POINT) {
        const auto& point = reinterpret_cast<const PointLight&>(emitter);
        shader[element_name + ".position"] = point.get_position();
    }

    // Shared properties
    shader[element_name + ".intensity"] = emitter.get_intensity();
    shader[element_name + ".shadow_near"] = emitter_data.shadow_near;
    shader[element_name + ".shadow_far"] = emitter_data.shadow_far;

    auto& texture = *emitter_data.shadow_map;

    texture.bind_to(GL_TEXTURE0 + texture_unit_counter);
    shader[element_name + ".shadow_map"] = texture_unit_counter;


    texture_unit_counter++;
    index++;


    shader[counter_name] = index;

    return true;
}

bool ibl_to_shader(Shader& shader,
    const EmitterRenderData& emitter_data,
    std::array<int, 4>& emitter_type_counter,
    int& texture_unit_counter)
{
    if (!emitter_data.emitter) {
        assert(false);
        return false;
    }
    auto& emitter = *emitter_data.emitter;
    if (emitter.get_type() != Emitter::Type::IBL) {
        assert(false);
        return false;
    }

    auto& counter = emitter_type_counter[static_cast<int>(Emitter::Type::IBL)];
    // Only one IBL at a time
    assert(counter == 0);

    const auto& ibl = reinterpret_cast<const IBL&>(emitter);

    const auto& diffuse = ibl.get_diffuse();
    diffuse.bind_to(GL_TEXTURE0 + texture_unit_counter);
    shader["ibl_diffuse"] = texture_unit_counter++;

    const auto& specular = ibl.get_specular();
    specular.bind_to(GL_TEXTURE0 + texture_unit_counter);
    shader["ibl_specular"] = texture_unit_counter++;
    shader["ibl_specular_levels"] = static_cast<int>(std::log2(specular.get_width()));

    shader["has_ibl"] = true;

    counter++;

    return true;
}



bool emitter_to_shader(Shader& shader,
    const EmitterRenderData& emitter_data,
    std::array<int, 4>& emitter_type_counter,
    int& texture_unit_counter)
{
    if (emitter_data.emitter && emitter_data.emitter->get_type() == Emitter::Type::IBL) {
        return utils::render::ibl_to_shader(
            shader, emitter_data, emitter_type_counter, texture_unit_counter);
    }

    return utils::render::light_to_shader(
        shader, emitter_data, emitter_type_counter, texture_unit_counter);
}


int assign_empty_emitter_units(
    Shader& shader, std::array<int, 4>& emitter_type_counter, int& texture_unit_counter)
{
    int unassigned_counter = 0;
    for (auto type : {Emitter::Type::POINT, Emitter::Type::DIRECTIONAL, Emitter::Type::SPOT}) {
        const auto arr_name = get_shader_light_array_name(type);

        auto assigned_count = emitter_type_counter[static_cast<int>(type)];

        if (assigned_count == 0) {
            const auto counter_name = arr_name + "_count";
            shader[counter_name] = 0;
        }

        int max_lights = 0;
        if (type == Emitter::Type::POINT)
            max_lights = MAX_DEFAULT_LIGHTS_POINT;
        else
            max_lights = MAX_DEFAULT_LIGHTS_SPOT_DIR / 2;

        for (auto i = assigned_count; i < max_lights; i++) {
            const auto element_name = string_format("{}[{}]", arr_name, i);
            shader[element_name + ".shadow_map"] = texture_unit_counter++;
            unassigned_counter++;
        }
    }


    auto ibl_counter = emitter_type_counter[static_cast<int>(Emitter::Type::IBL)];
    if (ibl_counter == 0) {
        shader["has_ibl"] = false;
        shader["ibl_diffuse"] = texture_unit_counter++;
        shader["ibl_specular"] = texture_unit_counter++;
        unassigned_counter += 2;
    }

    return unassigned_counter;
}

std::pair<Eigen::Vector3f, Eigen::Vector3f> compute_perpendicular_plane(Eigen::Vector3f direction)
{
    Eigen::Vector3f & u1 = direction;
    Eigen::Vector3f v2;

    if (std::abs(direction.x()) == 1.0f && direction.y() == 0.0f && direction.z() == 0.0f) {
        v2 = Eigen::Vector3f(0, 1, 0);
    }
    else {
        v2 = Eigen::Vector3f(1, 0, 0);
    }

    Eigen::Vector3f u2 = v2 - vector_projection(v2, u1);

    Eigen::Vector3f u3 = u2.cross(u1);

    return {u2.normalized(), u3.normalized()};
}


void set_emitter_uniforms(
    Shader& shader, const std::vector<EmitterRenderData>& emitters, int& tex_units_global)
{
    //Go through emitters from previous shadow mapping pass
    std::array<int, 4> emitter_type_counter = {0, 0, 0, 0};

    // Upload emitters to shader and use first available texture unit
    for (auto& emitter_data : emitters) {
        emitter_to_shader(shader, emitter_data, emitter_type_counter, tex_units_global);
    }

    assign_empty_emitter_units(shader, emitter_type_counter, tex_units_global);
}

void set_pbr_uniforms(Shader& shader, Texture& brdf_lut_texture, int& tex_unit_counter) {
    brdf_lut_texture.bind_to(GL_TEXTURE0 + tex_unit_counter);
    shader["ibl_brdf_lut"] = tex_unit_counter++;
}

void set_render_layer(GLScope& scope, int layer_index) {
    scope(glEnable, GL_POLYGON_OFFSET_FILL);
    scope(glPolygonOffset, 1.0f, float(layer_index) * -1.0f);
}

void set_map(Shader& shader, const std::string& name, const Material& material, int& tex_unit_counter)
{
    auto& map = material[name];

    // Set tex unit to sampler even if not used
    const int tex_unit = tex_unit_counter++;
    shader["material." + name + ".texture"] = tex_unit;

    if (map.texture) {
        map.texture->bind_to(GL_TEXTURE0 + tex_unit);
        shader["material." + name + ".has_texture"] = true;
    } else {
        shader["material." + name + ".value"] = map.value.to_vec4();
        shader["material." + name + ".has_texture"] = false;
    }
}

void set_render_pass_defaults(GLScope& scope) {
    scope(glEnable, GL_DEPTH_CLAMP);
    scope(glDepthFunc, GL_LEQUAL);

    scope(glEnable, GL_BLEND);
    scope(glBlendFuncSeparate, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    scope(glBlendEquation, GL_FUNC_ADD);

    scope(glDisable, GL_MULTISAMPLE);

    scope(glEnable, GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

} // namespace render
} // namespace utils
} // namespace ui
} // namespace lagrange
