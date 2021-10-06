
#pragma property range_min "RangeMin" Vector(0,0,0,0)
#pragma property range_max "RangeMax" Vector(1,1,1,1)
#pragma property colormap "Colormap" Texture2D() [colormap]

vec4 colormap_fn(vec4 value){
    vec4 nval = (value - range_min.xyzw) / (range_max.xyzw - range_min.xyzw);

    if(!colormap_texture_bound){
        return nval;
    }

    return texture(colormap, nval.xy);
}

vec4 colormap_fn(vec3 value){
    vec3 nval = (value - range_min.xyz) / (range_max.xyz - range_min.xyz);

    if(!colormap_texture_bound){
        return vec4(nval.x, nval.y, nval.z, 1);
    }

    return texture(colormap, nval.xy);
}

vec4 colormap_fn(vec2 value){

    vec2 nval = (value - range_min.xy) / (range_max.xy - range_min.xy);

    if(!colormap_texture_bound){
        return vec4(nval.x, nval.y, 0, 1);
    }

    return texture(colormap, nval.xy);
}


vec4 colormap_fn(float value){
    float nval = (value - range_min.x) / (range_max.x - range_min.x);


    if(!colormap_texture_bound){
        return vec4(nval, nval, nval, 1);
    }

    return texture(colormap, vec2(nval,0));
}

vec4 colormapped_fragment_color(vec4 value){

    vec4 out_color = vec4(0,0,0,1);


    if(range_min.w == range_max.w){
        if(range_min.z == range_max.z){
            if(range_min.y == range_max.y){
                out_color = colormap_fn(value.x);
            }
            else {
                out_color = colormap_fn(vec2(value));
            }
        }
        else {
            out_color = colormap_fn(vec3(value));
        }
    }
    else {
        out_color = colormap_fn(value);
    }

    return out_color;


}

