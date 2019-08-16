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

Texture2D g_objectRender : register(t0);
Texture2D g_motionVector : register(t1);

SamplerState g_sampler : register(s0);

VertexOut VS(VertexIn vertexIn)
{
	VertexOut vertexOut;
	vertexOut.position = float4(vertexIn.position, 1.0f);
    vertexOut.uv = vertexIn.uv;    
    return vertexOut;
}

float4 PS(VertexOut vertexOut) : SV_Target
{
    float2 texCoord = vertexOut.uv;
    float4 color = g_objectRender.Sample(g_sampler, texCoord);
    float2 velocity = g_motionVector.Sample(g_sampler, vertexOut.uv).xy;
    texCoord += velocity;
    for(int i = 1; i < 6; ++i, texCoord += velocity)
    {
        // Sample the color buffer along the velocity vector.
        float4 currentColor = g_objectRender.Sample(g_sampler, texCoord);
        // Add the current color to our color sum.
        color += currentColor;
    }
    // Average all of the samples to get the final blur color.
    float4 finalColor = color / 6.0;
    return finalColor;
}


