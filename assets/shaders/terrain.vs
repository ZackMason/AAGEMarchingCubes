#version 460 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec3 a_col;

out VS_OUT {
    vec3 pos;
    vec3 nrm;
    vec3 col;
} vs_out;

uniform mat4 uVP;
uniform mat4 uM = mat4(1.0);

void main() {
    vs_out.pos = vec3(uM * vec4(a_pos, 1.0));

    vs_out.nrm = a_nrm;
    vs_out.col = a_col;

    gl_Position = uVP * vec4(vs_out.pos, 1.0);
}
