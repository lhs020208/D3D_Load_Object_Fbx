
cbuffer cbFrameworkInfo : register(b0)
{
	float 		gfCurrentTime;
	float		gfElapsedTime;
	float2		gf2CursorPos;
};

cbuffer cbGameObjectInfo : register(b1)
{
	matrix		gmtxWorld : packoffset(c0);
	float3		gf3ObjectColor : packoffset(c4);
};

cbuffer cbCameraInfo : register(b2)
{
	matrix		gmtxView : packoffset(c0);
	matrix		gmtxProjection : packoffset(c4);
	float3		gf3CameraPosition : packoffset(c8);
};

cbuffer cbLightInfo : register(b3)
{
    float gfLightDirectionX;
    float gfLightDirectionY;
    float gfLightDirectionZ;
	
    float gf3LightColorX;
    float gf3LightColorY;
    float gf3LightColorZ;
}

static const uint MAX_BONES = 256;
cbuffer cbBones : register(b4)
{
    float4x4 gBoneTransforms[MAX_BONES];
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VS_INPUT
{
	float3		position : POSITION;
	float3		normal : NORMAL;
	float2		uv : TEXTURECOORD;
};

struct VS_INPUT_SKINNED
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    uint4 bi : BLENDINDICES;
    float4 bw : BLENDWEIGHT;
};

struct VS_OUTPUT
{
	float4		positionH : SV_POSITION;
	float3		positionW : POSITION;
	float3		normal : NORMAL0;
	float3		normalW : NORMAL1;
	float2		uv : TEXTURECOORD;
};

float4 VSPseudoLighting(float4 position : POSITION) : SV_POSITION
{
    return position;
}

VS_OUTPUT VSLighting(VS_INPUT input)
{
	VS_OUTPUT output;

	output.positionW = mul(float4(input.position, 1.0f), gmtxWorld).xyz;
	output.positionH = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);
	output.normalW = mul(float4(input.normal, 0.0f), gmtxWorld).xyz;
	output.normal = input.normal;
	output.uv = input.uv;

	return(output);
}

VS_OUTPUT VSLightingSkinned(VS_INPUT_SKINNED input)
{
	
    VS_OUTPUT output;
    output.positionW = mul(float4(input.position, 1.0f), gmtxWorld).xyz;
    output.positionH = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);
    output.normalW = mul(float4(input.normal, 0.0f), gmtxWorld).xyz;
    output.normal = input.normal;
    output.uv = float2(0.0f, 0.0f);
    return output;
}

static float3 gf3AmbientLightColor = float3(1.0f, 1.0f, 1.0f);
static float3 gf3AmbientSpecularColor = float3(1.0f, 1.0f, 1.0f);

static float3 gf3SpecularColor = float3(0.2f, 0.2f, 0.2f);

static float gfSpecular = 2.0f;
static float gfGlossiness = 0.8f;
static float gfSmoothness = 0.75f;
static float gfOneMinusReflectivity = 0.15f;

inline float Pow5(float x)
{
	return(x * x * x * x * x);
}

inline float3 FresnelTerm(float3 F0, float cosA)
{
	return((F0 + (1 - F0) * Pow5(1 - cosA)));
}

inline float3 FresnelLerp(float3 F0, float3 F90, half cosA)
{
	return(lerp(F0, F90, Pow5(1 - cosA)));
}

inline float PerceptualRoughnessToSpecPower(float fPerceptualRoughness)
{
	float m = fPerceptualRoughness * fPerceptualRoughness;
	float sq = max(1e-4f, m * m);
	float n = (2.0f / sq) - 2.0f;

	return(max(n, 1e-4f));
}

float DisneyDiffuse(float NdotV, float NdotL, float LdotH, float fPerceptualRoughness)
{
	float fd90 = 0.5f + 2.0f * LdotH * LdotH * fPerceptualRoughness;
	float fLightScatter = (1.0f + (fd90 - 1.0f) * Pow5(1.0f - NdotL));
	float fViewScatter = (1.0f + (fd90 - 1.0f) * Pow5(1.0f - NdotV));

	return(fLightScatter * fViewScatter);
}

// Smith-Schlick derived for Beckmann
inline float SmithBeckmannVisibilityTerm(float NdotL, float NdotV, float fRoughness)
{
	float k = fRoughness * 0.797884560802865f; //0.797884560802865 = sqrt(2.0f / Pi)

	return(1.0f / ((NdotL * (1 - k) + k) * (NdotV * (1 - k) + k) + 1e-5f)); 
}

inline float NDFBlinnPhongNormalizedTerm(float NdotH, float fRoughnessToSpecPower)
{
	float fNormTerm = (fRoughnessToSpecPower + 2.0f) * (0.5f / 3.14159f);
	float fSpecTerm = pow(NdotH, fRoughnessToSpecPower);

	return(fNormTerm * fSpecTerm);
}

float4 PSLighting(VS_OUTPUT input) : SV_TARGET
{
    float3 gfLightDirection = float3(gfLightDirectionX, gfLightDirectionY, gfLightDirectionZ);
    float3 gf3LightColor = float3(gf3LightColorX, gf3LightColorY, gf3LightColorZ);
	
	// Normalize normal
    float3 N = normalize(input.normalW);

    // 조명 방향(단위벡터, -붙이면 '빛이 오는 방향')
    float3 L = normalize(-gfLightDirection);

    // 카메라 방향
    float3 V = normalize(gf3CameraPosition - input.positionW);

    // Half vector (Blinn-Phong)
    float3 H = normalize(L + V);

    // 내적 계산
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));

    // 앰비언트 항
    float3 ambient = gf3AmbientLightColor * gf3ObjectColor;

    // 디퓨즈 항
    float3 diffuse = gf3LightColor * gf3ObjectColor * NdotL;

    // 스페큘러 항
    float shininess = 4.0f; // 하이라이트 강도 (임의 값, 조정 가능)
    float3 specular = gf3SpecularColor * pow(NdotH, shininess);
	
    // 합산
    float3 finalColor = ambient + diffuse + specular;
	
     // 텍스처 샘플링 추가
    float4 texColor = gTexture.Sample(gSampler, input.uv);

    // 텍스처와 조명 결과 합산
    return float4(finalColor, 1.0f) * texColor;

}