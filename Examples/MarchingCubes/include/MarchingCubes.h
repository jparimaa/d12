#pragma once

#include <DirectXMath.h>

#include <vector>
#include <array>

class MarchingCubes
{
public:
    struct Vertex
    {
        DirectX::XMVECTOR position;
        DirectX::XMVECTOR normal;
    };

    void generateVertices(size_t size);
    const std::vector<Vertex>& getVertices() const;
    const std::vector<int>& getIndices() const;

private:
    using DataSet = std::vector<std::vector<std::vector<float>>>;
    DataSet m_dataSet;
    const float m_limit = 0.0f;

    struct CubeInfo
    {
        std::vector<Vertex> vertices;
        std::vector<int> indices;
        std::vector<DirectX::XMVECTOR> m_shadingNormals;
    };
    using CubeData = std::vector<std::vector<std::vector<CubeInfo>>>;
    CubeData m_cubeData;

    std::vector<Vertex> m_vertices;
    std::vector<int> m_indices;

    void generateData(size_t size);
    void generateMesh();
    void generateCubeTriangles(CubeInfo& cubeInfo, size_t x, size_t y, size_t z, const std::array<float, 8>& values);
    void generateIndicesAndShadingNormals();
    void generateVertexDataForRendering();
};
