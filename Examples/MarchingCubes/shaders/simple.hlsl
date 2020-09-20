cbuffer cbPerObject : register(b0)
{
    float4x4 world;
    float4x4 worldViewProj;
    float4 cameraPositionAndTime;
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

VertexOut VS(VertexIn vertexIn)
{
    VertexOut vertexOut;
    vertexOut.position = mul(float4(vertexIn.position.xyz, 1.0f), worldViewProj);
    vertexOut.positionWorld = mul(float4(vertexIn.position.xyz, 1.0f), world);
    vertexOut.normal = mul(float4(vertexIn.normal.xyz, 0.0f), world);
    return vertexOut;
}

float4 PS(VertexOut vertexOut) :
    SV_Target
{
    const float3 N = normalize(vertexOut.normal.xyz);

    const float3 directLightDir = float3(-1.0f, -1.0f, 1.0);
    const float directLightPower = min(0.5, max(0.1, dot(N, directLightDir)));

    float pointLightPower = 0.0f;
    const float lightPower = 10.0f;
    const float time = cameraPositionAndTime.w;
    for (int x = -2; x < 2; ++x)
    {
        for (int z = -2; z < 2; ++z)
        {
            float3 lightPosition = cameraPositionAndTime.xyz;
            lightPosition.x += 20.0f * float(x);
            lightPosition.z += 20.0f * float(z);
            lightPosition.y += sin(float(x + z) + time) * 20.0f;
            const float3 pointLightDir = normalize(lightPosition.xyz - vertexOut.positionWorld.xyz);
            const float attenuation = lightPower / pow(distance(lightPosition, vertexOut.positionWorld.xyz), 1.2);
            pointLightPower += attenuation * max(0.0, dot(N, -pointLightDir));
        }
    }

    float3 color = float3(sin(vertexOut.positionWorld.x * 0.1),
                          cos(vertexOut.positionWorld.y * 0.2),
                          -cos(vertexOut.positionWorld.z * 0.3));
    color *= 0.35;
    color += 0.5;

    return (directLightPower + pointLightPower) * float4(color, 1.0);
}
