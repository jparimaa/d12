#pragma once

#include <assimp/material.h>
#include <DirectXMath.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace fw
{
class Mesh
{
public:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT3 tangent;
        DirectX::XMFLOAT2 uv;
    };

    using Vertices = std::vector<Vertex>;
    using TextureNames = std::unordered_map<aiTextureType, std::vector<std::string>>;

    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<DirectX::XMFLOAT3> normals;
    std::vector<DirectX::XMFLOAT3> tangents;
    std::vector<DirectX::XMFLOAT2> uvs;
    std::vector<uint16_t> indices;
    TextureNames textureNames;

    Mesh(){};
    Vertices getVertices() const;
    std::string getFirstTextureOfType(aiTextureType type) const;
};
} // namespace fw
