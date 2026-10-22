#include "ParticleManager.h"
#include <cassert>
#include "Logger.h"
#include "engine/Resource/TextureManager.h"
#include "engine/Graphics/SrvManager.h"
#include "engine/base/StringUtility.h"
#include <algorithm>

#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;

namespace {
uint32_t ClampEmitCountToCapacity(const ParticleManager::ParticleGroup &group, uint32_t requestedCount) {
	const size_t maxCount = static_cast<size_t>(group.kNumMaxInstance);
	const size_t activeCount = group.particle.size();
	if (activeCount >= maxCount) {
		return 0;
	}

	const size_t availableCount = maxCount - activeCount;
	return static_cast<uint32_t>((std::min)(static_cast<size_t>(requestedCount), availableCount));
}

constexpr bool kEnableGpuParticleSimulation = false;
}

IDxcBlob *CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring &filePath,
	// Compilerに使用するProfile
	const wchar_t *profile,
	// 初期化で生成したものを3つ
	IDxcUtils *dxcUtils,
	IDxcCompiler3 *dxcCompiler, // 変数名をわかりやすく変更
	IDxcIncludeHandler *includeHandler) {
	// 1. これからシェーダーをコンパイルする旨をログに出す
	// Loggerクラスのインスタンス(logger)を使用
	std::string message = StringUtility::ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile));
	logger.Log(message);

	// 2. hlslファイルを読む
	ComPtr<IDxcBlobEncoding> shaderSource = nullptr; // ComPtrに変更
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	// 読めなかったら止める
	assert(SUCCEEDED(hr));

	// 3. 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	LPCWSTR arguments[] = {
		filePath.c_str(),          // コンパイル対象のhlslファイル名
		L"-E", L"main",            // エントリーポイント
		L"-T", profile,            // ShaderProfile
		L"-Zi", L"-Qembed_debug",  // デバッグ情報
		L"-Od",                    // 最適化なし
		L"-Zpr",                   // 行優先
	};

	// 4. 実際にShaderをコンパイルする
	ComPtr<IDxcResult> shaderResult = nullptr; // ComPtrに変更
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult)
	);
	// 致命的なエラーチェック
	assert(SUCCEEDED(hr));

	// 5. 警告・エラーが出てたらログに出して止める
	ComPtr<IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		// エラー内容をログに出す
		std::string errorMsg = shaderError->GetStringPointer();
		logger.Log(errorMsg);
		// 警告・エラーダメゼッタイ
		assert(false);
	}

	// 6. コンパイル結果から実行用のバイナリ部分を取得
	ComPtr<IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	// 7. 成功したらログを出す
	message = StringUtility::ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile));
	logger.Log(message);

	// ComPtrを使っているので手動Releaseは不要です

	// 実行用のバイナリを返却 (Detachして所有権を呼び出し元に移す)
	return shaderBlob.Detach();
}


