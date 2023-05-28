#version 300 es

uniform float uniSpanWidth;
uniform float uniSpanHeight;
uniform float uniSliceOffsetWidth;
uniform float uniSliceOffsetHeight;
uniform float uniRtWidth;
uniform float uniRtHeight;

out vec2 varScreenTexturePos;
layout(location = 0) in vec3 attPosition;

void main()
{
    varScreenTexturePos = ((((attPosition.xy + vec2(1.0)) / vec2(2.0)) * vec2(uniSpanWidth, uniSpanHeight)) + vec2(uniSliceOffsetWidth, uniSliceOffsetHeight)) / vec2(uniRtWidth, uniRtHeight);
    gl_Position = vec4(attPosition, 1.0);
}

