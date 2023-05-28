#version 300 es
precision highp float;
precision highp int;

uniform mediump sampler2D ganTexture;
uniform mediump sampler2D maskTexture;
uniform mediump enable { float _enable; };

in mediump vec2 uv;
layout(location = 0) out mediump vec4 color;

void main()
{
    mediump vec4 ganColor = texture(ganTexture, uv);
    mediump vec4 maskColor = texture(maskTexture, uv);
    color = vec4(ganColor.xyz, ganColor.w * _enable);
}