void ParticleManager::Initialize(DirectXCommon *dxCommon) {
	// -------------------------------------------------------------
	// 引数でDirectXCommonとSrvManagerのポインタを受け取って記録する
	// -------------------------------------------------------------

	dxCommon_ = dxCommon;

	//デバイスを取得しておく
	ID3D12Device *device = dxCommon_->GetDevice();

	// -------------------------------------------------------------
	// ランダムエンジンの初期化
	// -------------------------------------------------------------

	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());

	// -------------------------------------------------------------
	// パイプライン生成 (RootSignatureとPSO)
	// -------------------------------------------------------------

	CreateRootSignature();
	CreatePipelineState();
	CreateComputeRootSignature();
	CreateComputePipelineState();

	// -------------------------------------------------------------
	// 頂点データの初期化
	// -------------------------------------------------------------

	//パーティクルを板ポリゴンとして描画するための４頂点
	VertexData vertices[4];
	//左下
	vertices[0].position = { -1.0f,-1.0f,0.0f,1.0f };
	vertices[0].texcoord = { 0.0f,1.0f };
	vertices[0].normal = { 0.0f,0.0f,-1.0f };
	//左上
	vertices[1].position = { -1.0f, 1.0f, 0.0f, 1.0f };
	vertices[1].texcoord = { 0.0f,0.0f };
	vertices[1].normal = { 0.0f,0.0f,-1.0f };
	//右下
	vertices[2].position = { 1.0f,-1.0f,0.0f,1.0f };
	vertices[2].texcoord = { 1.0f,1.0f };
	vertices[2].normal = { 0.0f,0.0f,-1.0f };
	//右上
	vertices[3].position = { 1.0f,1.0f,0.0f,1.0f };
	vertices[3].texcoord = { 1.0f,0.0f };
	vertices[3].normal = { 0.0f,0.0f,-1.0f };

	// -------------------------------------------------------------
	// 頂点リソース生成
	// -------------------------------------------------------------

	// 頂点バッファのサイズ = 頂点1つのサイズ * 頂点数
	UINT sizeInBytes = sizeof(VertexData) * 4;

	// ヒーププロパティ設定(Upload)
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// リソース設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes;
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexResource_)
	);
	assert(SUCCEEDED(hr));

	// -------------------------------------------------------------
	// 頂点バッファビュー (VBV) 作成
	// -------------------------------------------------------------
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeInBytes;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// -------------------------------------------------------------
	// 頂点リソースに頂点データを書き込む
	// -------------------------------------------------------------
	VertexData *vertexMap = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void **>(&vertexMap));

	// 全頂点データをメモリコピー
	std::memcpy(vertexMap, vertices, sizeof(vertices));

	vertexResource_->Unmap(0, nullptr);

	// --- GPU Particle 追加分 ---
	// カメラ用リソースの生成
	D3D12_HEAP_PROPERTIES cameraHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC cameraResDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(CameraForGPU));

	hr = device->CreateCommittedResource(
		&cameraHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&cameraResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&cameraResource_)
	);
	assert(SUCCEEDED(hr));

	// マッピング
	hr = cameraResource_->Map(0, nullptr, reinterpret_cast<void **>(&cameraDataPtr_));
	assert(SUCCEEDED(hr));

	// EmitterSphere用リソースの生成
	D3D12_HEAP_PROPERTIES emitterHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC emitterResDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(EmitterSphere));

	hr = device->CreateCommittedResource(
		&emitterHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&emitterResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&emitterResource_)
	);
	assert(SUCCEEDED(hr));

	// マッピング
	hr = emitterResource_->Map(0, nullptr, reinterpret_cast<void **>(&emitterDataPtr_));
	assert(SUCCEEDED(hr));

	// 初期値
	emitterDataPtr_->count = 10;
	emitterDataPtr_->frequency = 0.5f;
	emitterDataPtr_->frequencyTime = 0.0f;
	emitterDataPtr_->translate = Vector3(0.0f, 0.0f, 0.0f);
	emitterDataPtr_->radius = 1.0f;
	emitterDataPtr_->emit = 0;

	// PerFrame用リソースの生成
	D3D12_HEAP_PROPERTIES perFrameHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC perFrameResDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(PerFrame));

	hr = device->CreateCommittedResource(
		&perFrameHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&perFrameResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&perFrameResource_)
	);
	assert(SUCCEEDED(hr));
	hr = perFrameResource_->Map(0, nullptr, reinterpret_cast<void **>(&perFrameDataPtr_));
	assert(SUCCEEDED(hr));
	perFrameDataPtr_->time = 0.0f;
	perFrameDataPtr_->deltaTime = 1.0f / 60.0f;
}

