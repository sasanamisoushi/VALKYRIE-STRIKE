#include "ParticleManager.h"
#include "ParticleBurstEmitter.h"
#include "ParticlePipelineBuilder.h"
#include <cassert>
#include "Logger.h"
#include "engine/Resource/TextureManager.h"
#include "engine/Graphics/SrvManager.h"
#include "engine/base/StringUtility.h"
#include <algorithm>

#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;

namespace {
constexpr bool kEnableGpuParticleSimulation = false;
}

ParticleManager::~ParticleManager() {
	ReleaseParticleGroupDescriptors();
}

void ParticleManager::ReleaseParticleGroupDescriptors() {
	SrvManager* srvManager = SrvManager::GetInstance();
	for (auto& [name, group] : particleGroups_) {
		auto release = [srvManager](uint32_t& index) {
			if (index != UINT32_MAX) {
				srvManager->Release(index);
				index = UINT32_MAX;
			}
		};
		release(group.instancingSrvIndex);
		release(group.uavIndex);
		release(group.srvIndex);
		release(group.freeListIndexUavIndex);
		release(group.freeListUavIndex);
	}
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
			if (it->velocityDrag > 0.0f) {
				const float drag = (std::clamp)(it->velocityDrag, 0.0f, 0.95f);
				it->velocity.x *= 1.0f - drag;
				it->velocity.y *= 1.0f - drag;
				it->velocity.z *= 1.0f - drag;
			}
			it->velocity.y += it->upwardAcceleration;

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
			if (it->useColorTransition) {
				const float transitionRange = (std::max)(0.001f, 1.0f - it->colorTransitionStart);
				const float transition = (std::clamp)(
					(lifeProgress - it->colorTransitionStart) / transitionRange, 0.0f, 1.0f);
				// 滑らかな補間にして、炎の色が急に灰色へ切り替わらないようにする。
				const float smoothTransition = transition * transition * (3.0f - 2.0f * transition);
				it->color = {
					it->startColor.x + (it->endColor.x - it->startColor.x) * smoothTransition,
					it->startColor.y + (it->endColor.y - it->startColor.y) * smoothTransition,
					it->startColor.z + (it->endColor.z - it->startColor.z) * smoothTransition,
					it->startColor.w + (it->endColor.w - it->startColor.w) * smoothTransition,
				};
			}


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
				// 粒子固有の不透明度を保ったまま寿命でフェードさせる。
				group.instancingDataPtr[numInstance].color.w = it->color.w * alpha;

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
			commandList->SetPipelineState(group.useAlphaBlend
				? graphicsPipelineStateCPUAlpha_.Get()
				: graphicsPipelineStateCPU_.Get());
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

void ParticleManager::SetParticleGroupAlphaBlend(const std::string& name, bool enabled) {
	auto it = particleGroups_.find(name);
	if (it != particleGroups_.end()) {
		it->second.useAlphaBlend = enabled;
	}
}

void ParticleManager::ClearParticleGroup(const std::string& name) {
	auto it = particleGroups_.find(name);
	if (it != particleGroups_.end()) {
		it->second.particle.clear();
	}
}

std::size_t ParticleManager::GetParticleCount(const std::string& name) const {
	const auto it = particleGroups_.find(name);
	return it != particleGroups_.end() ? it->second.particle.size() : 0;
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
	particleGroups_[name] = std::move(group);

	// CPUパーティクルだけを使う構成では、GPUシミュレーション用の4種の
	// リソース／ディスクリプタを確保しない。これらは Draw でも参照されない。
	if (!kEnableGpuParticleSimulation) {
		return;
	}

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
	auto it = particleGroups_.find(name);
	if (it == particleGroups_.end()) return;
	ParticleBurstEmitter{}.Emit(it->second, position, count, color,
		speed, speedVariance, scale, scaleVariance, lifeTimeMin, lifeTimeMax, posVariance);
}

void ParticleManager::EmitDirectionalBurst(const std::string name, const Vector3 &position, const Vector3 &direction,
	uint32_t count, const Vector4 &color, float spreadRadians,
	float speed, float speedVariance,
	float scale, float scaleVariance,
	float lifeTimeMin, float lifeTimeMax,
	float posVariance) {
	auto it = particleGroups_.find(name);
	if (it == particleGroups_.end()) return;
	ParticleBurstEmitter{}.EmitDirectionalBurst(it->second, position, direction, count, color, spreadRadians,
		speed, speedVariance, scale, scaleVariance, lifeTimeMin, lifeTimeMax, posVariance);
}

void ParticleManager::EmitInwardBurst(const std::string name, const Vector3& position, uint32_t count,
	const Vector4& color, float innerRadius, float outerRadius,
	float speed, float speedVariance, float scale, float scaleVariance,
	float lifeTimeMin, float lifeTimeMax) {
	auto it = particleGroups_.find(name);
	if (it == particleGroups_.end()) return;
	ParticleBurstEmitter{}.EmitInwardBurst(it->second, position, count, color, innerRadius, outerRadius,
		speed, speedVariance, scale, scaleVariance, lifeTimeMin, lifeTimeMax);
}

void ParticleManager::EmitSmoke(const std::string& name, const Vector3& position, uint32_t count, const Vector4& color,
	float initialScale, float finalScale,
	float riseSpeed, float spreadSpeed,
	float lifeTimeMin, float lifeTimeMax,
	float posVariance) {
	auto it = particleGroups_.find(name);
	if (it == particleGroups_.end()) return;
	ParticleBurstEmitter{}.EmitSmoke(it->second, position, count, color,
		initialScale, finalScale, riseSpeed, spreadSpeed, lifeTimeMin, lifeTimeMax, posVariance);
}

void ParticleManager::EmitFireball(const std::string name, const Vector3 &position, uint32_t count,
	const Vector4 &color, float initialScale, float finalScale, float lifeTime, float radius) {
	auto it = particleGroups_.find(name);
	if (it == particleGroups_.end()) return;
	ParticleBurstEmitter{}.EmitFireball(it->second, position, count, color, initialScale, finalScale, lifeTime, radius);
}

void ParticleManager::EmitFlameToSmoke(const std::string& name, const Vector3& position, uint32_t count,
	const Vector4& flameColor, const Vector4& smokeColor,
	float initialScale, float finalScale,
	float speed, float speedVariance,
	float lifeTimeMin, float lifeTimeMax,
	float posVariance, float transitionStart) {
	auto it = particleGroups_.find(name);
	if (it == particleGroups_.end()) return;
	ParticleBurstEmitter{}.EmitFlameToSmoke(it->second, position, count, flameColor, smokeColor,
		initialScale, finalScale, speed, speedVariance, lifeTimeMin, lifeTimeMax, posVariance, transitionStart);
}

void ParticleManager::EmitCustom(const std::string& name, const CustomParticleParams& params) {
	auto it = particleGroups_.find(name);
	if (it == particleGroups_.end()) return;
	ParticleBurstEmitter{}.EmitCustom(it->second, params);
}

void ParticleManager::CreateRootSignature() {
	ParticlePipelineBuilder::CreateGraphicsRootSignature(dxCommon_->GetDevice(), rootSignature_);
}

void ParticleManager::CreatePipelineState() {
	ParticlePipelineBuilder::CreateGraphicsPipelineStates(
		dxCommon_->GetDevice(), rootSignature_.Get(),
		graphicsPipelineState_, graphicsPipelineStateCPU_, graphicsPipelineStateCPUAlpha_);
}

void ParticleManager::CreateComputeRootSignature() {
	ParticlePipelineBuilder::CreateComputeRootSignature(dxCommon_->GetDevice(), computeRootSignature_);
}

void ParticleManager::CreateComputePipelineState() {
	ParticlePipelineBuilder::CreateComputePipelineStates(
		dxCommon_->GetDevice(), computeRootSignature_.Get(),
		computePipelineStateInit_, computePipelineStateEmit_, computePipelineStateUpdate_);
}
