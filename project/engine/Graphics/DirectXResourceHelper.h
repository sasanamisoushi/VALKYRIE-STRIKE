#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <string>
#include <wrl.h>
#include "externals/DirectXTex/DirectXTex.h"

// DirectX12 のシェーダーコンパイル・バッファ／テクスチャリソース生成と転送を担うヘルパー。
class DirectXResourceHelper {
public:
	static Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* includeHandler,
		const std::wstring& filePath,
		const wchar_t* profile);

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(
		ID3D12Device* device,
		size_t sizeInBytes,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
		ID3D12Device* device,
		const DirectX::TexMetadata& metadata);

	static Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
		const DirectX::ScratchImage& mipImages);

	static DirectX::ScratchImage LoadTexture(const std::string& filePath);
};
