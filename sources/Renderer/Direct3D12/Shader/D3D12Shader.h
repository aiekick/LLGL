/*
 * D3D12Shader.h
 * 
 * This file is part of the "LLGL" project (Copyright (c) 2015 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef __LLGL_D3D12_SHADER_H__
#define __LLGL_D3D12_SHADER_H__


#include <LLGL/Shader.h>
#include <LLGL/VertexAttribute.h>
#include <LLGL/ConstantBuffer.h>
#include <LLGL/StorageBuffer.h>
#include "../../ComPtr.h"
#include <vector>
#include <d3d12.h>


namespace LLGL
{


class D3D12Shader : public Shader
{

    public:

        D3D12Shader(const D3D12Shader&) = delete;
        D3D12Shader& operator = (const D3D12Shader&) = delete;

        D3D12Shader(const ShaderType type);
        ~D3D12Shader();

        bool Compile(const std::string& shaderSource) override;
        bool Compile(const std::string& shaderSource, const std::string& entryPoint, const std::string& target, int flags = 0) override;

        std::string Disassemble(int flags = 0) override;

        std::string QueryInfoLog() override;

        /* ----- Extended internal functions ---- */

        D3D12_SHADER_BYTECODE GetByteCode() const;

        inline const std::vector<VertexAttribute>& GetVertexAttributes() const
        {
            return vertexAttributes_;
        }

        inline const std::vector<ConstantBufferDescriptor>& GetConstantBufferDescs() const
        {
            return constantBufferDescs_;
        }

        inline const std::vector<StorageBufferDescriptor>& GetStorageBufferDescs() const
        {
            return storageBufferDescs_;
        }

    private:

        void ReflectShader();

        std::vector<char>                       byteCode_;
        ComPtr<ID3DBlob>                        errors_;

        std::vector<VertexAttribute>            vertexAttributes_;
        std::vector<ConstantBufferDescriptor>   constantBufferDescs_;
        std::vector<StorageBufferDescriptor>    storageBufferDescs_;

};


} // /namespace LLGL


#endif



// ================================================================================
