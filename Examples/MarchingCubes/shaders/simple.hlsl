cbuffer cbPerObject : register(b0)
{
	float4x4 worldViewProj; 
};

struct VertexIn
{
	float3 position : POSITION;
};

struct VertexOut
{
	float4 position : SV_POSITION;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

VertexOut VS(VertexIn vertexIn)
{
	VertexOut vertexOut;
	vertexOut.position = mul(float4(vertexIn.position, 1.0f), worldViewProj);
    return vertexOut;
}

float4 PS(VertexOut vertexOut) : SV_Target
{
    return 1.0;
}


