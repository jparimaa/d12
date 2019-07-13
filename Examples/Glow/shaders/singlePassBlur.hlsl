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

Texture2D g_texture : register(t0);
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
    float texelHorizontal = 1.0 / 1200.0;
    float texelVertical = 1.0 / 900.0;
    float4 final = float4(0.0, 0.0, 0.0, 0.0);
    for (int i = -2; i <= 2; ++i)
    {
        for (int j = -2; j <= j; ++j)
        {
            float2 uv = vertexOut.uv;
            uv.x += i * texelHorizontal;
            uv.y += j * texelVertical;
            g_texture.Sample(g_sampler, uv);
        }
    }
    
    return final;
}


