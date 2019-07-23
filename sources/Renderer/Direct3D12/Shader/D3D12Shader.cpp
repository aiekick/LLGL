/*
 * D3D12Shader.cpp
 * 
 * This file is part of the "LLGL" project (Copyright (c) 2015-2018 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "D3D12Shader.h"
#include "../../DXCommon/DXCore.h"
#include "../../DXCommon/DXTypes.h"
#include "../../../Core/Helper.h"
#include <algorithm>
#include <stdexcept>
#include <d3dcompiler.h>


namespace LLGL
{


D3D12Shader::D3D12Shader(const ShaderDescriptor& desc) :
    Shader { desc.type }
{
    if (!Build(desc))
        hasErrors_ = true;
}

bool D3D12Shader::HasErrors() const
{
    return hasErrors_;
}

std::string D3D12Shader::Disassemble(int flags)
{
    if (!byteCode_.empty())
    {
        ComPtr<ID3DBlob> disasm;

        auto hr = D3DDisassemble(byteCode_.data(), byteCode_.size(), DXGetDisassemblerFlags(flags), nullptr, &disasm);
        DXThrowIfFailed(hr, "failed to disassemble D3D12 shader byte code");

        return DXGetBlobString(disasm.Get());
    }
    return "";
}

std::string D3D12Shader::QueryInfoLog()
{
    return (errors_.Get() != nullptr ? DXGetBlobString(errors_.Get()) : "");
}

D3D12_SHADER_BYTECODE D3D12Shader::GetByteCode() const
{
    D3D12_SHADER_BYTECODE byteCode;

    byteCode.pShaderBytecode    = byteCode_.data();
    byteCode.BytecodeLength     = static_cast<SIZE_T>(byteCode_.size());

    return byteCode;
}

void D3D12Shader::Reflect(ShaderReflection& reflection) const
{
    if (!byteCode_.empty())
        ReflectShaderByteCode(reflection);
}


/*
 * ======= Private: =======
 */

bool D3D12Shader::Build(const ShaderDescriptor& shaderDesc)
{
    if (IsShaderSourceCode(shaderDesc.sourceType))
        return CompileSource(shaderDesc);
    else
        return LoadBinary(shaderDesc);
}

// see https://msdn.microsoft.com/en-us/library/windows/desktop/dd607324(v=vs.85).aspx
bool D3D12Shader::CompileSource(const ShaderDescriptor& shaderDesc)
{
    /* Get source code */
    std::string fileContent;
    const char* sourceCode      = nullptr;
    SIZE_T      sourceLength    = 0;

    if (shaderDesc.sourceType == ShaderSourceType::CodeFile)
    {
        fileContent     = ReadFileString(shaderDesc.source);
        sourceCode      = fileContent.c_str();
        sourceLength    = fileContent.size();
    }
    else
    {
        sourceCode      = shaderDesc.source;
        sourceLength    = shaderDesc.sourceSize;
    }

    /* Get parameter from union */
    const char* entry   = shaderDesc.entryPoint;
    const char* target  = (shaderDesc.profile != nullptr ? shaderDesc.profile : "");
    auto        flags   = shaderDesc.flags;

    /* Compile shader code */
    ComPtr<ID3DBlob> code;

    auto hr = D3DCompile(
        sourceCode,
        sourceLength,
        nullptr,                            // LPCSTR               pSourceName
        nullptr,                            // D3D_SHADER_MACRO*    pDefines
        nullptr,                            // ID3DInclude*         pInclude
        entry,                              // LPCSTR               pEntrypoint
        target,                             // LPCSTR               pTarget
        DXGetCompilerFlags(flags),          // UINT                 Flags1
        0,                                  // UINT                 Flags2 (recommended to always be 0)
        code.ReleaseAndGetAddressOf(),      // ID3DBlob**           ppCode
        errors_.ReleaseAndGetAddressOf()    // ID3DBlob**           ppErrorMsgs
    );

    /* Get byte code from blob */
    if (code)
        byteCode_ = DXGetBlobData(code.Get());

    /* Store if compilation was successful */
    return !FAILED(hr);
}

