cbuffer cbPerObject : register(b0)
{
	float4x4 previousMatrix; 
    float4x4 currentMatrix; 
};

struct VertexIn
{
	float3 position : POSITION;
    float2 uv : TEXCOORD;
};

struct VertexOut
{
	float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VertexOut VS(VertexIn vertexIn)
{
	VertexOut vertexOut;
	vertexOut.position = float4(vertexIn.position, 1.0f);
    vertexOut.uv = vertexIn.uv;    
    return vertexOut;
}

Texture2D g_depth : register(t0);
SamplerState g_sampler : register(s0);

float4 PS(VertexOut vertexOut) : SV_Target
{
    float r = g_depth.Sample(g_sampler, vertexOut.uv).r;
    return float4(r, 0.0, 0.0, 0.0);
}


