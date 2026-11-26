#include "DirectXCommon.h"
#include "DirectXResourceHelper.h"
#include "Logger.h"
#include "engine/base/StringUtility.h"
#include <dxcapi.h>
#include<cassert>
#include <format>
#include <externals/imgui/imgui.h>
#include <externals/imgui/imgui_impl_win32.h>
#include <externals/imgui/imgui_impl_dx12.h>
#include <iostream>
#include <externals/DirectXTex/d3dx12.h>
#include <thread>
#include "SrvManager.h"


#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

using namespace Microsoft::WRL;
StringUtility su;

const uint32_t DirectXCommon::kMaxSRVCount = 512;

DirectXCommon *DirectXCommon::GetInstance() {
	static DirectXCommon instance;
	return &instance;
}

void DirectXCommon::Initialize(WinApp *winApp) {

	//NULL検出
	assert(winApp);

	//メンバ変数に記録
	this->winApp = winApp;
	renderWidth_ = WinApp::GetClientWidth();
	renderHeight_ = WinApp::GetClientHeight();

	//FPS固定初期化
	InitialieFixFPS();

	//デバイスの初期化
	CreateDevice();

	//コマンド関連の初期化
	CreateCommand();

	//スワップチェーンの初期化
	CreateSwapChain();

	//各種デスクリプタヒープの生成
	CreateDescriptorHeaps();

	//深度バッファの生成
	depthBufferGeneration(renderWidth_, renderHeight_);
	
	//レンダーターゲットビューの初期化
	CreateRenderTargetView();

	//深度ステンシルビューの初期化
	CreateDepthStencilView();

	//フェンスの初期化
	CreateFence();

	//ビューポート矩形の初期化
	CreateViewportScissorRect();

	//シザリング矩形の初期化
	CreateScissorRect();

	//DXCコンパイラの生成
	CreateDXCCompiler();

	//ImGuiの初期化 (ImGuiManagerで行うため、ここでは行わない)
	//InitializeImGui();
}

void DirectXCommon::CreateDevice() {

	HRESULT hr;

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		//デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		//さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif 

	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	assert(SUCCEEDED(hr));

	

	//使用するアダプタ用の変数
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;

	//良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {

		//アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		//ソフトウェアアダプタ出なければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {

			//採用したアダプタの情報をログに出力
			GetLogger().Log(su.ConvertString(std::format(L"Use Adapter: {}\n", adapterDesc.Description)));
			break;
		}

		useAdapter = nullptr;
	}

	//適切なアダプタが見つからなかったので起動できない
	assert(useAdapter != nullptr);


	device = nullptr;
	//機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
	};

	const char *featureLevelStrings[] = { "12.2","12.1","12.0" };

	//高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		//採用したアダプターでデバイス生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		//指定した機能レベルでデバイスを生成できたか確認
		if (SUCCEEDED(hr)) {
			//生成できたのでログ出力を行ってループを抜ける
			GetLogger().Log(std::format("FeatureLevel: {}\n", featureLevelStrings[i]));
			break;
		}
	}

	//デバイスの生成が上手くいかなかったので起動できない
	assert(device != nullptr);
	GetLogger().Log("Complete create D3D12Device!!!\n");


#ifdef _DEBUG

	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		//ヤバいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		//警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		//抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		//指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);

	}
#endif
}

void DirectXCommon::CreateCommand() {

	HRESULT hr;

	//コマンドアロケータを生成する
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	//コマンドアロケータを生成がうまくいかなっかたので起動できない
	assert(SUCCEEDED(hr));


	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));

	//コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドキューを生成
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));

	//コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

}

void DirectXCommon::CreateSwapChain() {

	HRESULT hr;
	//画面の幅
	swapChainDesc.Width = renderWidth_;
	//画面の高さ
	swapChainDesc.Height = renderHeight_;
	//色の形式
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//マルチサンプルしない
	swapChainDesc.SampleDesc.Count = 1;
	//描画ターゲットとして利用する
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	//ダブルバッファ
	swapChainDesc.BufferCount = 2;
	//モニターにうつしたら、中身を破棄
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	//コマンドキュー、ウインドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), winApp->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1 **>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));
}

void DirectXCommon::depthBufferGeneration(int32_t width, int32_t height) {

	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//Resourceの生成

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&depthStencilResource));
	assert(SUCCEEDED(hr));

}

void DirectXCommon::CreateDepthSrv(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	// SRVとして読むときはR24_UNORM_X8_TYPELESSにする
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(depthStencilResource.Get(), &srvDesc, cpuHandle);
}

