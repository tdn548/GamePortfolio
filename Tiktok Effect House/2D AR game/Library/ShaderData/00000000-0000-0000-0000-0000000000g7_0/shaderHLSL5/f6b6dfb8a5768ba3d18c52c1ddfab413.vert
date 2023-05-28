row_major uniform float4x4 mvpMat;

static float4 gl_Position;
static float3 attPosition;
static float2 uv;
static float2 attTexcoord0;

struct SPIRV_Cross_Input
{
    float3 attPosition : attPosition;
    float2 attTexcoord0 : attTexcoord0;
};

struct SPIRV_Cross_Output
{
    float2 uv : uv;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    float4 pos = float4(attPosition.xy, 0.0f, 1.0f);
    gl_Position = mul(pos, mvpMat);
    uv = attTexcoord0;
    gl_Position.y = -gl_Position.y;
    gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    attPosition = stage_input.attPosition;
    attTexcoord0 = stage_input.attTexcoord0;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.uv = uv;
    return stage_output;
}
