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
	vertexOut.position = mul(float4(vertexIn.position, 1.0f), currentMatrix);
    vertexOut.uv = vertexIn.uv;    
    return vertexOut;
}

float4 PS(VertexOut vertexOut) : SV_Target
{
    return float4(1.0, 0.5, 0.0, 1.0);
}


