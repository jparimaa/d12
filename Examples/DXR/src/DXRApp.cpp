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
    createRayGenRootSignature();
    createMissRootSignature();
    createHitRootSignature();
    createDummyRootSignatures();
    createStateObject();
    createOutputBuffer();
    createDescriptorHeap();

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

void DXRApp::createRayGenRootSignature()
{
    D3D12_DESCRIPTOR_RANGE outputDesc{};
    outputDesc.BaseShaderRegister = 0; // u0
    outputDesc.NumDescriptors = 1;
    outputDesc.RegisterSpace = 0; // implicit register space 0
    outputDesc.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // RWTexture2D<float4>
    outputDesc.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE tlasDesc{};
    tlasDesc.BaseShaderRegister = 0; // t0
    tlasDesc.NumDescriptors = 1;
    tlasDesc.RegisterSpace = 0; // implicit register space 0
    tlasDesc.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // RaytracingAccelerationStructure
    tlasDesc.OffsetInDescriptorsFromTableStart = 1;

    std::vector<D3D12_DESCRIPTOR_RANGE> rangesPerRootParameter{outputDesc, tlasDesc};

    D3D12_ROOT_PARAMETER rootParameter{};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(rangesPerRootParameter.size());
    rootParameter.DescriptorTable.pDescriptorRanges = rangesPerRootParameter.data();
    std::vector<D3D12_ROOT_PARAMETER> parameters{rootParameter};

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.NumParameters = static_cast<UINT>(parameters.size());
    rootSignatureDesc.pParameters = parameters.data();
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    ID3DBlob* signatureBlob;
    ID3DBlob* errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, &errorBlob);

    if (errorBlob)
    {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        CHECK(hr);
    }

    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    CHECK(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rayGenRootSignature)));
    signatureBlob->Release();
}

void DXRApp::createMissRootSignature()
{
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.NumParameters = 0;
    rootSignatureDesc.pParameters = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    ID3DBlob* signatureBlob;
    ID3DBlob* errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, &errorBlob);

    if (errorBlob)
    {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        CHECK(hr);
    }

    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    CHECK(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_missRootSignature)));
    signatureBlob->Release();
}

void DXRApp::createHitRootSignature()
{
    D3D12_ROOT_PARAMETER rootParameter{};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParameter.Descriptor.RegisterSpace = 0;
    rootParameter.Descriptor.ShaderRegister = 0;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    std::vector<D3D12_ROOT_PARAMETER> parameters{rootParameter};

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.NumParameters = static_cast<UINT>(parameters.size());
    rootSignatureDesc.pParameters = parameters.data();
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    ID3DBlob* signatureBlob;
    ID3DBlob* errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, &errorBlob);

    if (errorBlob)
    {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        CHECK(hr);
    }

    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    CHECK(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_hitRootSignature)));
    signatureBlob->Release();
}

void DXRApp::createDummyRootSignatures()
{
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 0;
    rootDesc.pParameters = nullptr;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE; // A global root signature is the default, hence this flag

    ID3DBlob* signatureBlob;
    ID3DBlob* errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    CHECK(hr);
    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_dummyGlobalRootSignature));
    signatureBlob->Release();
    CHECK(hr);

    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    CHECK(hr);
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_dummyLocalRootSignature));
    signatureBlob->Release();
    CHECK(hr);
}

