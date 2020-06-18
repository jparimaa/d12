#include "DXRApp.h"

#include <fw/Framework.h>
#include <fw/Common.h>
#include <fw/API.h>
#include <fw/Macros.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <cassert>

namespace
{
bool hasDXRSupport()
{
    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};
    CHECK(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options)));
    return options.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
}

} // namespace

bool DXRApp::initialize()
{
    bool status = true;

    status = status && hasDXRSupport();
    assert(status && "No DXR support");

    fw::Model model;
    loadModel(model);

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList = fw::API::getCommandList();
    std::vector<VertexUploadBuffers> vertexUploadBuffers;
    createVertexBuffers(model, commandList, vertexUploadBuffers);
    createBLAS(commandList);

    // Execute and wait initialization commands
    CHECK(commandList->Close());
    fw::API::completeInitialization();

    return status;
}

void DXRApp::update()
{
    if (fw::API::isKeyReleased(GLFW_KEY_ESCAPE))
    {
        fw::API::quit();
    }
}

void DXRApp::fillCommandList()
{
}

void DXRApp::onGUI()
{
}

void DXRApp::loadModel(fw::Model& model)
{
    std::string modelFilepath = ASSET_PATH;
    modelFilepath += "attack_droid.obj";
    bool modelLoaded = model.loadModel(modelFilepath);
    assert(modelLoaded);
}

void DXRApp::createVertexBuffers(const fw::Model& model,
                                 Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList,
                                 std::vector<VertexUploadBuffers>& vertexUploadBuffers)
{
    Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice = fw::API::getD3dDevice();

    const fw::Model::Meshes& meshes = model.getMeshes();
    const size_t numMeshes = meshes.size();
    vertexUploadBuffers.resize(numMeshes);
    m_renderObjects.resize(numMeshes);

    for (size_t i = 0; i < numMeshes; ++i)
    {
        RenderObject& ro = m_renderObjects[i];
        const fw::Mesh& mesh = meshes[i];
        std::vector<fw::Mesh::Vertex> vertices = mesh.getVertices();
        const size_t vertexBufferSize = vertices.size() * sizeof(fw::Mesh::Vertex);
        const size_t indexBufferSize = mesh.indices.size() * sizeof(uint16_t);

        ro.vertexBuffer = fw::createGPUBuffer(d3dDevice.Get(), commandList.Get(), vertices.data(), vertexBufferSize, vertexUploadBuffers[i].vertexUploadBuffer);
        ro.indexBuffer = fw::createGPUBuffer(d3dDevice.Get(), commandList.Get(), mesh.indices.data(), indexBufferSize, vertexUploadBuffers[i].indexUploadBuffer);

        ro.vertexBufferView.BufferLocation = ro.vertexBuffer->GetGPUVirtualAddress();
        ro.vertexBufferView.StrideInBytes = sizeof(fw::Mesh::Vertex);
        ro.vertexBufferView.SizeInBytes = (UINT)vertexBufferSize;

        ro.indexBufferView.BufferLocation = ro.indexBuffer->GetGPUVirtualAddress();
        ro.indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        ro.indexBufferView.SizeInBytes = (UINT)indexBufferSize;

        ro.vertexCount = static_cast<UINT>(vertices.size());
        ro.indexCount = static_cast<UINT>(mesh.indices.size());
    }
}

void DXRApp::createBLAS(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList)
{
    // Add geometries
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;

    for (const RenderObject& ro : m_renderObjects)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc{};
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geometryDesc.Triangles.VertexBuffer.StartAddress = ro.vertexBufferView.BufferLocation;
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = ro.vertexBufferView.StrideInBytes;
        geometryDesc.Triangles.VertexCount = ro.vertexCount;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDesc.Triangles.IndexBuffer = ro.indexBufferView.BufferLocation;
        geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
        geometryDesc.Triangles.IndexCount = ro.indexCount;
        geometryDesc.Triangles.Transform3x4 = 0;

        geometryDescs.push_back(geometryDesc);
    }

    // Get scratch size
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS accelerationStructureInputs{};
    accelerationStructureInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    accelerationStructureInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    accelerationStructureInputs.NumDescs = static_cast<UINT>(geometryDescs.size());
    accelerationStructureInputs.pGeometryDescs = geometryDescs.data();
    accelerationStructureInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    device->GetRaytracingAccelerationStructurePrebuildInfo(&accelerationStructureInputs, &info);

    UINT64 scratchSizeInBytes = ROUND_UP(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    UINT64 resultSizeInBytes = ROUND_UP(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    // Create buffers
    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Alignment = 0;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.Height = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;
    bufferDesc.Width = scratchSizeInBytes;

    D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0};

    ID3D12Resource* scratchBuffer;
    CHECK(device->CreateCommittedResource(&heapProps,
                                          D3D12_HEAP_FLAG_NONE,
                                          &bufferDesc,
                                          D3D12_RESOURCE_STATE_COMMON,
                                          nullptr,
                                          IID_PPV_ARGS(&scratchBuffer)));

    bufferDesc.Width = resultSizeInBytes;
    ID3D12Resource* resultBuffer;
    CHECK(device->CreateCommittedResource(&heapProps,
                                          D3D12_HEAP_FLAG_NONE,
                                          &bufferDesc,
                                          D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                                          nullptr,
                                          IID_PPV_ARGS(&resultBuffer)));

    // Build BLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
    buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildDesc.Inputs.NumDescs = static_cast<UINT>(geometryDescs.size());
    buildDesc.Inputs.pGeometryDescs = geometryDescs.data();
    buildDesc.DestAccelerationStructureData = {resultBuffer->GetGPUVirtualAddress()};
    buildDesc.ScratchAccelerationStructureData = {scratchBuffer->GetGPUVirtualAddress()};
    buildDesc.SourceAccelerationStructureData = 0;
    buildDesc.Inputs.Flags = flags;

    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the build to complete
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = resultBuffer;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    commandList->ResourceBarrier(1, &uavBarrier);
}

void DXRApp::createTLAS()
{
}