void DirectXCommon::SetDepthStateToSRV() {
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = depthStencilResource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);
}

void DirectXCommon::SetDepthStateToDSV() {
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = depthStencilResource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	commandList->ResourceBarrier(1, &barrier);
}

void DirectXCommon::CreateDescriptorHeaps() {

	//RTV用のdesriptorSizeを取得
	this->descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//RTV用のdesriptorSizeを取得
	this->descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//DSV用のdesriptorSizeを取得
	this->descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);


	//RTV用のヒープでディスクリプタの数は10。　RTVはShader内で触る物ではないので、shaderVisibleはfalse
	this->rtvDescriptorHeap = CreateDescriptorHesp(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 10, false);

	//SRV用のヒープでディスクリプタの数は128。SRVはShaderVisibleはtrue
	this->srvDescriptorHeap = CreateDescriptorHesp(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);

	//DSV用のヒープでディスクリプタの数は１。DSVはShaderVisidleはfalse
	this->dsvDescriptorHeap = CreateDescriptorHesp(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

}


Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHesp(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	assert(device);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

void DirectXCommon::CreateRenderTargetView() {
	HRESULT hr;
	
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));
	//RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = rtvFormat;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	//ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	// 修正: クラスメンバーの rtvHandles に格納する
	uint32_t rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//裏表の2つ分
	for (uint32_t i = 0; i < 2; ++i) {
		// ハンドルを計算してクラスメンバーに格納
		rtvHandles[i] = rtvStartHandle;
		rtvHandles[i].ptr += (i * rtvDescriptorSize);

		// RTVの生成
		device->CreateRenderTargetView(swapChainResources[i].Get(), &rtvDesc, rtvHandles[i]);
	}

}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index) {
	return GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index) {
	return GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}

void DirectXCommon::CreateDepthStencilView() {
	//DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	//DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void DirectXCommon::CreateFence() {
	HRESULT hr;

	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	//FenceのSignalを持つためのイベントを作成する
	fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);
}

void DirectXCommon::CreateViewportScissorRect() {

	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = static_cast<float>(renderWidth_);
	viewport.Height = static_cast<float>(renderHeight_);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

void DirectXCommon::CreateScissorRect() {

	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRecct.left = 0;
	scissorRecct.right = renderWidth_;
	scissorRecct.top = 0;
	scissorRecct.bottom = renderHeight_;

}

void DirectXCommon::CreateDXCCompiler() {
	HRESULT hr;
	// dxcUtils の初期化
	// 修正: IID_PPV_ARGS に ComPtr::GetAddressOf() を渡す
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_)); // ✅ 修正
	assert(SUCCEEDED(hr));

	// dxcCompiler の初期化
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_)); // ✅ 修正
	assert(SUCCEEDED(hr));

	// include handler の作成
	// 修正: IID_PPV_ARGS に ComPtr::GetAddressOf() を渡す
	hr = dxcUtils_->CreateDefaultIncludeHandler(includeHandler_.GetAddressOf()); // ✅ これでOK
	assert(SUCCEEDED(hr));

}

void DirectXCommon::InitializeImGui() {
	//ImGuiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(winApp->GetHwnd());
	ImGui_ImplDX12_Init(device.Get(),
		swapChainDesc.BufferCount,
		rtvFormat,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
}

void DirectXCommon::PreDraw(SrvManager *srvManager, D3D12_CPU_DESCRIPTOR_HANDLE renderTextureRtvHandle) {

	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	
	////今回のバリアはTransition
	//barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	////Noneにしておく
	//barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	////バリアを張る対象のリソース。現在のバックバッファに対して使う
	//barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	////前のResourceState
	//barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	////後のResourceState
	//barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	////TransitionBarrierを張る
	//commandList->ResourceBarrier(1, &barrier);

	//描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &renderTextureRtvHandle, false, &dsvHandle);

	//指定した色での画面全体をクリアする
	float clearColor[] = { 0.1f,0.25,0.5,1.0f };
	commandList->ClearRenderTargetView(renderTextureRtvHandle, clearColor, 0, nullptr);

	//指定した深度で画面全体をクリアする
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// SrvManagerのヒープをセット
	if (srvManager) {
		ID3D12DescriptorHeap *heaps[] = { srvManager->GetDescriptorHeap() };
		commandList->SetDescriptorHeaps(1, heaps);
	}


	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRecct);
}

