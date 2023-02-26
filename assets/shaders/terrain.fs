#version 460 core

out vec4 FragColor;

in VS_OUT {
    vec3 pos;
    vec3 nrm;
    vec3 col;
} fs_in;

void main() {
    float ndl = dot(normalize(vec3(1)), normalize(fs_in.nrm));

    FragColor.rgb = fs_in.col * clamp(ndl, 0.40, 1.0);
    FragColor.a = 1.0;
}