cbuffer cbPerObject : register(b0)
{
    float4x4 previousViewProjection; 
    float4x4 inverseViewProjection;
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

float2 PS(VertexOut vertexOut) : SV_Target
{
    float z = g_depth.Sample(g_sampler, vertexOut.uv).r;
    float4 viewportPos = float4(vertexOut.uv.x * 2 - 1, 
                                (1 - vertexOut.uv.y) * 2 - 1,
                                z, 1); 

    float4 worldPos = mul(viewportPos, inverseViewProjection);
    worldPos /= worldPos.w;

    float4 currentPos = viewportPos;
    float4 previousPos = mul(worldPos, previousViewProjection);
    previousPos /= previousPos.w;
    float4 velocity = (currentPos - previousPos) / 3.0;
    return velocity.xy;
}


