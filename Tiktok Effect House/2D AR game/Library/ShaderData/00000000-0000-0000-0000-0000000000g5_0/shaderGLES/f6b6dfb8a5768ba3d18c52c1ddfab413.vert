#version 300 es

uniform mat4 mvpMat;

layout(location = 0) in vec3 attPosition;
out vec2 uv;
layout(location = 1) in vec2 attTexcoord0;

void main()
{
    vec4 pos = vec4(attPosition.xy, 0.0, 1.0);
    gl_Position = mvpMat * pos;
    uv = attTexcoord0;
}

