#pragma once

#include <DirectXMath.h>

#include <vector>
#include <array>

class MarchingCubes
{
public:
    using Grid = std::vector<std::vector<std::vector<float>>>;

    void createData(size_t size);
    void generateMesh(float limit);
    const std::vector<DirectX::XMFLOAT3>& getVertices() const;

private:
    Grid m_grid;
    float m_limit = 0.0f;
    std::vector<DirectX::XMFLOAT3> m_vertices;

    void generateCubeMesh(float x, float y, float z, const std::array<float, 8>& values);
};