bool D3D12Shader::LoadBinary(const ShaderDescriptor& shaderDesc)
{
    if (shaderDesc.sourceType == ShaderSourceType::BinaryFile)
    {
        /* Load binary code from file */
        byteCode_ = ReadFileBuffer(shaderDesc.source);
    }
    else
    {
        /* Copy binary code into container and create native shader */
        byteCode_.resize(shaderDesc.sourceSize);
        std::copy(shaderDesc.source, shaderDesc.source + shaderDesc.sourceSize, byteCode_.begin());
    }

    return !byteCode_.empty();
}

/*
NOTE:
Most of this code for shader reflection is 1:1 copied from the D3D11 renderer.
However, all descriptors have the "D3D12" prefix, so a generalization (without macros) is tricky.
*/

static ShaderResource* FetchOrInsertResource(
    ShaderReflection&   reflection,
    const char*         name,
    const ResourceType  type,
    std::uint32_t       slot)
{
    /* Fetch resource from list */
    for (auto& resource : reflection.resources)
    {
        if (resource.binding.type == type &&
            resource.binding.slot == slot &&
            resource.binding.name.compare(name) == 0)
        {
            return (&resource);
        }
    }

    /* Allocate new resource and initialize parameters */
    reflection.resources.resize(reflection.resources.size() + 1);
    auto ref = &(reflection.resources.back());
    {
        ref->binding.name = std::string(name);
        ref->binding.type = type;
        ref->binding.slot = slot;
    }
    return ref;
}

static void ReflectShaderVertexAttributes(
    ID3D12ShaderReflection*     reflectionObject,
    const D3D12_SHADER_DESC&    shaderDesc,
    ShaderReflection&           reflection)
{
    for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
    {
        /* Get signature parameter descriptor */
        D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
        auto hr = reflectionObject->GetInputParameterDesc(i, &paramDesc);

        if (FAILED(hr))
        {
            std::string info = "failed to retrieve D3D12 signature parameter descriptor " + std::to_string(i + 1) + " of " + std::to_string(shaderDesc.InputParameters);
            DXThrowIfFailed(hr, info.c_str());
        }

        /* Add vertex attribute to output list */
        VertexAttribute vertexAttrib;
        {
            vertexAttrib.name           = std::string(paramDesc.SemanticName);
            vertexAttrib.format         = DXGetSignatureParameterType(paramDesc.ComponentType, paramDesc.Mask);
            vertexAttrib.semanticIndex  = paramDesc.SemanticIndex;
            vertexAttrib.systemValue    = DXTypes::Unmap(paramDesc.SystemValueType);
        }
        reflection.vertexAttributes.push_back(vertexAttrib);
    }
}

static void ReflectShaderResourceGeneric(
    const D3D12_SHADER_INPUT_BIND_DESC& inputBindDesc,
    ShaderReflection&                   reflection,
    const ResourceType                  resourceType,
    long                                bindFlags,
    long                                stageFlags,
    const StorageBufferType             storageBufferType   = StorageBufferType::Undefined)
{
    /* Initialize resource view descriptor for a generic resource (texture, sampler, storage buffer etc.) */
    auto resource = FetchOrInsertResource(reflection, inputBindDesc.Name, resourceType, inputBindDesc.BindPoint);
    {
        resource->binding.bindFlags     |= bindFlags;
        resource->binding.stageFlags    |= stageFlags;
        resource->binding.arraySize     = inputBindDesc.BindCount;
        resource->storageBufferType     = storageBufferType;
    }
}

