#include "../../Common.hlsli"

struct VS_OUTPUT
{
    float4 posCS : SV_Position;
    float2 uvs : TEXCOORD;
};


struct RootConstants
{
    unsigned int width;
    unsigned int height;
};


ConstantBuffer<RootConstants> gRootConstants : register(b0, space1);

Texture2D gPositionBuffer : register(t0);
Texture2D gNormalBuffer : register(t1);
Texture2D gAlbedoBuffer : register(t2);
Texture2D gGDepthBuffer : register(t3);

Texture2D gShadowMapBuffer : register(t4);



SamplerState sPointClamp : register(s0,space0);
SamplerState sLinearClamp : register(s1,space0);
SamplerComparisonState sShadowCompare : register(s0, space1);


float4 main(VS_OUTPUT input) : SV_TARGET
{
    
    float4 albedo = gAlbedoBuffer.Sample(sPointClamp, input.uvs);
    
       
    
    
  
   

   
    
    
    
    return albedo;
    
}