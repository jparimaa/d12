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
    float texelHorizontal = 1.0 / (1200.0 / 2.0);
    float texelVertical = 1.0 / (900.0 / 2.0);
    float4 final = float4(0.0, 0.0, 0.0, 0.0);
    float weights[7] = {0.01, 0.015, 0.025, 0.04, 0.025, 0.015, 0.01};
    int kernelSize = 3;
    for (int i = -kernelSize; i <= kernelSize; ++i) // Slow single pass blur, more optimal way is to divide to horizontal and vertical blur
    {
        for (int j = -kernelSize; j <= kernelSize; ++j)
        {
            float2 uv = vertexOut.uv;
            uv.x += i * texelHorizontal;
            uv.y += j * texelVertical;
            final += g_texture.Sample(g_sampler, uv) * (weights[i + kernelSize] + weights[j + kernelSize]);
        }
    }
    
    return final;
}


