cbuffer cbPerObject : register(b0)
{
	float4x4 worldViewProj; 
};

struct VertexIn
{
	float3 position : POSITION;
    float4 color : COLOR;
};

struct VertexOut
{
	float4 position : SV_POSITION;
    float4 color : COLOR;
};

VertexOut VS(VertexIn vertexIn)
{
	VertexOut vertexOut;
	vertexOut.position = mul(float4(vertexIn.position, 1.0f), worldViewProj);
    vertexOut.color	 = vertexIn.color;    
    return vertexOut;
}

float4 PS(VertexOut vertexOut) : SV_Target
{
    return vertexOut.color;
}


