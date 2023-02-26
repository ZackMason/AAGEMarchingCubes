#version 460 core
out vec4 FragColor;

in vec2 voPos;
in vec2 voUV;
flat in uint voTexture;
flat in uint voColor;

uniform sampler2D uTextures[32];



void main() {
    uint is_font = voTexture & (1 << 30);
    uint actual_texture = voTexture & ~(1<<30);
    bool is_texture = actual_texture < 32 && actual_texture >= 0;
    // opt use mix
    vec4 color = (is_texture) ? 
        texture(uTextures[actual_texture], voUV) : 
        vec4(1.0);
   
    if (is_font != 0) {
        color = color.rrrr;
        color.a = step(0.5, color.a);
    }

    color *= vec4(
        float( voColor&0xff),
        float((voColor&0xff00)>>8),
        float((voColor&0xff0000)>>16),
        float((voColor&0xff000000)>>24))/255.0;

    FragColor.rgb = pow(color.rgb, vec3(2.2));
    FragColor.a = clamp(color.a, 0.0, 1.0);

    if (FragColor.a < 0.01) {
        discard;
    }
}