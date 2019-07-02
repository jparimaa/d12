#include "Model.h"
#include "Common.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <iostream>
#include <utility>

namespace fw
{
bool Model::loadModel(const std::string& file)
{
    Assimp::Importer importer;
    unsigned int flags = aiProcess_Triangulate
        | aiProcess_JoinIdenticalVertices
        | aiProcess_GenSmoothNormals
        | aiProcess_GenUVCoords
        | aiProcess_CalcTangentSpace;
    const aiScene* aScene = importer.ReadFile(file, flags);

    if (aScene)
    {
        if (aScene->mNumMeshes == 0)
        {
            std::cerr << "No mesh found in the model: " << file << "\n";
            return false;
        }

        for (unsigned int meshIndex = 0; meshIndex < aScene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* aMesh = aScene->mMeshes[meshIndex];
            Mesh mesh;
            for (unsigned int vertexIndex = 0; vertexIndex < aMesh->mNumVertices; ++vertexIndex)
            {
                mesh.positions.push_back(DirectX::XMFLOAT3(aMesh->mVertices[vertexIndex].x,
                                                           aMesh->mVertices[vertexIndex].y,
                                                           aMesh->mVertices[vertexIndex].z));

                mesh.normals.push_back(DirectX::XMFLOAT3(aMesh->mNormals[vertexIndex].x,
                                                         aMesh->mNormals[vertexIndex].y,
                                                         aMesh->mNormals[vertexIndex].z));

                if (aMesh->HasTangentsAndBitangents())
                {
                    mesh.tangents.push_back(DirectX::XMFLOAT3(aMesh->mTangents[vertexIndex].x,
                                                              aMesh->mTangents[vertexIndex].y,
                                                              aMesh->mTangents[vertexIndex].z));
                }

                if (aMesh->HasTextureCoords(0))
                {
                    mesh.uvs.push_back(DirectX::XMFLOAT2(aMesh->mTextureCoords[0][vertexIndex].x,
                                                         1.0f - aMesh->mTextureCoords[0][vertexIndex].y));
                }
            }

            for (unsigned int faceIndex = 0; faceIndex < aMesh->mNumFaces; ++faceIndex)
            {
                if (aMesh->mFaces[faceIndex].mNumIndices != 3)
                {
                    std::cerr << "Unable to parse model indices for " << file << "\n";
                    return false;
                }
                mesh.indices.push_back(aMesh->mFaces[faceIndex].mIndices[0]);
                mesh.indices.push_back(aMesh->mFaces[faceIndex].mIndices[1]);
                mesh.indices.push_back(aMesh->mFaces[faceIndex].mIndices[2]);
            }

            aiMaterial* aMaterial = aScene->mMaterials[aMesh->mMaterialIndex];
            if (aMaterial)
            {
                for (int typeIndex = 0; typeIndex < aiTextureType_UNKNOWN; ++typeIndex)
                {
                    aiTextureType type = static_cast<aiTextureType>(typeIndex);
                    unsigned int numTextures = aMaterial->GetTextureCount(type);
                    for (unsigned int texIndex = 0; texIndex < numTextures; ++texIndex)
                    {
                        aiString path;
                        aMaterial->GetTexture(type, texIndex, &path);
                        mesh.textureNames[type].push_back(std::string(path.C_Str()));
                    }
                }
            }

            if (aScene->HasTextures())
            {
                for (unsigned int i = 0; i < aScene->mNumTextures; ++i)
                {
                    aiTexture* aTexture = aScene->mTextures[i];
                    assert(aTexture->mHeight == 0); // Means texture is compressed
                    m_textureDatas[i] = std::vector<unsigned char>(aTexture->mWidth);
                    TextureData& data = m_textureDatas[i];
                    std::memcpy(data.data(), aTexture->pcData, aTexture->mWidth);
                }
            }

            if (mesh.positions.empty())
            {
                std::cerr << "Invalid mesh (no vertices): " << file << "\n";
                return false;
            }

            size_t numVertices = mesh.positions.size();
            if (mesh.normals.size() != numVertices)
            {
                std::cerr << "Invalid mesh (number of vertices and normals do not match): " << file << "\n";
                return false;
            }
            m_meshes.push_back(std::move(mesh));
        }
    }
    else
    {
        std::cerr << "Failed to read model: " << file << "\n";
        std::cerr << "Assimp error message: " << importer.GetErrorString() << "\n";
        return false;
    }

    if (m_meshes.empty())
    {
        std::cerr << "Empty model: " << file << "\n";
        return false;
    }

    return true;
}

const Model::Meshes& Model::getMeshes() const
{
    return m_meshes;
}

const Model::TextureData& Model::getTextureData(unsigned int index)
{
    return m_textureDatas[index];
}

} // namespace fw
