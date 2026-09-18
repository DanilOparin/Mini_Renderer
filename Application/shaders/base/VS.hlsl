#include "../Common.hlsli"

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    
};


struct VS_OUTPUT
{
	
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
	
};


VS_OUTPUT main(VS_INPUT input)
{ 
    VS_OUTPUT output;
    
    float3 worldPos = input.position;
    worldPos = mul(gObjectData.transform, float4(worldPos, 1.0f)).xyz;

    output.position = mul(gPassData.viewproj, float4(worldPos, 1.0f));
    output.normal = mul((float3x3)gObjectData.transform, input.normal);
    
    return output;
}
