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

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 PS(VertexOut vertexOut) : SV_Target
{
    return g_texture.Sample(g_sampler, vertexOut.uv);
}