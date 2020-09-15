#pragma once

#include <DirectXMath.h>

#include <vector>
#include <array>

class MarchingCubes
{
public:
    using Grid = std::vector<std::vector<std::vector<float>>>;

    struct Vertex
    {
        DirectX::XMVECTOR position;
        DirectX::XMVECTOR normal;
    };

    void createData(size_t size);
    void generateMesh(float limit);
    const std::vector<Vertex>& getVertices() const;

private:
    Grid m_grid;
    float m_limit = 0.0f;
    float m_scale = 0.5f;
    std::vector<Vertex> m_vertices;

    void generateCubeMesh(float x, float y, float z, const std::array<float, 8>& values);
};
