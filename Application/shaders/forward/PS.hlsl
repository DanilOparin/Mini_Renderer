struct VS_OUTPUT
{
	
    float4 positionCS : SV_POSITION;
    float3 positionWS : POSITION0;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
	
};

struct PS_OUTPUT
{
    float4 color : SV_Target0; //last component could be objectidx (w)
   
};


struct MaterialData
{
    float4 diffuseAlbedo;
    
};

ConstantBuffer<MaterialData> gMaterialData : register(b2);

SamplerState sLinearClamp : register(s1, space0);

Texture2D BrickTexture : register(t5);

PS_OUTPUT main(VS_OUTPUT input)
{
    PS_OUTPUT output;
    
   
    
    
    output.color =
BrickTexture.Sample(
sLinearClamp,
input.uv);
    
 
    return output;
}