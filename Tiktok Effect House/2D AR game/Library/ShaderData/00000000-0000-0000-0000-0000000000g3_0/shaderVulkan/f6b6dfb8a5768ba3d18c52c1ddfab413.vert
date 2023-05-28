#version 300 es

uniform mvpMat { mat4 _mvpMat; };

layout(location = 0) in vec3 attPosition;
out vec2 uv;
layout(location = 1) in vec2 attTexcoord0;

void main()
{
    vec4 pos = vec4(attPosition.xy, 0.0, 1.0);
    gl_Position = _mvpMat * pos;
    uv = attTexcoord0;
    gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5f;
}