void ParticleManager::Update(Camera *camera) {
	assert(camera); // カメラがないと描画計算ができません

	// 1. カメラ情報の取得
	Matrix4x4 matView = camera->GetViewMatrix();
	Matrix4x4 matProjection = camera->GetProjectionMatrix();

	// 2. ビルボード行列の計算
	// カメラのワールド行列の回転成分を取り出すことで、常にカメラの方を向く行列を作れます
	Matrix4x4 matBillboard = camera->GetWorldMatrix();
	// 平行移動成分は除去する（回転のみ利用するため）
	matBillboard.m[3][0] = 0.0f;
	matBillboard.m[3][1] = 0.0f;
	matBillboard.m[3][2] = 0.0f;
	matBillboard.m[3][3] = 1.0f;

	// --- GPU Particle 追加分 ---
	MyMath myMath;
	// カメラ情報の書き込み
	cameraDataPtr_->viewProjection = myMath.Multiply(matView, matProjection);
	cameraDataPtr_->billboard = matBillboard;

	// CPUでのEmitterの更新
	const float kDeltaTime = 1.0f / 60.0f;
	emitterDataPtr_->frequencyTime += kDeltaTime;
	if (emitterDataPtr_->frequency <= emitterDataPtr_->frequencyTime) {
		emitterDataPtr_->frequencyTime -= emitterDataPtr_->frequency;
		emitterDataPtr_->emit = 1;
	} else {
		emitterDataPtr_->emit = 0;
	}

	// PerFrameの更新
	perFrameDataPtr_->time += kDeltaTime;
	perFrameDataPtr_->deltaTime = kDeltaTime;

	// 再初期化リクエストの処理
	if (requestGpuInitialize) {
		for (auto& [name, group] : particleGroups_) {
			group.isGpuInitialized = false;
		}
		requestGpuInitialize = false;
	}


	// 3. 全てのパーティクルグループについて処理する (スライド)
	// particleGroups は std::unordered_map<std::string, ParticleGroup> と想定
	for (auto &[name, group] : particleGroups_) {

		uint32_t numInstance = 0; // 今回描画するインスタンス数

		MyMath myMath; // 行列計算用インスタンス

		// 4. グループ内の全てのパーティクルについて処理する (スライド)
		// std::list なので、途中で削除(erase)しても良いようにイテレータを使う
		for (auto it = group.particle.begin(); it != group.particle.end(); ) {

			// ■ 寿命に達していたらグループから外す (スライド)
			if (it->currentTime >= it->lifeTime) {
				it = group.particle.erase(it); // 削除して次のイテレータを受け取る
				continue;                       // 次のループへ
			}

			// ■ 場の影響を計算（加速） (スライド)
			// 例: 重力を加える場合など (今回は省略可、必要なら以下のように記述)
			// it->velocity.y -= 0.01f; 

			// ■ 移動処理（速度を座標に加算） (スライド)
			it->transform.translate.x += it->velocity.x;
			it->transform.translate.y += it->velocity.y;
			it->transform.translate.z += it->velocity.z;
			it->transform.rotate.z += it->angularVelocity;

			// ■ 経過時間を加算 (スライド)
			const float kDeltaTime = 1.0f / 60.0f; // 固定60fps想定
			it->currentTime += kDeltaTime;
			float lifeProgress = 1.0f;
			if (it->lifeTime > 0.0f) {
				lifeProgress = (std::min)(it->currentTime / it->lifeTime, 1.0f);
			}
			const float currentScale =
				it->initialScale + (it->finalScale - it->initialScale) * lifeProgress;
			it->transform.scale = { currentScale, currentScale, 1.0f };


			// ■ インスタンシング用データ1個分の書き込み (スライド)
			// インスタンス数が最大数を超えない範囲で書き込む
			if (numInstance < group.kNumMaxInstance) {
				// ■ ワールド行列を計算 (スライド)
				// ビルボード行列を使うため、回転計算は matBillboard を使用
				// 手順: スケール -> 回転(ビルボード) -> 平行移動
				Matrix4x4 scaleMat = myMath.MkeScaleMatrix(it->transform.scale);
				Matrix4x4 translateMat = myMath.MakeTranslateMatrix(it->transform.translate);

				// Z軸の回転行列を作成
				// （MyMathに MakeRotateZMatrix が無ければ、直接計算して作ります）
				Matrix4x4 rotateZMat = myMath.MakeIdentity4x4();
				rotateZMat.m[0][0] = std::cos(it->transform.rotate.z);
				rotateZMat.m[0][1] = std::sin(it->transform.rotate.z);
				rotateZMat.m[1][0] = -std::sin(it->transform.rotate.z);
				rotateZMat.m[1][1] = std::cos(it->transform.rotate.z);

				// 行列の合成 (Scale * Billboard * Translate)
				Matrix4x4 worldMatrix = myMath.Multiply(scaleMat, rotateZMat);
				worldMatrix = myMath.Multiply(worldMatrix, matBillboard);
				worldMatrix = myMath.Multiply(worldMatrix, translateMat);

				// ■ ワールドビュープロジェクション行列を合成 (スライド)
				Matrix4x4 worldViewProjection = myMath.Multiply(worldMatrix, myMath.Multiply(matView, matProjection));

				group.instancingDataPtr[numInstance].WVP = worldViewProjection;
				group.instancingDataPtr[numInstance].World = worldMatrix;
				group.instancingDataPtr[numInstance].color = it->color;

				// 寿命に近づくにつれて alpha (透明度) を 1.0 から 0.0 へ減らす
				float alpha = 1.0f;
				if (it->lifeTime > 0.0f) {
					alpha = 1.0f - (it->currentTime / it->lifeTime);
					if (alpha < 0.0f) {
						alpha = 0.0f;
					} else if (alpha > 1.0f) {
						alpha = 1.0f;
					}
				}
				group.instancingDataPtr[numInstance].color.w = alpha;

				numInstance++; // 書き込んだ件数をカウント
			}

			++it; // 次のパーティクルへ
		}

		// ※描画時に「何個描画するか」が必要になるので、ここで記録しておくと便利です
		// group.numActiveInstances = numInstance; 
	}
}


