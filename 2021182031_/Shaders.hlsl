#define DEBUG_FORCE_IDENTITY_BONES 0

cbuffer cbFrameworkInfo : register(b0)
{
    float gfCurrentTime;
    float gfElapsedTime;
    float2 gf2CursorPos;
};

cbuffer cbGameObjectInfo : register(b1)
{
    matrix gmtxWorld : packoffset(c0);
    float3 gf3ObjectColor : packoffset(c4);
};

cbuffer cbCameraInfo : register(b2)
{
    matrix gmtxView : packoffset(c0);
    matrix gmtxProjection : packoffset(c4);
    float3 gf3CameraPosition : packoffset(c8);
};

cbuffer cbLightInfo : register(b3)
{
    float gfLightDirectionX;
    float gfLightDirectionY;
    float gfLightDirectionZ;

    float gf3LightColorX;
    float gf3LightColorY;
    float gf3LightColorZ;
}; // 세미콜론 추가

static const uint MAX_BONES = 256;
cbuffer cbBones : register(b4)
{
    float4x4 gBoneTransforms[MAX_BONES];
};

Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXTURECOORD;
};

struct VS_INPUT_SKINNED
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXTURECOORD;
    uint4 bi : BLENDINDICES;
    float4 bw : BLENDWEIGHT;
};

struct VS_OUTPUT
{
    float4 positionH : SV_POSITION;
    float3 positionW : POSITION;
    float3 normal : NORMAL0;
    float3 normalW : NORMAL1;
    float2 uv : TEXCOORD0;
};

float4 VSPseudoLighting(float4 position : POSITION) : SV_POSITION
{
    return position;
}

VS_OUTPUT VSLighting(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 posW = mul(float4(input.position, 1.0f), gmtxWorld);
    output.positionW = posW.xyz;
    output.positionH = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);

    output.normalW = mul(float4(input.normal, 0.0f), gmtxWorld).xyz;
    output.normal = input.normal;

    output.uv = input.uv;

    return output;
}

VS_OUTPUT VSLightingSkinned(VS_INPUT_SKINNED input)
{
    VS_OUTPUT output;

    // ---------------------------------------------------
    // 0) 디버그 플래그: 본 무시하고 비스키닝 경로 사용
    // ---------------------------------------------------
#if DEBUG_FORCE_IDENTITY_BONES
    float4 localPos    = float4(input.position, 1.0f);
    float4 localNormal = float4(input.normal, 0.0f);
#else
    // 1) 원본 로컬 위치/노멀
    float4 posL = float4(input.position, 1.0f);
    float4 nrmL = float4(input.normal, 0.0f);

    // 2) 스키닝 누적
    float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        uint b = input.bi[i];
        float w = input.bw[i];

        if (w <= 0.0001f)
            continue;
        if (b >= MAX_BONES)
            continue; // 안전한 범위 체크

        float4x4 B = gBoneTransforms[b];

        skinnedPos += mul(posL, B) * w;
        skinnedNormal += mul(nrmL, B).xyz * w;
    }

    // 모든 weight가 0이거나 유효 본이 없을 때 fallback
    if (all(skinnedPos == 0.0f))
    {
        skinnedPos = posL;
        skinnedNormal = input.normal;
    }

    skinnedNormal = normalize(skinnedNormal);

    float4 localPos = skinnedPos;
    float4 localNormal = float4(skinnedNormal, 0.0f);
#endif

    // 3) 월드/뷰/프로젝션 변환 (비스키닝 VS와 동일)
    float4 posW = mul(localPos, gmtxWorld);
    output.positionW = posW.xyz;
    output.positionH = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);

    float3 normalW = mul(localNormal, gmtxWorld).xyz;
    normalW = normalize(normalW); // 월드 노멀 정규화
    output.normalW = normalW;
    output.normal = normalW; // 필요하면 여기서 localNormal.xyz 써도 됨

    output.uv = input.uv;

    return output;
}

// =======================
// 조명/BRDF 유틸 함수들
// =======================

static float3 gf3AmbientLightColor = float3(1.0f, 1.0f, 1.0f);
static float3 gf3AmbientSpecularColor = float3(1.0f, 1.0f, 1.0f);

static float3 gf3SpecularColor = float3(0.2f, 0.2f, 0.2f);

static float gfSpecular = 2.0f;
static float gfGlossiness = 0.8f;
static float gfSmoothness = 0.75f;
static float gfOneMinusReflectivity = 0.15f;

inline float Pow5(float x)
{
    return (x * x * x * x * x);
}

inline float3 FresnelTerm(float3 F0, float cosA)
{
    return (F0 + (1 - F0) * Pow5(1 - cosA));
}

inline float3 FresnelLerp(float3 F0, float3 F90, half cosA)
{
    return (lerp(F0, F90, Pow5(1 - cosA)));
}

inline float PerceptualRoughnessToSpecPower(float fPerceptualRoughness)
{
    float m = fPerceptualRoughness * fPerceptualRoughness;
    float sq = max(1e-4f, m * m);
    float n = (2.0f / sq) - 2.0f;

    return (max(n, 1e-4f));
}

float DisneyDiffuse(float NdotV, float NdotL, float LdotH, float fPerceptualRoughness)
{
    float fd90 = 0.5f + 2.0f * LdotH * LdotH * fPerceptualRoughness;
    float fLightScatter = (1.0f + (fd90 - 1.0f) * Pow5(1.0f - NdotL));
    float fViewScatter = (1.0f + (fd90 - 1.0f) * Pow5(1.0f - NdotV));

    return (fLightScatter * fViewScatter);
}

// Smith-Schlick derived for Beckmann
inline float SmithBeckmannVisibilityTerm(float NdotL, float NdotV, float fRoughness)
{
    float k = fRoughness * 0.797884560802865f; // sqrt(2.0f / Pi)

    return (1.0f / ((NdotL * (1 - k) + k) * (NdotV * (1 - k) + k) + 1e-5f));
}

inline float NDFBlinnPhongNormalizedTerm(float NdotH, float fRoughnessToSpecPower)
{
    float fNormTerm = (fRoughnessToSpecPower + 2.0f) * (0.5f / 3.14159f);
    float fSpecTerm = pow(NdotH, fRoughnessToSpecPower);

    return (fNormTerm * fSpecTerm);
}

float4 PSLighting(VS_OUTPUT input) : SV_TARGET
{
    // 라이트/색 조합
    float3 lightDir = normalize(float3(gfLightDirectionX, gfLightDirectionY, gfLightDirectionZ));
    float3 lightColor = float3(gf3LightColorX, gf3LightColorY, gf3LightColorZ);

    // 월드 공간에서의 법선/벡터들
    float3 N = normalize(input.normalW);
    float3 L = normalize(-lightDir);
    float3 V = normalize(gf3CameraPosition - input.positionW);
    float3 H = normalize(L + V);

    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));

    // 단순 조명 모델 (ambient + diffuse + specular)
    float3 ambient = gf3AmbientLightColor * gf3ObjectColor;
    float3 diffuse = lightColor * gf3ObjectColor * NdotL;
    float3 specular = gf3SpecularColor * pow(NdotH, 4.0f);

    float3 finalLight = ambient + diffuse + specular;

    // 텍스처 샘플링
    float4 texColor = gDiffuseMap.Sample(gSampler, input.uv);

    return float4(texColor.rgb * finalLight, texColor.a);
}
