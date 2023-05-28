Texture2D<float4> ganTexture : register(t0);
SamplerState _ganTexture_sampler : register(s0);
Texture2D<float4> maskTexture : register(t1);
SamplerState _maskTexture_sampler : register(s1);
uniform float enable;

static float2 uv;
static float4 color;

struct SPIRV_Cross_Input
{
    float2 uv : uv;
};

struct SPIRV_Cross_Output
{
    float4 color : SV_Target0;
};

void frag_main()
{
    float4 ganColor = ganTexture.Sample(_ganTexture_sampler, uv);
    float4 maskColor = maskTexture.Sample(_maskTexture_sampler, uv);
    color = float4(ganColor.xyz, ganColor.w * enable);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    uv = stage_input.uv;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.color = color;
    return stage_output;
}