void ParticleManager::Draw() {
	// 1. コマンドリストの取得
	ID3D12GraphicsCommandList *commandList = dxCommon_->GetCommandList();

	// 2. パイプラインステート等の共通設定
	// これから使うルートシグネチャとPSOをセット
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());

	// プリミティブ形状の設定（トライアングルストリップ）
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// 3. グループごとの描画ループ
	for (auto &[name, group] : particleGroups_) {

		// --- GPU Particle 初回初期化および更新 ---
		if (kEnableGpuParticleSimulation && group.particleResource) {
			commandList->SetComputeRootSignature(computeRootSignature_.Get());

			ID3D12DescriptorHeap *descriptorHeaps[] = { SrvManager::GetInstance()->GetDescriptorHeap() };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			if (!group.isGpuInitialized) {
				D3D12_RESOURCE_BARRIER barriersToUav[3] = {
					CD3DX12_RESOURCE_BARRIER::Transition(group.particleResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
					CD3DX12_RESOURCE_BARRIER::Transition(group.freeListIndexResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
					CD3DX12_RESOURCE_BARRIER::Transition(group.freeListResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				};
				commandList->ResourceBarrier(3, barriersToUav);

				// 初期化CSの実行
				commandList->SetPipelineState(computePipelineStateInit_.Get());
				SrvManager::GetInstance()->SetComputeRootDescriptorTable(0, group.uavIndex);
				SrvManager::GetInstance()->SetComputeRootDescriptorTable(1, group.freeListIndexUavIndex);
				SrvManager::GetInstance()->SetComputeRootDescriptorTable(2, group.freeListUavIndex);
				commandList->SetComputeRootConstantBufferView(3, emitterResource_->GetGPUVirtualAddress());
				commandList->SetComputeRootConstantBufferView(4, perFrameResource_->GetGPUVirtualAddress());
				commandList->Dispatch(1, 1, 1);
			} else {
				D3D12_RESOURCE_BARRIER barriersToUav[3] = {
					CD3DX12_RESOURCE_BARRIER::Transition(group.particleResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
					CD3DX12_RESOURCE_BARRIER::Transition(group.freeListIndexResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
					CD3DX12_RESOURCE_BARRIER::Transition(group.freeListResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				};
				commandList->ResourceBarrier(3, barriersToUav);
			}

			// 射出CSの実行
			commandList->SetPipelineState(computePipelineStateEmit_.Get());
			SrvManager::GetInstance()->SetComputeRootDescriptorTable(0, group.uavIndex);
			SrvManager::GetInstance()->SetComputeRootDescriptorTable(1, group.freeListIndexUavIndex);
			SrvManager::GetInstance()->SetComputeRootDescriptorTable(2, group.freeListUavIndex);
			commandList->SetComputeRootConstantBufferView(3, emitterResource_->GetGPUVirtualAddress());
			commandList->SetComputeRootConstantBufferView(4, perFrameResource_->GetGPUVirtualAddress());
			commandList->Dispatch(1, 1, 1);

			// UAVバリア (Emit -> Update間の依存関係を解消)
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.UAV.pResource = group.particleResource.Get();
			commandList->ResourceBarrier(1, &barrier);

			// 更新CSの実行
			commandList->SetPipelineState(computePipelineStateUpdate_.Get());
			// RootParameterはEmitと同じものを使うので、再設定は不要
			commandList->Dispatch(1, 1, 1);

			// バリアを張ってから描画へ (UAV -> SRV)
			D3D12_RESOURCE_BARRIER barriersToSrv[3] = {
				CD3DX12_RESOURCE_BARRIER::Transition(group.particleResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(group.freeListIndexResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(group.freeListResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			};
			commandList->ResourceBarrier(3, barriersToSrv);
			group.isGpuInitialized = true;

			// 描画用のパイプラインに戻す
			commandList->SetGraphicsRootSignature(rootSignature_.Get());
			commandList->SetPipelineState(graphicsPipelineState_.Get());
		}

		// ==========================================
		// 1. CPUパーティクル（今回の爆発エフェクト）の描画
		// ==========================================
		UINT cpuInstanceCount = static_cast<UINT>(group.particle.size());
		if (cpuInstanceCount > group.kNumMaxInstance) {
			cpuInstanceCount = group.kNumMaxInstance;
		}

		if (cpuInstanceCount > 0) {
			// CPU用のパイプラインステートをセットして描画
			commandList->SetPipelineState(graphicsPipelineStateCPU_.Get());
			// CPU用のリソースをバインドして描画
			SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, group.textureSrvIndex);
			SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, group.instancingSrvIndex);
			commandList->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());

			commandList->DrawInstanced(4, cpuInstanceCount, 0, 0);
		}

		// ==========================================
		// 2. GPUパーティクル（ImGuiで操作している方）の描画
		// ==========================================
		//if (group.particleResource) {
		//	 GPU用のリソースをバインドして描画
		//	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, group.textureSrvIndex);
		//	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, group.srvIndex);
		//	commandList->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());

		//	commandList->DrawInstanced(4, 1024, 0, 0); // GPUは常に1024個
		//}
	}
}

void ParticleManager::CreateParticleGroup(const std::string name, const std::string textureFilePath) {
	//登録済みの名前なら何もしない
	if (particleGroups_.contains(name)) {
		return;
	}

	//新しいグループを作成
	ParticleGroup group;
	group.kNumMaxInstance = 1000;// このグループの最大パーティクル数

	// テクスチャの読み込み & SRVインデックス取得
	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	group.textureFilePath = textureFilePath;
	// テクスチャのSRVインデックスを取得 (TextureManagerの実装に合わせて書き換えてください)
	group.textureSrvIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);


	// 2. インスタンシング用リソース (StructuredBuffer) の生成
	// GPUに送るデータ(ParticleForGPU)を kNumMaxInstance 個分確保する
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ParticleForGPU) * group.kNumMaxInstance);

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&group.instancingResource)
	);
	assert(SUCCEEDED(hr));


	// 3. データを書き込むためのメモリをマッピング
	hr = group.instancingResource->Map(0, nullptr, reinterpret_cast<void **>(&group.instancingDataPtr));
	assert(SUCCEEDED(hr));
	// 初期化（念のためゴミが入らないようにゼロ埋めしても良い）
	std::memset(group.instancingDataPtr, 0, sizeof(ParticleForGPU) * group.kNumMaxInstance);


	// 4. StructuredBuffer用のSRVを作成
	// SrvManagerを使って空いているSRV番号を確保する
	group.instancingSrvIndex = SrvManager::GetInstance()->Allocate();

	SrvManager::GetInstance()->CreateSRVforStructuredBuffer(
		group.instancingSrvIndex,
		group.instancingResource.Get(),
		group.kNumMaxInstance,
		sizeof(ParticleForGPU)
	);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = group.kNumMaxInstance;
	srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;


	// 確保した場所にSRVを作る
	dxCommon_->GetDevice()->CreateShaderResourceView(
		group.instancingResource.Get(),
		&srvDesc,
		SrvManager::GetInstance()->GetCPUDescriptorHandle(group.instancingSrvIndex)
	);


	// 5. マップに登録
	particleGroups_[name] = group;

	// --- GPU Particle 追加分 ---
	// 1. Resource作成 (DEFAULT)
	D3D12_HEAP_PROPERTIES defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC particleResDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ParticleCS) * 1024);
	particleResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&particleResDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&particleGroups_[name].particleResource)
	);
	assert(SUCCEEDED(hr));

	// 2. View作成 (UAV & SRV)
	particleGroups_[name].uavIndex = SrvManager::GetInstance()->Allocate();
	particleGroups_[name].srvIndex = SrvManager::GetInstance()->Allocate();

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1024;
	uavDesc.Buffer.StructureByteStride = sizeof(ParticleCS);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		particleGroups_[name].particleResource.Get(),
		nullptr,
		&uavDesc,
		SrvManager::GetInstance()->GetCPUDescriptorHandle(particleGroups_[name].uavIndex)
	);

	D3D12_SHADER_RESOURCE_VIEW_DESC gpuSrvDesc{};
	gpuSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	gpuSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	gpuSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	gpuSrvDesc.Buffer.FirstElement = 0;
	gpuSrvDesc.Buffer.NumElements = 1024;
	gpuSrvDesc.Buffer.StructureByteStride = sizeof(ParticleCS);
	gpuSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	dxCommon_->GetDevice()->CreateShaderResourceView(
		particleGroups_[name].particleResource.Get(),
		&gpuSrvDesc,
		SrvManager::GetInstance()->GetCPUDescriptorHandle(particleGroups_[name].srvIndex)
	);

	// freeListIndexResourceの作成
	D3D12_RESOURCE_DESC indexResDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(int32_t));
	indexResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&indexResDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&particleGroups_[name].freeListIndexResource)
	);
	assert(SUCCEEDED(hr));

	particleGroups_[name].freeListIndexUavIndex = SrvManager::GetInstance()->Allocate();

	D3D12_UNORDERED_ACCESS_VIEW_DESC indexUavDesc{};
	indexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	indexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	indexUavDesc.Buffer.FirstElement = 0;
	indexUavDesc.Buffer.NumElements = 1;
	indexUavDesc.Buffer.StructureByteStride = sizeof(int32_t);
	indexUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		particleGroups_[name].freeListIndexResource.Get(),
		nullptr,
		&indexUavDesc,
		SrvManager::GetInstance()->GetCPUDescriptorHandle(particleGroups_[name].freeListIndexUavIndex)
	);

	// freeListResourceの作成
	D3D12_RESOURCE_DESC listResDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * 1024);
	listResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&listResDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&particleGroups_[name].freeListResource)
	);
	assert(SUCCEEDED(hr));

	particleGroups_[name].freeListUavIndex = SrvManager::GetInstance()->Allocate();

	D3D12_UNORDERED_ACCESS_VIEW_DESC listUavDesc{};
	listUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	listUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	listUavDesc.Buffer.FirstElement = 0;
	listUavDesc.Buffer.NumElements = 1024;
	listUavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
	listUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		particleGroups_[name].freeListResource.Get(),
		nullptr,
		&listUavDesc,
		SrvManager::GetInstance()->GetCPUDescriptorHandle(particleGroups_[name].freeListUavIndex)
	);
}

