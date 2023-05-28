#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct buffer_t
{
    float enable;
};

struct main0_out
{
    float4 color [[color(0)]];
};

struct main0_in
{
    float2 uv;
};

fragment main0_out main0(main0_in in [[stage_in]], constant buffer_t& buffer, texture2d<float> ganTexture [[texture(0)]], texture2d<float> maskTexture [[texture(1)]], sampler ganTextureSmplr [[sampler(0)]], sampler maskTextureSmplr [[sampler(1)]])
{
    main0_out out = {};
    float4 ganColor = ganTexture.sample(ganTextureSmplr, in.uv);
    float4 maskColor = maskTexture.sample(maskTextureSmplr, in.uv);
    out.color = float4(ganColor.xyz, (ganColor.w * maskColor.x) * buffer.enable);
    return out;
}