static void ReflectShaderConstantBuffer(
    ID3D12ShaderReflection*             reflectionObject,
    ShaderReflection&                   reflection,
    const D3D12_SHADER_DESC&            shaderDesc,
    const D3D12_SHADER_INPUT_BIND_DESC& inputBindDesc,
    long                                stageFlags,
    UINT&                               cbufferIdx)
{
    /* Initialize resource view descriptor for constant buffer */
    auto resource = FetchOrInsertResource(reflection, inputBindDesc.Name, ResourceType::Buffer, inputBindDesc.BindPoint);
    {
        resource->binding.bindFlags     |= BindFlags::ConstantBuffer;
        resource->binding.stageFlags    |= stageFlags;
        resource->binding.arraySize     = inputBindDesc.BindCount;
    }

    /* Determine constant buffer size */
    if (cbufferIdx < shaderDesc.ConstantBuffers)
    {
        auto cbufferReflection = reflectionObject->GetConstantBufferByIndex(cbufferIdx++);

        D3D12_SHADER_BUFFER_DESC shaderBufferDesc;
        auto hr = cbufferReflection->GetDesc(&shaderBufferDesc);
        DXThrowIfFailed(hr, "failed to retrieve D3D12 shader buffer descriptor");

        if (shaderBufferDesc.Type == D3D_CT_CBUFFER)
        {
            /* Store constant buffer size in output descriptor */
            resource->constantBufferSize = shaderBufferDesc.Size;
        }
        else
        {
            /* Type mismatch in descriptors */
            throw std::runtime_error(
                "failed to match D3D12 shader buffer descriptor \"" + std::string(shaderBufferDesc.Name) +
                "\" with input binding descriptor for constant buffer \"" + std::string(inputBindDesc.Name) + "\""
            );
        }
    }
    else
    {
        /* Resource index mismatch in descriptor */
        throw std::runtime_error(
            "failed to find D3D12 shader buffer descriptor for input binding descriptor \"" +
            std::string(inputBindDesc.Name) + "\""
        );
    }
}

static void ReflectShaderInputBindings(
    ID3D12ShaderReflection*     reflectionObject,
    const D3D12_SHADER_DESC&    shaderDesc,
    long                        stageFlags,
    ShaderReflection&           reflection)
{
    UINT cbufferIdx = 0;

    for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
    {
        /* Get shader input resource descriptor */
        D3D12_SHADER_INPUT_BIND_DESC inputBindDesc;
        auto hr = reflectionObject->GetResourceBindingDesc(i, &inputBindDesc);
        DXThrowIfFailed(hr, "failed to retrieve D3D12 shader input binding descriptor");

        /* Reflect shader resource view */
        switch (inputBindDesc.Type)
        {
            case D3D_SIT_CBUFFER:
                ReflectShaderConstantBuffer(reflectionObject, reflection, shaderDesc, inputBindDesc, stageFlags, cbufferIdx);
                break;

            case D3D_SIT_TBUFFER:
            case D3D_SIT_TEXTURE:
                ReflectShaderResourceGeneric(inputBindDesc, reflection, ResourceType::Texture, BindFlags::SampleBuffer, stageFlags);
                break;

            case D3D_SIT_SAMPLER:
                ReflectShaderResourceGeneric(inputBindDesc, reflection, ResourceType::Sampler, 0, stageFlags);
                break;

            case D3D_SIT_STRUCTURED:
            case D3D_SIT_BYTEADDRESS:
                ReflectShaderResourceGeneric(inputBindDesc, reflection, ResourceType::Buffer, BindFlags::SampleBuffer, stageFlags);
                break;

            case D3D_SIT_UAV_RWTYPED:
            case D3D_SIT_UAV_RWSTRUCTURED:
            case D3D_SIT_UAV_RWBYTEADDRESS:
            case D3D_SIT_UAV_APPEND_STRUCTURED:
            case D3D_SIT_UAV_CONSUME_STRUCTURED:
            case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
                ReflectShaderResourceGeneric(inputBindDesc, reflection, ResourceType::Buffer, BindFlags::RWStorageBuffer, stageFlags);
                break;

            default:
                break;
        }
    }
}

void D3D12Shader::ReflectShaderByteCode(ShaderReflection& reflection) const
{
    HRESULT hr = 0;

    /* Get shader reflection */
    ComPtr<ID3D12ShaderReflection> reflectionObject;
    hr = D3DReflect(byteCode_.data(), byteCode_.size(), IID_PPV_ARGS(reflectionObject.ReleaseAndGetAddressOf()));
    DXThrowIfFailed(hr, "failed to retrieve D3D12 shader reflection");

    D3D12_SHADER_DESC shaderDesc;
    hr = reflectionObject->GetDesc(&shaderDesc);
    DXThrowIfFailed(hr, "failed to retrieve D3D12 shader descriptor");

    /* Get input parameter descriptors */
    if (GetType() == ShaderType::Vertex)
        ReflectShaderVertexAttributes(reflectionObject.Get(), shaderDesc, reflection);

    /* Get input bindings */
    ReflectShaderInputBindings(reflectionObject.Get(), shaderDesc, GetStageFlags(), reflection);
}


} // /namespace LLGL



// ================================================================================