void ParticleManager::Emit(const std::string name, const Vector3 &position, uint32_t count, const Vector4 &color,
	float speed, float speedVariance,
	float scale, float scaleVariance,
	float lifeTimeMin, float lifeTimeMax,
	float posVariance) {
	// 1. 登録済みのパーティクルグループを検索
	if (particleGroups_.find(name) == particleGroups_.end()) {
		return;
	}

	// グループの参照を取得
	ParticleGroup &group = particleGroups_[name];
	const uint32_t emitCount = ClampEmitCountToCapacity(group, count);
	if (emitCount == 0) {
		return;
	}

	// 2. ランダム生成器の準備
	static std::random_device seed_gen;
	static std::mt19937_64 engine(seed_gen());

	// ランダムの分布設定
	constexpr float kPi = 3.14159265f;
	std::uniform_real_distribution<float> distZ(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distAngle(0.0f, kPi * 2.0f);
	std::uniform_real_distribution<float> distRotate(-3.14159265f, 3.14159265f);
	std::uniform_real_distribution<float> distAngularSpeed(0.06f, 0.16f);
	std::uniform_int_distribution<int> distSpinDirection(0, 1);
	std::uniform_real_distribution<float> distScale(scale - scaleVariance, scale + scaleVariance);
	std::uniform_real_distribution<float> distRadius(posVariance * 0.4f, posVariance);
	std::uniform_real_distribution<float> distLife(lifeTimeMin, lifeTimeMax);
	std::uniform_real_distribution<float> distSpeed(speed - speedVariance, speed + speedVariance);

	// 3. 指定された数だけパーティクルを生成
	for (uint32_t i = 0; i < emitCount; ++i) {
		Particle newParticle{};

		// ==========================================
		// ■ Transform (位置・スケール・回転)
		// ==========================================
		// スケールを縦横同じにして「真円」にする
		float randomScale = distScale(engine);
		newParticle.initialScale = randomScale;
		newParticle.finalScale = 0.0f;
		newParticle.transform.scale = { randomScale, randomScale, 1.0f };

		newParticle.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
		const float spinDirection = distSpinDirection(engine) == 0 ? -1.0f : 1.0f;
		newParticle.angularVelocity = distAngularSpeed(engine) * spinDirection;

		// 球面上の一様な方向を作り、初期位置と飛ぶ方向をそろえる
		const float z = distZ(engine);
		const float angle = distAngle(engine);
		const float xyLength = std::sqrt(1.0f - z * z);
		const Vector3 dir = {
			xyLength * std::cos(angle),
			xyLength * std::sin(angle),
			z
		};
		const float spawnRadius = distRadius(engine);
		newParticle.transform.translate = {
			position.x + dir.x * spawnRadius,
			position.y + dir.y * spawnRadius,
			position.z + dir.z * spawnRadius
		};

		// ==========================================
		// ■ Velocity (速度)
		// ==========================================
		// 粒ごとに少しスピードにばらつきを持たせる
		float kSpeed = distSpeed(engine);
		newParticle.velocity = {
			dir.x * kSpeed,
			dir.y * kSpeed,
			dir.z * kSpeed
		};

		// ==========================================
		// ■ Color (色) と Lifetime (寿命)
		// ==========================================
		// ★修正4：引数で受け取った色（オレンジなど）を適用する！
		newParticle.color = color;

		newParticle.lifeTime = distLife(engine);
		newParticle.currentTime = 0.0f; // 経過時間は0からスタート

		// リストに追加
		group.particle.push_back(newParticle);
	}
}

void ParticleManager::EmitDirectionalBurst(const std::string name, const Vector3 &position, const Vector3 &direction,
	uint32_t count, const Vector4 &color, float spreadRadians,
	float speed, float speedVariance,
	float scale, float scaleVariance,
	float lifeTimeMin, float lifeTimeMax,
	float posVariance) {
	if (particleGroups_.find(name) == particleGroups_.end()) {
		return;
	}

	ParticleGroup &group = particleGroups_[name];
	const uint32_t emitCount = ClampEmitCountToCapacity(group, count);
	if (emitCount == 0) {
		return;
	}

	const float directionLength = std::sqrt(
		direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
	if (directionLength <= 0.0001f) {
		return;
	}

	const Vector3 forward = {
		direction.x / directionLength,
		direction.y / directionLength,
		direction.z / directionLength,
	};
	const Vector3 referenceAxis = std::abs(forward.y) > 0.92f
		? Vector3{ 1.0f, 0.0f, 0.0f }
		: Vector3{ 0.0f, 1.0f, 0.0f };
	Vector3 right = {
		referenceAxis.y * forward.z - referenceAxis.z * forward.y,
		referenceAxis.z * forward.x - referenceAxis.x * forward.z,
		referenceAxis.x * forward.y - referenceAxis.y * forward.x,
	};
	const float rightLength = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
	right = { right.x / rightLength, right.y / rightLength, right.z / rightLength };
	const Vector3 up = {
		forward.y * right.z - forward.z * right.y,
		forward.z * right.x - forward.x * right.z,
		forward.x * right.y - forward.y * right.x,
	};

	static std::random_device seedGenerator;
	static std::mt19937_64 engine(seedGenerator());
	constexpr float kPi = 3.14159265f;
	const float clampedSpread = std::clamp(spreadRadians, 0.0f, 1.50f);
	std::uniform_real_distribution<float> distCosTheta(std::cos(clampedSpread), 1.0f);
	std::uniform_real_distribution<float> distAngle(0.0f, kPi * 2.0f);
	std::uniform_real_distribution<float> distSpeed(speed - speedVariance, speed + speedVariance);
	std::uniform_real_distribution<float> distScale(scale - scaleVariance, scale + scaleVariance);
	std::uniform_real_distribution<float> distLife(lifeTimeMin, lifeTimeMax);
	std::uniform_real_distribution<float> distRadius(0.0f, posVariance);
	std::uniform_real_distribution<float> distRotate(-kPi, kPi);
	std::uniform_real_distribution<float> distAngularSpeed(0.08f, 0.22f);
	std::uniform_int_distribution<int> distSpinDirection(0, 1);

	for (uint32_t i = 0; i < emitCount; ++i) {
		const float cosTheta = distCosTheta(engine);
		const float sinTheta = std::sqrt((std::max)(0.0f, 1.0f - cosTheta * cosTheta));
		const float angle = distAngle(engine);
		const Vector3 burstDirection = {
			forward.x * cosTheta + right.x * std::cos(angle) * sinTheta + up.x * std::sin(angle) * sinTheta,
			forward.y * cosTheta + right.y * std::cos(angle) * sinTheta + up.y * std::sin(angle) * sinTheta,
			forward.z * cosTheta + right.z * std::cos(angle) * sinTheta + up.z * std::sin(angle) * sinTheta,
		};

		Particle particle{};
		const float particleScale = (std::max)(0.0f, distScale(engine));
		particle.initialScale = particleScale;
		particle.finalScale = 0.0f;
		particle.transform.scale = { particleScale, particleScale, 1.0f };
		particle.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
		particle.angularVelocity = distAngularSpeed(engine) * (distSpinDirection(engine) == 0 ? -1.0f : 1.0f);

		const float spawnRadius = distRadius(engine);
		particle.transform.translate = {
			position.x + burstDirection.x * spawnRadius,
			position.y + burstDirection.y * spawnRadius,
			position.z + burstDirection.z * spawnRadius,
		};
		const float particleSpeed = (std::max)(0.0f, distSpeed(engine));
		particle.velocity = {
			burstDirection.x * particleSpeed,
			burstDirection.y * particleSpeed,
			burstDirection.z * particleSpeed,
		};
		particle.color = color;
		particle.lifeTime = (std::max)(0.01f, distLife(engine));
		particle.currentTime = 0.0f;
		group.particle.push_back(particle);
	}
}

void ParticleManager::EmitFireball(const std::string name, const Vector3 &position, uint32_t count,
	const Vector4 &color, float initialScale, float finalScale, float lifeTime, float radius) {
	if (particleGroups_.find(name) == particleGroups_.end()) {
		return;
	}

	ParticleGroup &group = particleGroups_[name];
	const uint32_t emitCount = ClampEmitCountToCapacity(group, count);
	if (emitCount == 0) {
		return;
	}

	static std::random_device seedGenerator;
	static std::mt19937_64 engine(seedGenerator());

	constexpr float kPi = 3.14159265f;
	std::uniform_real_distribution<float> distZ(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distAngle(0.0f, kPi * 2.0f);
	std::uniform_real_distribution<float> distRadius(0.0f, radius);
	std::uniform_real_distribution<float> distScale(0.85f, 1.15f);
	std::uniform_real_distribution<float> distLife(0.9f, 1.1f);
	std::uniform_real_distribution<float> distRotate(-kPi, kPi);
	std::uniform_real_distribution<float> distAngularSpeed(0.04f, 0.11f);
	std::uniform_int_distribution<int> distSpinDirection(0, 1);

	for (uint32_t i = 0; i < emitCount; ++i) {
		const float z = distZ(engine);
		const float angle = distAngle(engine);
		const float xyLength = std::sqrt(1.0f - z * z);
		const Vector3 dir = {
			xyLength * std::cos(angle),
			xyLength * std::sin(angle),
			z
		};
		const float spawnRadius = distRadius(engine);
		const float scaleFactor = distScale(engine);

		Particle particle{};
		particle.transform.translate = {
			position.x + dir.x * spawnRadius,
			position.y + dir.y * spawnRadius,
			position.z + dir.z * spawnRadius
		};
		particle.velocity = { dir.x * 0.004f, dir.y * 0.004f, dir.z * 0.004f };
		particle.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
		particle.angularVelocity =
			distAngularSpeed(engine) * (distSpinDirection(engine) == 0 ? -1.0f : 1.0f);
		particle.initialScale = initialScale * scaleFactor;
		particle.finalScale = finalScale * scaleFactor;
		particle.transform.scale = { particle.initialScale, particle.initialScale, 1.0f };
		particle.color = color;
		particle.lifeTime = lifeTime * distLife(engine);
		particle.currentTime = 0.0f;

		group.particle.push_back(particle);
	}
}

void ParticleManager::CreateRootSignature() {
	HRESULT hr = S_FALSE;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//RootParameter作成。複製設定できるので配列
	D3D12_ROOT_PARAMETER rootParameters[5] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;           //レジスタ番号0とバインド

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	// --- GPU Particle 追加分 ---
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[4].Descriptor.ShaderRegister = 0; // b0
	
	descriptionRootSignature.pParameters = rootParameters;//ルートパラメータ配列のポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	//シリアライズしてバイナルにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		std::string errorMsg = reinterpret_cast<char *>(errorBlob->GetBufferPointer());
		logger.Log(errorMsg);
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_)
	);
	assert(SUCCEEDED(hr));
}

void ParticleManager::CreatePipelineState() {
	HRESULT hr = S_FALSE;

	//dxcCompilerを初期化
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils = nullptr;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	//現時点でincludはしないが、includeに対応するための設定を行っておく
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));



	//RasiterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//シェーダーのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	vertexShaderBlob = CompileShader(L"resources/shaders/Particle.VS.hlsl",
		L"vs_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get());
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
	pixelShaderBlob = CompileShader(L"resources/shaders/Particle.PS.hlsl",
		L"ps_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get());
	assert(pixelShaderBlob != nullptr);


	//----------------頂点レイアウト----------------
	//InputLayoutの作成
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//----------------パイプラインステートの設定----------------
	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;

	//PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	//RootSignature
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	//InputLayout
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	//VertexShader
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };
	//PixelShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };

	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	// RGBのみ書き込む（アルファチャンネルへの書き込みを無効化し、ウィンドウ透過による黒ずみを防ぐ）
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	//BlendState
	graphicsPipelineStateDesc.BlendState = blendDesc;
	//RasterizerState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	//利用するトポロジのタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	//----------------生成----------------
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	if (FAILED(hr)) {
		logger.Log("Failed to create GraphicsPipelineState for Particle.\n");
		assert(false);
	}

	// CPUパーティクル用のパイプラインステートを作成
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderCPUBlob;
	vertexShaderCPUBlob = CompileShader(L"resources/shaders/ParticleCPU.VS.hlsl",
		L"vs_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get());
	assert(vertexShaderCPUBlob != nullptr);

	graphicsPipelineStateDesc.VS = { vertexShaderCPUBlob->GetBufferPointer(),
	vertexShaderCPUBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStateCPU_));
	if (FAILED(hr)) {
		logger.Log("Failed to create GraphicsPipelineState for CPU Particle.\n");
		assert(false);
	}
}

