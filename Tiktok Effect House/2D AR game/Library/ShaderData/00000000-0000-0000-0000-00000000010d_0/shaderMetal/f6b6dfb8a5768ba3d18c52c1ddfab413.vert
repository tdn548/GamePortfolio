#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct buffer_t
{
    float uniSpanWidth;
    float uniSpanHeight;
    float uniSliceOffsetWidth;
    float uniSliceOffsetHeight;
    float uniRtWidth;
    float uniRtHeight;
};

struct main0_out
{
    float2 varScreenTexturePos;
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 attPosition [[attribute(0)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant buffer_t& buffer)
{
    main0_out out = {};
    out.varScreenTexturePos = ((((in.attPosition.xy + float2(1.0)) / float2(2.0)) * float2(buffer.uniSpanWidth, buffer.uniSpanHeight)) + float2(buffer.uniSliceOffsetWidth, buffer.uniSliceOffsetHeight)) / float2(buffer.uniRtWidth, buffer.uniRtHeight);
    out.gl_Position = float4(in.attPosition, 1.0);
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    return out;
}

