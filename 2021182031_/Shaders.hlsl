cbuffer cbFrameworkInfo : register(b0)
{
    float gfCurrentTime;
    float gfElapsedTime;
    float2 gf2CursorPos;
};

cbuffer cbGameObjectInfo : register(b1)
{
    row_major float4x4 gmtxWorld;
    float3 gf3ObjectColor;
    float _pad_gobj0;
};

cbuffer cbCameraInfo : register(b2)
{
    row_major float4x4 gmtxView;
    row_major float4x4 gmtxProjection;
    float3 gf3CameraPosition;
    float _pad_cam0;
};

cbuffer cbLightInfo : register(b3)
{
    float gfLightDirectionX;
    float gfLightDirectionY;
    float gfLightDirectionZ;
    float _pad_light0;

    float gf3LightColorX;
    float gf3LightColorY;
    float gf3LightColorZ;
    float _pad_light1;
};

static const uint MAX_BONES = 256;
cbuffer cbBones : register(b4)
{
    row_major float4x4 gBoneTransforms[MAX_BONES];
};

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
    uint4 bi : BLENDINDICES;
    float4 bw : BLENDWEIGHT;
};

struct VS_OUTPUT
{
    float4 positionH : SV_POSITION;
    float3 positionW : POSITION;
    float3 normal : NORMAL0;
    float3 normalW : NORMAL1;
    float2 uv : TEXTURECOORD;
};

float4x4 GetVP()
{
    return mul(gmtxView, gmtxProjection);
}

VS_OUTPUT VSPseudoLighting(VS_INPUT input)
{
    VS_OUTPUT o;
    float4 posW = mul(float4(input.position, 1.0f), gmtxWorld);
    float3 nW = mul((float3x3) gmtxWorld, input.normal);

    o.positionH = mul(posW, GetVP());
    o.positionW = posW.xyz;
    o.normalW = normalize(nW);
    o.normal = input.normal;
    o.uv = input.uv;
    return o;
}

VS_OUTPUT VSLighting(VS_INPUT input)
{
    return VSPseudoLighting(input);
}

VS_OUTPUT VSLightingSkinned(VS_INPUT_SKINNED input)
{
    VS_OUTPUT o;

    float4x4 skinM = 0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float w = input.bw[i];
        if (w > 0)
        {
            uint idx = input.bi[i];
            skinM += gBoneTransforms[idx] * w;
        }
    }

    float4 posSkinned = mul(float4(input.position, 1.0f), skinM);
    float3 nSkinned = mul((float3x3) skinM, input.normal);

    float4 posW = mul(posSkinned, gmtxWorld);
    float3 nW = mul((float3x3) gmtxWorld, nSkinned);

    o.positionH = mul(posW, GetVP());
    o.positionW = posW.xyz;
    o.normalW = normalize(nW);
    o.normal = input.normal;
    o.uv = float2(0.0f, 0.0f);
    return o;
}

static float3 gf3AmbientLightColor = float3(1.0f, 1.0f, 1.0f);
static float3 gf3SpecularColor = float3(0.2f, 0.2f, 0.2f);

float4 PSLighting(VS_OUTPUT input) : SV_TARGET
{
    float3 Ldir = normalize(float3(gfLightDirectionX, gfLightDirectionY, gfLightDirectionZ));
    float3 Lcol = float3(gf3LightColorX, gf3LightColorY, gf3LightColorZ);

    float3 N = normalize(input.normalW);
    float3 V = normalize(gf3CameraPosition - input.positionW);
    float3 L = normalize(-Ldir);
    float3 H = normalize(L + V);

    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));

    float3 ambient = gf3AmbientLightColor * gf3ObjectColor;
    float3 diffuse = gf3ObjectColor * Lcol * NdotL;
    float3 specular = gf3SpecularColor * pow(NdotH, 16.0f);

    return float4(ambient + diffuse + specular, 1.0f);
}
