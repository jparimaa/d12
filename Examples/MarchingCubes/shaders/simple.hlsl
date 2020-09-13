cbuffer cbPerObject : register(b0)
{
	float4x4 world;
	float4x4 worldViewProj; 
};

struct VertexIn
{
	float4 position : POSITION;
	float4 normal : NORMAL;
};

struct VertexOut
{
	float4 position : SV_POSITION;
	float4 positionWorld : POSITIONT;
	float4 normal : NORMAL;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

VertexOut VS(VertexIn vertexIn)
{
	VertexOut vertexOut;
	vertexOut.position = mul(float4(vertexIn.position.xyz, 1.0f), worldViewProj);
	vertexOut.positionWorld = mul(float4(vertexIn.position.xyz, 1.0f), world);
	vertexOut.normal = mul(float4(vertexIn.normal.xyz, 0.0f), world);
    return vertexOut;
}

float4 PS(VertexOut vertexOut) : SV_Target
{
	float3 L = float3(-1.0f, -1.0f, 1.0);
	float3 N = normalize(vertexOut.normal.xyz);
    float NdotL = max(0.0, dot(N, L));	
	return max(0.3f, NdotL) * float4(vertexOut.positionWorld.xyz / 30.0f, 1.0);
}


