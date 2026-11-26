#pragma once

#include <d3d12.h>
#include <wrl.h>

// パーティクル描画・Compute シミュレーション用の RootSignature / PSO 生成を担当する。
class ParticlePipelineBuilder {
public:
	static void CreateGraphicsRootSignature(
		ID3D12Device* device,
		Microsoft::WRL::ComPtr<ID3D12RootSignature>& outRootSignature);
	static void CreateGraphicsPipelineStates(
		ID3D12Device* device,
		ID3D12RootSignature* rootSignature,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& outGpuPipelineState,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& outCpuPipelineState,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& outCpuAlphaPipelineState);
	static void CreateComputeRootSignature(
		ID3D12Device* device,
		Microsoft::WRL::ComPtr<ID3D12RootSignature>& outComputeRootSignature);
	static void CreateComputePipelineStates(
		ID3D12Device* device,
		ID3D12RootSignature* computeRootSignature,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& outInitPipelineState,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& outEmitPipelineState,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& outUpdatePipelineState);
};
