#include "DXRApp.h"

#include <fw/Framework.h>
#include <fw/Common.h>
#include <fw/API.h>
#include <fw/Macros.h>
#include <GLFW/glfw3.h>

#include <minwinbase.h>

#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>

namespace
{
const D3D12_HEAP_PROPERTIES c_defaultHeapProps = {D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0};
const D3D12_HEAP_PROPERTIES c_uploadHeapProps = {D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0};

bool hasDXRSupport()
{
    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};
    CHECK(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options)));
    return options.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
}

IDxcBlob* compileDXRShader(LPCWSTR fileName)
{
    static IDxcCompiler* compiler = nullptr;
    static IDxcLibrary* library = nullptr;
    static IDxcIncludeHandler* dxcIncludeHandler = nullptr;

    if (!compiler)
    {
        CHECK(DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)&compiler));
        CHECK(DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)&library));
        CHECK(library->CreateIncludeHandler(&dxcIncludeHandler));
    }

    std::ifstream shaderFile(fileName);
    assert(shaderFile.good());

    std::stringstream strStream;
    strStream << shaderFile.rdbuf();
    std::string shaderCode = strStream.str();

    IDxcBlobEncoding* textBlob;
    CHECK(library->CreateBlobWithEncodingFromPinned((LPBYTE)shaderCode.c_str(), (uint32_t)shaderCode.size(), 0, &textBlob));

    IDxcOperationResult* result;
    CHECK(compiler->Compile(textBlob, fileName, L"", L"lib_6_3", nullptr, 0, nullptr, 0, dxcIncludeHandler, &result));

    HRESULT resultStatus;
    result->GetStatus(&resultStatus);
    if (FAILED(resultStatus))
    {
        IDxcBlobEncoding* error;
        HRESULT hr = result->GetErrorBuffer(&error);
        if (FAILED(hr))
        {
            std::cerr << "ERROR: Failed to shader compilation error message\n";
        }

        std::vector<char> infoLog(error->GetBufferSize() + 1);
        memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
        infoLog[error->GetBufferSize()] = 0;

        std::cerr << "ERROR: Shader compiler error:\n";
        for (const char& c : infoLog)
        {
            std::cerr << c;
        }
        std::cerr << "\n";

        CHECK(false);
        return nullptr;
    }

    IDxcBlob* blob;
    CHECK(result->GetResult(&blob));
    return blob;
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
    createTLAS(commandList);
    createShaders();

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

    // Get buffer sizes
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

    ID3D12Resource* scratchBuffer;
    CHECK(device->CreateCommittedResource(&c_defaultHeapProps,
                                          D3D12_HEAP_FLAG_NONE,
                                          &bufferDesc,
                                          D3D12_RESOURCE_STATE_COMMON,
                                          nullptr,
                                          IID_PPV_ARGS(&scratchBuffer)));

    bufferDesc.Width = resultSizeInBytes;
    CHECK(device->CreateCommittedResource(&c_defaultHeapProps,
                                          D3D12_HEAP_FLAG_NONE,
                                          &bufferDesc,
                                          D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                                          nullptr,
                                          IID_PPV_ARGS(&m_blasBuffer)));

    // Build BLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
    buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildDesc.Inputs.NumDescs = static_cast<UINT>(geometryDescs.size());
    buildDesc.Inputs.pGeometryDescs = geometryDescs.data();
    buildDesc.DestAccelerationStructureData = {m_blasBuffer->GetGPUVirtualAddress()};
    buildDesc.ScratchAccelerationStructureData = {scratchBuffer->GetGPUVirtualAddress()};
    buildDesc.SourceAccelerationStructureData = 0;
    buildDesc.Inputs.Flags = flags;

    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the build to complete
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_blasBuffer.Get();
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    commandList->ResourceBarrier(1, &uavBarrier);
}

void DXRApp::createTLAS(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList)
{
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    // Get buffer sizes
    const UINT instanceCount = 1;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS accelerationStructureInputs{};
    accelerationStructureInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    accelerationStructureInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    accelerationStructureInputs.NumDescs = instanceCount;
    accelerationStructureInputs.Flags = flags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    device->GetRaytracingAccelerationStructurePrebuildInfo(&accelerationStructureInputs, &info);

    info.ResultDataMaxSizeInBytes = ROUND_UP(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    info.ScratchDataSizeInBytes = ROUND_UP(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    const UINT64 resultSizeInBytes = info.ResultDataMaxSizeInBytes;
    const UINT64 scratchSizeInBytes = info.ScratchDataSizeInBytes;
    const UINT64 instanceDescsSizeInBytes = ROUND_UP(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * static_cast<UINT64>(instanceCount), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

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

    ID3D12Resource* scratchBuffer;
    CHECK(device->CreateCommittedResource(&c_defaultHeapProps,
                                          D3D12_HEAP_FLAG_NONE,
                                          &bufferDesc,
                                          D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                          nullptr,
                                          IID_PPV_ARGS(&scratchBuffer)));

    bufferDesc.Width = resultSizeInBytes;
    CHECK(device->CreateCommittedResource(&c_defaultHeapProps,
                                          D3D12_HEAP_FLAG_NONE,
                                          &bufferDesc,
                                          D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                                          nullptr,
                                          IID_PPV_ARGS(&m_tlasBuffer)));

    bufferDesc.Width = instanceDescsSizeInBytes;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    CHECK(device->CreateCommittedResource(&c_uploadHeapProps,
                                          D3D12_HEAP_FLAG_NONE,
                                          &bufferDesc,
                                          D3D12_RESOURCE_STATE_GENERIC_READ,
                                          nullptr,
                                          IID_PPV_ARGS(&m_tlasInstanceDescsBuffer)));

    // Build TLAS
    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;
    m_tlasInstanceDescsBuffer->Map(0, nullptr, reinterpret_cast<void**>(&instanceDescs));
    assert(instanceDescs);
    ZeroMemory(instanceDescs, instanceDescsSizeInBytes);

    { // Only one instance, if there were multiple instances they should be added here
        const int i = 0;
        instanceDescs[i].InstanceID = 0;
        instanceDescs[i].InstanceContributionToHitGroupIndex = 0;
        instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

        DirectX::XMMATRIX m = DirectX::XMMatrixIdentity();
        memcpy(instanceDescs[i].Transform, &m, sizeof(instanceDescs[i].Transform));
        instanceDescs[i].AccelerationStructure = m_blasBuffer->GetGPUVirtualAddress();
        instanceDescs[i].InstanceMask = 0xFF;
    }

    m_tlasInstanceDescsBuffer->Unmap(0, nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
    buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildDesc.Inputs.InstanceDescs = m_tlasInstanceDescsBuffer->GetGPUVirtualAddress();
    buildDesc.Inputs.NumDescs = instanceCount;
    buildDesc.DestAccelerationStructureData = {m_tlasBuffer->GetGPUVirtualAddress()};
    buildDesc.ScratchAccelerationStructureData = {scratchBuffer->GetGPUVirtualAddress()};
    buildDesc.SourceAccelerationStructureData = 0;
    buildDesc.Inputs.Flags = flags;

    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the build to complete
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_tlasBuffer.Get();
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    commandList->ResourceBarrier(1, &uavBarrier);
}

void DXRApp::createShaders()
{
    m_rayGenShader = compileDXRShader(L"RayGen.hlsl");
    m_missShader = compileDXRShader(L"Miss.hlsl");
    m_hitShader = compileDXRShader(L"Hit.hlsl");
}

void DXRApp::createPSO()
{
}
