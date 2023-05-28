#version 300 es
precision highp float;
precision highp int;

uniform vec2 samplerEnvSize;
uniform mediump sampler2D samplerEnv;
uniform float roughnessScale;
uniform vec2 uniTopMipRes;

in vec2 varScreenTexturePos;
layout(location = 0) out vec4 fragColor;

vec3 calcDirFromPanoramicTexCoords(vec2 uv)
{
    float a = 6.283185482025146484375 * uv.x;
    float b = 3.1415927410125732421875 * uv.y;
    float x = sin(a) * sin(b);
    float y = cos(b);
    float z = cos(a) * sin(b);
    return vec3(z, y, x);
}

float radicalInverse(inout mediump int n)
{
    float val = 0.0;
    float invBase = 0.5;
    float invBi = invBase;
    while (n > 0)
    {
        mediump int d_i = n - ((n / 2) * 2);
        val += (float(d_i) * invBi);
        n /= 2;
        invBi *= 0.5;
    }
    return val;
}

vec2 hammersley(mediump int i, mediump int N)
{
    mediump int param = i;
    float _107 = radicalInverse(param);
    return vec2(float(i) / float(N), _107);
}

vec3 importanceSampleGGX(vec2 Xi, float roughness, vec3 N)
{
    float a = roughness * roughness;
    float Phi = 6.283185482025146484375 * Xi.x;
    float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (((a * a) - 1.0) * Xi.y)));
    float SinTheta = sqrt(1.0 - (CosTheta * CosTheta));
    vec3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    vec3 UpVector = vec3(bvec3(abs(N.z) < 0.999000012874603271484375));
    vec3 TangentX = normalize(cross(UpVector, N));
    vec3 TangentY = cross(N, TangentX);
    return normalize(((TangentX * H.x) + (TangentY * H.y)) + (N * H.z));
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0)) + 1.0;
    denom = (3.1415927410125732421875 * denom) * denom;
    return nom / denom;
}

float _atan2(float x, float y)
{
    float signx = (x < 0.0) ? (-1.0) : 1.0;
    return signx * acos(clamp(y / length(vec2(x, y)), -1.0, 1.0));
}

vec2 calcPanoramicTexCoordsFromDir(vec3 reflDir)
{
    float param = reflDir.x;
    float param_1 = -reflDir.z;
    vec2 uv;
    uv.x = _atan2(param, param_1) - 1.57079637050628662109375;
    uv.y = acos(reflDir.y);
    uv /= vec2(6.283185482025146484375, 3.1415927410125732421875);
    uv.y = 1.0 - uv.y;
    return uv;
}

vec4 prefilterEnvMap(vec3 R, float roughness, float bias)
{
    vec3 N = R;
    vec3 V = R;
    vec4 color = vec4(0.0);
    float totalWeight = 0.0;
    float _422;
    for (mediump int i = 0; i < 4096; i++)
    {
        mediump int param = i;
        mediump int param_1 = 4096;
        vec2 Xi = hammersley(param, param_1);
        vec2 param_2 = Xi;
        float param_3 = roughness;
        vec3 param_4 = N;
        vec3 H = importanceSampleGGX(param_2, param_3, param_4);
        vec3 L = (H * (2.0 * dot(V, H))) - V;
        float dotNL = clamp(dot(N, L), 0.0, 1.0);
        if (dotNL > 0.0)
        {
            vec3 param_5 = N;
            vec3 param_6 = H;
            float param_7 = roughness;
            float D = DistributionGGX(param_5, param_6, param_7);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
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
            vec3 param_8 = L;
            color += (textureLod(samplerEnv, calcPanoramicTexCoordsFromDir(param_8), mipLevel + bias) * dotNL);
            totalWeight += dotNL;
        }
    }
    return color / vec4(totalWeight);
}

vec4 encodeRGBD(vec3 rgb)
{
    float a = 1.0 / max(1.0, max(rgb.x, max(rgb.y, rgb.z)));
    return vec4(rgb * a, a);
}

vec4 doEnv(vec2 uv, float Roughness, mediump int uniMipToRender)
{
    vec2 param = uv;
    vec3 N = calcDirFromPanoramicTexCoords(param);
    vec3 param_1 = N;
    float param_2 = Roughness;
    float param_3 = Roughness * roughnessScale;
    vec3 param_4 = prefilterEnvMap(param_1, param_2, param_3).xyz;
    return encodeRGBD(param_4);
}

void main()
{
    vec2 uv = varScreenTexturePos;
    uv.y = 1.0 - uv.y;
    vec2 param = uv;
    float param_1 = 1.0;
    mediump int param_2 = 0;
    fragColor = doEnv(param, param_1, param_2);
}

