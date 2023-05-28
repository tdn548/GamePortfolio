#version 300 es
precision highp float;
precision highp int;

uniform mediump vec2 samplerEnvSize;
uniform mediump sampler2D samplerEnv;
uniform mediump float roughnessScale;
uniform mediump float perceptualRoughness;

in vec2 varScreenTexturePos;
layout(location = 0) out vec4 fragColor;

mediump vec3 calcDirFromPanoramicTexCoords(mediump vec2 uv)
{
    mediump float a = 6.283185482025146484375 * uv.x;
    mediump float b = 3.1415927410125732421875 * uv.y;
    mediump float x = sin(a) * sin(b);
    mediump float y = cos(b);
    mediump float z = cos(a) * sin(b);
    return vec3(z, y, x);
}

mediump float radicalInverse(inout mediump int n)
{
    mediump float val = 0.0;
    mediump float invBase = 0.5;
    mediump float invBi = invBase;
    while (n > 0)
    {
        mediump int d_i = n - ((n / 2) * 2);
        val += (float(d_i) * invBi);
        n /= 2;
        invBi *= 0.5;
    }
    return val;
}

mediump vec2 hammersley(mediump int i, mediump int N)
{
    mediump int param = i;
    mediump float _106 = radicalInverse(param);
    return vec2(float(i) / float(N), _106);
}

mediump vec3 importanceSampleGGX(mediump vec2 Xi, mediump float roughness, mediump vec3 N)
{
    mediump float a = roughness * roughness;
    mediump float Phi = 6.283185482025146484375 * Xi.x;
    mediump float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (((a * a) - 1.0) * Xi.y)));
    mediump float SinTheta = sqrt(1.0 - (CosTheta * CosTheta));
    mediump vec3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    mediump vec3 UpVector = vec3(0.0, 0.0, 1.0);
    mediump vec3 TangentX = normalize(cross(UpVector, N));
    mediump vec3 TangentY = cross(N, TangentX);
    return normalize(((TangentX * H.x) + (TangentY * H.y)) + (N * H.z));
}

mediump float DistributionGGX(mediump vec3 N, mediump vec3 H, mediump float roughness)
{
    mediump float a = roughness * roughness;
    mediump float a2 = a * a;
    mediump float NdotH = max(dot(N, H), 0.0);
    mediump float NdotH2 = NdotH * NdotH;
    mediump float nom = a2;
    mediump float denom = (NdotH2 * (a2 - 1.0)) + 1.0;
    denom = (3.1415927410125732421875 * denom) * denom;
    return nom / denom;
}

mediump float _atan2(mediump float x, mediump float y)
{
    mediump float signx = (x < 0.0) ? (-1.0) : 1.0;
    return signx * acos(clamp(y / length(vec2(x, y)), -1.0, 1.0));
}

mediump vec2 calcPanoramicTexCoordsFromDir(mediump vec3 reflDir)
{
    mediump float param = reflDir.x;
    mediump float param_1 = -reflDir.z;
    mediump vec2 uv;
    uv.x = _atan2(param, param_1) - 1.57079637050628662109375;
    uv.y = acos(reflDir.y);
    uv /= vec2(6.283185482025146484375, 3.1415927410125732421875);
    uv.y = 1.0 - uv.y;
    return uv;
}

mediump vec4 prefilterEnvMap(mediump vec3 R, mediump float roughness, mediump float bias)
{
    mediump vec3 N = R;
    mediump vec3 V = R;
    mediump vec4 color = vec4(0.0);
    mediump float totalWeight = 0.0;
    mediump float _412;
    for (mediump int i = 0; i < 512; i++)
    {
        mediump int param = i;
        mediump int param_1 = 512;
        mediump vec2 Xi = hammersley(param, param_1);
        mediump vec2 param_2 = Xi;
        mediump float param_3 = roughness;
        mediump vec3 param_4 = N;
        mediump vec3 H = importanceSampleGGX(param_2, param_3, param_4);
        mediump vec3 L = (H * (2.0 * dot(V, H))) - V;
        mediump float dotNL = clamp(dot(N, L), 0.0, 1.0);
        if (dotNL > 0.0)
        {
            mediump vec3 param_5 = N;
            mediump vec3 param_6 = H;
            mediump float param_7 = roughness;
            mediump float D = DistributionGGX(param_5, param_6, param_7);
            mediump float NdotH = max(dot(N, H), 0.0);
            mediump float HdotV = max(dot(H, V), 0.0);
            mediump float pdf = ((D * NdotH) / (4.0 * HdotV)) + 9.9999997473787516355514526367188e-05;
            mediump float resulotion = samplerEnvSize.x * samplerEnvSize.y;
            mediump float saTexel = 12.56637096405029296875 / resulotion;
            mediump float saSample = 1.0 / ((512.0 * pdf) + 9.9999997473787516355514526367188e-05);
            if (roughness == 0.0)
            {
                _412 = 0.0;
            }
            else
            {
                _412 = 0.5 * log2(saSample / saTexel);
            }
            mediump float mipLevel = _412;
            mediump vec3 param_8 = L;
            color += (textureLod(samplerEnv, calcPanoramicTexCoordsFromDir(param_8), mipLevel + bias) * dotNL);
            totalWeight += dotNL;
            color = min(color, vec4(10000.0));
        }
    }
    return color / vec4(totalWeight);
}

mediump vec4 encodeRGBD(mediump vec3 rgb)
{
    mediump float a = 1.0 / max(1.0, max(rgb.x, max(rgb.y, rgb.z)));
    return vec4(rgb * a, a);
}

mediump vec4 doEnv(mediump vec2 uv, mediump float roughness)
{
    mediump vec2 param = uv;
    mediump vec3 N = calcDirFromPanoramicTexCoords(param);
    mediump vec3 param_1 = N;
    mediump float param_2 = roughness;
    mediump float param_3 = roughnessScale;
    mediump vec3 param_4 = prefilterEnvMap(param_1, param_2, param_3).xyz;
    return encodeRGBD(param_4);
}

void main()
{
    mediump vec2 uv = varScreenTexturePos;
    uv.y = 1.0 - uv.y;
    mediump vec2 param = uv;
    mediump float param_1 = perceptualRoughness;
    fragColor = doEnv(param, param_1);
}

