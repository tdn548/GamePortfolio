#version 300 es

out vec2 varScreenTexturePos;
layout(location = 0) in vec3 attPosition;

void main()
{
    varScreenTexturePos = (vec2(attPosition.x, attPosition.y) / vec2(2.0)) + vec2(0.5);
    gl_Position = vec4(attPosition, 1.0);
}