void ParticleManager::CreateComputeRootSignature() {
	D3D12_DESCRIPTOR_RANGE descriptorRangeU0[1] = {};
	descriptorRangeU0[0].BaseShaderRegister = 0;
	descriptorRangeU0[0].NumDescriptors = 1;
	descriptorRangeU0[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorRangeU0[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeU1[1] = {};
	descriptorRangeU1[0].BaseShaderRegister = 1;
	descriptorRangeU1[0].NumDescriptors = 1;
	descriptorRangeU1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorRangeU1[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeU2[1] = {};
	descriptorRangeU2[0].BaseShaderRegister = 2;
	descriptorRangeU2[0].NumDescriptors = 1;
	descriptorRangeU2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorRangeU2[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[5] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangeU0;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeU0);

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeU1;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeU1);

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeU2;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeU2);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].Descriptor.ShaderRegister = 0;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[4].Descriptor.ShaderRegister = 1;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	ComPtr<ID3DBlob> signatureBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		logger.Log(reinterpret_cast<char *>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_));
	assert(SUCCEEDED(hr));
}

void ParticleManager::CreateComputePipelineState() {
	ComPtr<IDxcUtils> dxcUtils = nullptr;
	ComPtr<IDxcCompiler3> dxcCompiler = nullptr;
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	ComPtr<IDxcIncludeHandler> includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

	ComPtr<IDxcBlob> computeShaderInitBlob = CompileShader(L"resources/shaders/InitializeParticle.CS.hlsl", L"cs_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get());
	assert(computeShaderInitBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateInitDesc{};
	computePipelineStateInitDesc.pRootSignature = computeRootSignature_.Get();
	computePipelineStateInitDesc.CS = { computeShaderInitBlob->GetBufferPointer(), computeShaderInitBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&computePipelineStateInitDesc, IID_PPV_ARGS(&computePipelineStateInit_));
	assert(SUCCEEDED(hr));

	ComPtr<IDxcBlob> computeShaderEmitBlob = CompileShader(L"resources/shaders/EmitParticle.CS.hlsl", L"cs_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get());
	assert(computeShaderEmitBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateEmitDesc{};
	computePipelineStateEmitDesc.pRootSignature = computeRootSignature_.Get();
	computePipelineStateEmitDesc.CS = { computeShaderEmitBlob->GetBufferPointer(), computeShaderEmitBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&computePipelineStateEmitDesc, IID_PPV_ARGS(&computePipelineStateEmit_));
	assert(SUCCEEDED(hr));

	ComPtr<IDxcBlob> computeShaderUpdateBlob = CompileShader(L"resources/shaders/UpdateParticle.CS.hlsl", L"cs_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get());
	assert(computeShaderUpdateBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateUpdateDesc{};
	computePipelineStateUpdateDesc.pRootSignature = computeRootSignature_.Get();
	computePipelineStateUpdateDesc.CS = { computeShaderUpdateBlob->GetBufferPointer(), computeShaderUpdateBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&computePipelineStateUpdateDesc, IID_PPV_ARGS(&computePipelineStateUpdate_));
	assert(SUCCEEDED(hr));
}
