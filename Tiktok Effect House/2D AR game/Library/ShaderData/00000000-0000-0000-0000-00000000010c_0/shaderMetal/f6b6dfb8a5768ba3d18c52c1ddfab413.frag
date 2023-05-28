#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct buffer_t
{
    float2 samplerEnvSize;
    float roughnessScale;
};

struct main0_out
{
    float4 fragColor [[color(0)]];
};

struct main0_in
{
    float2 varScreenTexturePos;
};

static inline __attribute__((always_inline))
float3 calcDirFromPanoramicTexCoords(thread const float2& uv)
{
    float a = 6.283185482025146484375 * uv.x;
    float b = 3.1415927410125732421875 * uv.y;
    float x = sin(a) * sin(b);
    float y = cos(b);
    float z = cos(a) * sin(b);
    return float3(z, y, x);
}

static inline __attribute__((always_inline))
float radicalInverse(thread int& n)
{
    float val = 0.0;
    float invBase = 0.5;
    float invBi = invBase;
    while (n > 0)
    {
        int d_i = n - ((n / 2) * 2);
        val += (float(d_i) * invBi);
        n /= 2;
        invBi *= 0.5;
    }
    return val;
}

static inline __attribute__((always_inline))
float2 hammersley(thread const int& i, thread const int& N)
{
    int param = i;
    float _107 = radicalInverse(param);
    return float2(float(i) / float(N), _107);
}

static inline __attribute__((always_inline))
float3 importanceSampleGGX(thread const float2& Xi, thread const float& roughness, thread const float3& N)
{
    float a = roughness * roughness;
    float Phi = 6.283185482025146484375 * Xi.x;
    float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (((a * a) - 1.0) * Xi.y)));
    float SinTheta = sqrt(1.0 - (CosTheta * CosTheta));
    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    float3 UpVector = float3(bool3(abs(N.z) < 0.999000012874603271484375));
    float3 TangentX = normalize(cross(UpVector, N));
    float3 TangentY = cross(N, TangentX);
    return normalize(((TangentX * H.x) + (TangentY * H.y)) + (N * H.z));
}

static inline __attribute__((always_inline))
float DistributionGGX(thread const float3& N, thread const float3& H, thread const float& roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = fast::max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0)) + 1.0;
    denom = (3.1415927410125732421875 * denom) * denom;
    return nom / denom;
}

static inline __attribute__((always_inline))
float _atan2(thread const float& x, thread const float& y)
{
    float signx = (x < 0.0) ? (-1.0) : 1.0;
    return signx * acos(fast::clamp(y / length(float2(x, y)), -1.0, 1.0));
}

static inline __attribute__((always_inline))
float2 calcPanoramicTexCoordsFromDir(thread const float3& reflDir)
{
    float param = reflDir.x;
    float param_1 = -reflDir.z;
    float2 uv;
    uv.x = _atan2(param, param_1) - 1.57079637050628662109375;
    uv.y = acos(reflDir.y);
    uv /= float2(6.283185482025146484375, 3.1415927410125732421875);
    uv.y = 1.0 - uv.y;
    return uv;
}

static inline __attribute__((always_inline))
float4 prefilterEnvMap(thread const float3& R, thread const float& roughness, thread const float& bias0, constant float2& samplerEnvSize, texture2d<float> samplerEnv, sampler samplerEnvSmplr)
{
    float3 N = R;
    float3 V = R;
    float4 color = float4(0.0);
    float totalWeight = 0.0;
    float _422;
    for (int i = 0; i < 4096; i++)
    {
        int param = i;
        int param_1 = 4096;
        float2 Xi = hammersley(param, param_1);
        float2 param_2 = Xi;
        float param_3 = roughness;
        float3 param_4 = N;
        float3 H = importanceSampleGGX(param_2, param_3, param_4);
        float3 L = (H * (2.0 * dot(V, H))) - V;
        float dotNL = fast::clamp(dot(N, L), 0.0, 1.0);
        if (dotNL > 0.0)
        {
            float3 param_5 = N;
            float3 param_6 = H;
            float param_7 = roughness;
            float D = DistributionGGX(param_5, param_6, param_7);
            float NdotH = fast::max(dot(N, H), 0.0);
            float HdotV = fast::max(dot(H, V), 0.0);
            float pdf = ((D * NdotH) / (4.0 * HdotV)) + 9.9999997473787516355514526367188e-05;
            float resulotion = samplerEnvSize.x * samplerEnvSize.y;
            float saTexel = 12.56637096405029296875 / resulotion;
            float saSample = 1.0 / ((4096.0 * pdf) + 9.9999997473787516355514526367188e-05);
            if (roughness == 0.0)
            {
                _422 = 0.0;
            }
            else
            {
                _422 = 0.5 * log2(saSample / saTexel);
            }
            float mipLevel = _422;
            float3 param_8 = L;
            color += (samplerEnv.sample(samplerEnvSmplr, calcPanoramicTexCoordsFromDir(param_8), level(mipLevel + bias0)) * dotNL);
            totalWeight += dotNL;
        }
    }
    return color / float4(totalWeight);
}

static inline __attribute__((always_inline))
float4 encodeRGBD(thread const float3& rgb)
{
    float a = 1.0 / fast::max(1.0, fast::max(rgb.x, fast::max(rgb.y, rgb.z)));
    return float4(rgb * a, a);
}

static inline __attribute__((always_inline))
float4 doEnv(thread const float2& uv, thread const float& Roughness, thread const int& uniMipToRender, constant float2& samplerEnvSize, texture2d<float> samplerEnv, sampler samplerEnvSmplr, constant float& roughnessScale)
{
    float2 param = uv;
    float3 N = calcDirFromPanoramicTexCoords(param);
    float3 param_1 = N;
    float param_2 = Roughness;
    float param_3 = Roughness * roughnessScale;
    float3 param_4 = prefilterEnvMap(param_1, param_2, param_3, samplerEnvSize, samplerEnv, samplerEnvSmplr).xyz;
    return encodeRGBD(param_4);
}

fragment main0_out main0(main0_in in [[stage_in]], constant buffer_t& buffer, texture2d<float> samplerEnv [[texture(0)]], sampler samplerEnvSmplr [[sampler(0)]])
{
    main0_out out = {};
    float2 uv = in.varScreenTexturePos;
    uv.y = 1.0 - uv.y;
    float2 param = uv;
    float param_1 = 1.0;
    int param_2 = 0;
    out.fragColor = doEnv(param, param_1, param_2, buffer.samplerEnvSize, samplerEnv, samplerEnvSmplr, buffer.roughnessScale);
    return out;
}

