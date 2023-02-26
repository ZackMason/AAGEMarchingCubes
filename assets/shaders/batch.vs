#version 460 core

#include <batch.glsl>

out vec2 voPos;
out vec2 voUV;
flat out uint voTexture;
flat out uint voColor;

uniform vec2 uZoom; // export 1.0

void main()
{
    voUV = aUV.xy;
    voTexture = aTex;
    voColor = aColor;
    voPos = aPos.xy * 2.0 - 1.0;
        
    gl_Position = vec4(voPos, 0.00001 , 1);
}
