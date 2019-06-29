#include "Mesh.h"

namespace fw
{
Mesh::Vertices Mesh::getVertices() const
{
    Vertices vertices;
    vertices.reserve(positions.size());
    for (unsigned int i = 0; i < positions.size(); ++i)
    {
        Vertex v;
        v.position = positions[i];
        v.normal = normals[i];

        if (!tangents.empty())
        {
            v.tangent = tangents[i];
        }

        if (!uvs.empty())
        {
            v.uv = uvs[i];
        }
        vertices.push_back(std::move(v));
    }
    return vertices;
}

std::string Mesh::getFirstTextureOfType(aiTextureType type) const
{
    auto typeIter = textureNames.find(type);
    if (typeIter != textureNames.end() && typeIter->second.empty())
    {
        return typeIter->second.front();
    }
    return "";
}

} // namespace fw
