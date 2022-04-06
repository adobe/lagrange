
layout(location = 0) out vec4 fragColor;

/*in VARYING {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 tangent;
    vec3 bitangent;
} fs_in;*/


in vec3 vs_out_pos;
in vec3 vs_out_normal;
in vec2 vs_out_uv;
in vec4 vs_out_color;
in vec3 vs_out_tangent;
in vec3 vs_out_bitangent;


