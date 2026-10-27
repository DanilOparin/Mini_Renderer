#include "../Common.hlsli"

struct VS_OUTPUT
{
    float4 posCS : SV_Position;
    float2 uvs : TEXCOORD;
};







Texture2D gAlbedoBuffer : register(t6);

SamplerState sPointClamp : register(s0,space0);
SamplerState sLinearClamp : register(s1,space0);
SamplerComparisonState sShadowCompare : register(s0, space1);


float4 main(VS_OUTPUT input) : SV_TARGET
{
    
    float4 albedo = gAlbedoBuffer.Sample(sPointClamp, input.uvs);
    
       
    
    
  
   

   
    
    
    
    return albedo;
    
}