void DirectXCommon::PostDraw() {

	HRESULT hr;

	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	
	//今回はRenderTargetからPresentにする
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	//TransitionBarrierを張る
	commandList->ResourceBarrier(1, &barrier);
	//コマンドリストの内容を確定させるすべてのコマンドを積んでからCloseすること
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	//GPUにコマンドリストの実行を行わせる
	Microsoft::WRL::ComPtr<ID3D12CommandList> commandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(1, commandLists->GetAddressOf());
	//GPUとのosに画面の交換を行うよう通知する
	swapChain->Present(1, 0);

	// Present(1, 0) が垂直同期を行う。ここで別途スリープすると、
	// OS のスケジューリング誤差によってフレーム間隔が不均一になる。

	//Fenceの値を更新
	fenceValue++;
	//GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	commandQueue->Signal(fence.Get(), fenceValue);

	//Fenceの値が指定したSignal値にたどり着いているか確認する
	//GetCompletedValueの初期値はFence作成時に渡した初期値
	if (fence->GetCompletedValue() < fenceValue) {
		//指定したSignalにたどり着いていないのでたどり着くまで待つようにイベントを設定する
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		//イベント待つ
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	//次のフレーム用のコマンドリスト準備
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::PreDrawSwapchain() {
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	// ここで初めてスワップチェーン用のバリアを張る
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier);

	// スライド3枚目：Swapchainに対してOMSetRenderTargets
	// ※Depth(dsvHandle)は不要なので nullptr を渡す！
	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

	// スライド3枚目：Swapchainの画面をクリア
	float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f }; // 画面の余白は黒
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

	// ※SwapchainではDepthを使わないので ClearDepthStencilView は呼ばない！

	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRecct);
}

void DirectXCommon::Resize(int32_t width, int32_t height) {
	if (width <= 0 || height <= 0) {
		return;
	}
	if (width == renderWidth_ && height == renderHeight_) {
		return;
	}

	WaitForGPU();

	for (auto &resource : swapChainResources) {
		resource.Reset();
	}
	depthStencilResource.Reset();

	HRESULT hr = swapChain->ResizeBuffers(
		swapChainDesc.BufferCount,
		static_cast<UINT>(width),
		static_cast<UINT>(height),
		swapChainDesc.Format,
		0);
	assert(SUCCEEDED(hr));

	renderWidth_ = width;
	renderHeight_ = height;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;

	CreateRenderTargetView();
	depthBufferGeneration(renderWidth_, renderHeight_);
	CreateDepthStencilView();
	CreateViewportScissorRect();
	CreateScissorRect();
}

void DirectXCommon::WaitForGPU() {
	fenceValue++;
	commandQueue->Signal(fence.Get(), fenceValue);

	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring &filePath, const wchar_t *profile) {
	return DirectXResourceHelper::CompileShader(dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get(), filePath, profile);
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes, D3D12_RESOURCE_FLAGS flags) {
	return DirectXResourceHelper::CreateBufferResource(device.Get(), sizeInBytes, flags);
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata &metadata) {
	return DirectXResourceHelper::CreateTextureResource(device.Get(), metadata);
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource> &texture, const DirectX::ScratchImage &mipImages) {
	return DirectXResourceHelper::UploadTextureData(device.Get(), commandList.Get(), texture, mipImages);
}

DirectX::ScratchImage DirectXCommon::LoadTexture(const std::string &filePath) {
	return DirectXResourceHelper::LoadTexture(filePath);
}



D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> &descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> &descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetNewRtvHandle() {
	// ヒープの先頭アドレスを取得
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	// 現在の空き番号（rtvHeapIndex_）の場所までアドレスをずらす
	handle.ptr += (descriptorSizeRTV * rtvHeapIndex_);

	// 次に呼ばれた時のために番号を1増やす（3, 4, 5...と増えていく）
	rtvHeapIndex_++;

	return handle;
}

void DirectXCommon::InitialieFixFPS() {
	//現在時間を記憶する
	reference_ = std::chrono::steady_clock::now();

	//現在時間を記録する
	reference_ = std::chrono::steady_clock::now();

}

void DirectXCommon::UpdateFixFPS() {

	//1/60秒ピッタリの時間
	const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));

	//1/60秒よりわずかに短い時間
	const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f/65.0f));

	//現在時間を取得
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	//前回記録した時間からの経過時間を取得
	std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);
	//1/60秒より短い場合
	if (elapsed < kMinCheckTime) {
		//1/60秒経過するまで微小なスリープを繰り返す
		while(std::chrono::steady_clock::now()-reference_<kMinTime) {
			//わずかに1/60秒を下回る時間スリープする
			std::this_thread::sleep_for(std::chrono::microseconds(1));
			
		}
	}
	//現在の時間を記録する
	reference_ = std::chrono::steady_clock::now();
}