void DXRApp::createStateObject()
{
    const UINT64 subobjectCount = //
        3 + // Shaders: RayGen, Miss, ClosestHit
        1 + // Hit group
        1 + // Shader configuration
        1 + // Shader payload
        2 * 3 + // Root signature declaration + association
        2 + // Empty global and local root signatures
        1; // Final pipeline subobject

    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.reserve(subobjectCount);

    // Shaders
    struct LibraryExport
    {
        LibraryExport(LPCWSTR n, Microsoft::WRL::ComPtr<IDxcBlob> b) :
            name(n), blob(b) {}

        const LPCWSTR name;
        const Microsoft::WRL::ComPtr<IDxcBlob> blob;
        D3D12_EXPORT_DESC exportDesc{};
        D3D12_DXIL_LIBRARY_DESC libraryDesc{};
        D3D12_STATE_SUBOBJECT subobject{};
    };

    std::vector<LibraryExport> exportData{{L"RayGen", m_rayGenShader}, {L"Miss", m_missShader}, {L"ClosestHit", m_hitShader}};

    for (LibraryExport& e : exportData)
    {
        e.exportDesc.Name = e.name;
        e.exportDesc.ExportToRename = nullptr;
        e.exportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

        e.libraryDesc.DXILLibrary.BytecodeLength = e.blob->GetBufferSize();
        e.libraryDesc.DXILLibrary.pShaderBytecode = e.blob->GetBufferPointer();
        e.libraryDesc.NumExports = 1;
        e.libraryDesc.pExports = &e.exportDesc;

        e.subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        e.subobject.pDesc = &e.libraryDesc;

        subobjects.push_back(e.subobject);
    }

    // Hit group
    D3D12_HIT_GROUP_DESC hitGroupDesc{};
    hitGroupDesc.HitGroupExport = L"HitGroup";
    hitGroupDesc.ClosestHitShaderImport = L"ClosestHit";
    hitGroupDesc.AnyHitShaderImport = nullptr;
    hitGroupDesc.IntersectionShaderImport = nullptr;

    D3D12_STATE_SUBOBJECT hitGroupSubobject{};
    hitGroupSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    hitGroupSubobject.pDesc = &hitGroupDesc;

    subobjects.push_back(hitGroupSubobject);

    // Shader configuration
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{};
    shaderConfig.MaxPayloadSizeInBytes = 4 * sizeof(float); // RGB + distance
    shaderConfig.MaxAttributeSizeInBytes = 2 * sizeof(float); // barycentric coordinates;

    D3D12_STATE_SUBOBJECT shaderConfigSubobject{};
    shaderConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigSubobject.pDesc = &shaderConfig;

    subobjects.push_back(shaderConfigSubobject);

    // Subobject to export association
    const std::vector<std::wstring> exportedSymbols{L"RayGen", L"Miss", L"HitGroup"};
    std::vector<LPCWSTR> exportedSymbolPointers{
        exportedSymbols[0].c_str(),
        exportedSymbols[1].c_str(),
        exportedSymbols[2].c_str()};

    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation{};
    shaderPayloadAssociation.NumExports = static_cast<UINT>(exportedSymbols.size());
    shaderPayloadAssociation.pExports = exportedSymbolPointers.data();
    shaderPayloadAssociation.pSubobjectToAssociate = &subobjects.back();

    D3D12_STATE_SUBOBJECT shaderPayloadAssociationSubobject{};
    shaderPayloadAssociationSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    shaderPayloadAssociationSubobject.pDesc = &shaderPayloadAssociation;
    subobjects.push_back(shaderPayloadAssociationSubobject);

    // Root signature associations
    struct RootSignatureAssociation
    {
        ID3D12RootSignature* rootSig;
        LPCWSTR symbolPointer;
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association{};
    };

    std::vector<RootSignatureAssociation> rootSigAssociations{
        {m_rayGenRootSignature.Get(), L"RayGen"},
        {m_missRootSignature.Get(), L"Miss"},
        {m_hitRootSignature.Get(), L"HitGroup"}};

    for (RootSignatureAssociation& rsa : rootSigAssociations)
    {
        D3D12_STATE_SUBOBJECT rootSigSubobject{};
        rootSigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
        rootSigSubobject.pDesc = &rsa.rootSig;

        subobjects.push_back(rootSigSubobject);

        rsa.association.NumExports = 1;
        rsa.association.pExports = &rsa.symbolPointer;
        rsa.association.pSubobjectToAssociate = &subobjects.back();

        D3D12_STATE_SUBOBJECT rootSigAssociationSubobject{};
        rootSigAssociationSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        rootSigAssociationSubobject.pDesc = &rsa.association;

        subobjects.push_back(rootSigAssociationSubobject);
    }

    // The pipeline construction always requires an empty global root signature
    D3D12_STATE_SUBOBJECT globalRootSig{};
    globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    ID3D12RootSignature* dummyGlobalRootSig = m_dummyGlobalRootSignature.Get();
    globalRootSig.pDesc = &dummyGlobalRootSig;

    subobjects.push_back(globalRootSig);

    // The pipeline construction always requires an empty local root signature
    D3D12_STATE_SUBOBJECT localRootSig{};
    localRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    ID3D12RootSignature* dummyLocalRootSig = m_dummyLocalRootSignature.Get();
    localRootSig.pDesc = &dummyLocalRootSig;

    subobjects.push_back(localRootSig);

    // Ray tracing pipeline configuration
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig{};
    pipelineConfig.MaxTraceRecursionDepth = 1;

    D3D12_STATE_SUBOBJECT pipelineConfigObject{};
    pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    pipelineConfigObject.pDesc = &pipelineConfig;

    subobjects.push_back(pipelineConfigObject);

    // Create the state object
    D3D12_STATE_OBJECT_DESC stateObjectDesc{};
    stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    stateObjectDesc.NumSubobjects = subobjectCount;
    stateObjectDesc.pSubobjects = subobjects.data();

    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    HRESULT hr = device->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&m_stateObject));
    CHECK(hr);
}

void DXRApp::createOutputBuffer()
{
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    resourceDesc.Width = fw::API::getWindowWidth();
    resourceDesc.Height = fw::API::getWindowHeight();
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    CHECK(device->CreateCommittedResource(&c_defaultHeapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_outputBuffer)));
}

void DXRApp::createDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = 2;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    Microsoft::WRL::ComPtr<ID3D12Device5> device = fw::API::getD3dDevice();
    CHECK(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvUavHeap)));

    // Add output buffer as SRV
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(m_outputBuffer.Get(), nullptr, &uavDesc, srvHandle);

    // Add TLAS SRV right after the output buffer
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_tlasBuffer->GetGPUVirtualAddress();
    device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);
}
