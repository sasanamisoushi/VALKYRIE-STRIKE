#include "Game.h"
#include "3D/ModelManager.h"
#include "engine/Audio/AudioManager.h"
#include "engine/Graphics/SrvManager.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Camera/FlyCamera.h"
#include <engine/Resource/TextureManager.h>
#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>

namespace {
	bool ShouldStartSimulationScene() {
		const wchar_t *commandLine = GetCommandLineW();
		if (commandLine && std::wstring(commandLine).find(L"--simulation") != std::wstring::npos) {
			return true;
		}

		wchar_t modulePath[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (length == 0) {
			return false;
		}

		std::wstring exeName = std::filesystem::path(modulePath).stem().wstring();
		std::transform(exeName.begin(), exeName.end(), exeName.begin(), [](wchar_t c) {
			return static_cast<wchar_t>(std::towlower(c));
		});
		return exeName.find(L"simulation") != std::wstring::npos;
	}

	bool ShouldStartCodexDemoScene() {
		const wchar_t *commandLine = GetCommandLineW();
		if (commandLine && std::wstring(commandLine).find(L"--codex-demo") != std::wstring::npos) {
			return true;
		}

		wchar_t modulePath[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (length == 0) {
			return false;
		}

		std::wstring exeName = std::filesystem::path(modulePath).stem().wstring();
		std::transform(exeName.begin(), exeName.end(), exeName.begin(), [](wchar_t c) {
			return static_cast<wchar_t>(std::towlower(c));
		});
		return exeName.find(L"codexdemo") != std::wstring::npos;
	}
}

void Game::Initialize() {

	Framework::Initialize();

#ifdef ENABLE_IMGUI
	// Game の初期値と ImGui 側の表示状態を揃える。非表示中は ImGui の
	// フレーム生成・編集パネル描画を行わず、通常プレイの負荷を抑える。
	ImGuiManager::SetVisible(showImGui_);
#endif

	renderWidth_ = static_cast<uint32_t>(WinApp::GetClientWidth());
	renderHeight_ = static_cast<uint32_t>(WinApp::GetClientHeight());

	// RenderTextureの生成と初期化
	renderTexture_ = std::make_unique<RenderTexture>();
	renderTexture_->Initialize(renderWidth_, renderHeight_);

	// ポストエフェクト用テクスチャも初期化
	postEffectTexture_ = std::make_unique<RenderTexture>();
	postEffectTexture_->Initialize(renderWidth_, renderHeight_);


	postEffect_ = std::make_unique<PostEffect>();
	postEffect_->Initialize();

	// 深度バッファのSRVを作成する！
	depthSrvIndex_ = SrvManager::GetInstance()->Allocate();
	DirectXCommon::GetInstance()->CreateDepthSrv(SrvManager::GetInstance()->GetCPUDescriptorHandle(depthSrvIndex_));

	//シーンファクトリーを生成
	sceneFactory_ = std::make_unique<SceneFactory>();

	//マネージャーにファクトリーをセット
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());

	// 通常起動はタイトル画面から開始する。検証専用の起動だけは専用シーンを直接開く。
	const char *initialScene = "TITLE";
	if (ShouldStartSimulationScene()) {
		initialScene = "SIMULATION";
	} else if (ShouldStartCodexDemoScene()) {
		initialScene = "CODEX_DEMO";
	}
	SceneManager::GetInstance()->ChangeScene(initialScene);

	// ノイズ画像を2種類とも読み込んでおく
	TextureManager::GetInstance()->LoadTexture("resources/noise0.png");
	TextureManager::GetInstance()->LoadTexture("resources/noise1.png");
}

void Game::Finalize() {

	
	Input::GetInstance()->SetMouseCursorClipEnabled(false);

	SceneManager::GetInstance()->Finalize();

	//３Dモデルマネージャ・SRVマネージャの終了処理
	ModelManager::GetInstance()->Finalize();
	SrvManager::GetInstance()->Finalize();

	Framework::Finalize();
	winApp->Finalize();
	
}

void Game::Update() {

	Framework::Update();
	HandleResize();
	// シーン側が明示的に有効にした通常プレイ以外では、画面外へのカーソル移動を許可する。
	Input::GetInstance()->SetMouseCursorClipEnabled(false);
	// タイトル・ポーズ・ゲームオーバーなど、マウスを使わない画面ではカーソルを隠す。
	// 編集UIを開いている間だけOSカーソルを表示する。
	winApp->SetCursorVisible(showImGui_);

#ifdef ENABLE_IMGUI
	// F1キーでImGuiの表示・非表示を切り替え
	// DirectInputではなく、ゲームウィンドウが受け取ったWM_KEYDOWNを使う。
	// これならVisual Studioからの起動時やImGuiの入力捕捉中でも取りこぼさない。
	if (winApp->ConsumeF1Pressed()) {
		showImGui_ = !showImGui_;
		ImGuiManager::SetVisible(showImGui_);
	}

	// 編集UIが閉じている通常プレイ中も、ゲーム内操作ガイドなどの
	// オーバーレイを描画できるよう ImGui フレームだけは開始する。
	imGuiManager->BeginFrame();
	if (showImGui_) {

		// 画面全体をImGuiの「ドッキングエリア」にする
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

		postEffect_->DrawImGui();
	} else {
		FlyCamera::SetGameViewHovered(false);
		Input::GetInstance()->ClearMouseCursorClipRect();
	}
#endif

	// ポストエフェクトの更新（自動再生などを計算）
	postEffect_->Update();

	SceneManager::GetInstance()->Update();
}

void Game::HandleResize() {
	uint32_t width = static_cast<uint32_t>(WinApp::GetClientWidth());
	uint32_t height = static_cast<uint32_t>(WinApp::GetClientHeight());
	if (width == 0 || height == 0) {
		return;
	}
	if (width == renderWidth_ && height == renderHeight_) {
		return;
	}

	DirectXCommon::GetInstance()->Resize(static_cast<int32_t>(width), static_cast<int32_t>(height));
	renderTexture_->Resize(width, height);
	postEffectTexture_->Resize(width, height);
	DirectXCommon::GetInstance()->CreateDepthSrv(SrvManager::GetInstance()->GetCPUDescriptorHandle(depthSrvIndex_));

	renderWidth_ = width;
	renderHeight_ = height;
}

void Game::Draw() {

	// ==========================================
	// 1. オフスクリーン描画：3Dシーンを1枚目「renderTexture_」に録画！
	// ==========================================
	renderTexture_->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	DirectXCommon::GetInstance()->PreDraw(SrvManager::GetInstance(), renderTexture_->GetRtvHandle());
	SceneManager::GetInstance()->Draw();
	renderTexture_->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// ==========================================
	// 2. ポストエフェクト描画
	// ==========================================
#ifdef ENABLE_IMGUI
	if (showImGui_) {
		// ImGuiあり：ポストエフェクトを 2枚目「postEffectTexture_」に描画する
		postEffectTexture_->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		DirectXCommon::GetInstance()->PreDraw(SrvManager::GetInstance(), postEffectTexture_->GetRtvHandle());

		// 深度バッファをSRV（読み込み）に切り替え（読み込み）に切り替え
		DirectXCommon::GetInstance()->SetDepthStateToSRV();

		auto noise0Handle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/noise0.png");
		auto noise1Handle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/noise1.png");

		// 1枚目(renderTexture_)を入力にして、2枚目にポストエフェクトをかける！
		postEffect_->Draw(
			renderTexture_->GetSrvHandle(),
			SrvManager::GetInstance()->GetGPUDescriptorHandle(depthSrvIndex_),
			noise0Handle,
			noise1Handle
		);
		postEffectTexture_->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// 深度バッファをDSV（書き込み）に戻す
		DirectXCommon::GetInstance()->SetDepthStateToDSV();

		// ==========================================
		// 3. 実際の画面（スワップチェーン）への描画 (ImGui経由)
		// ==========================================
		DirectXCommon::GetInstance()->PreDrawSwapchain();

		// ゲーム画面をImGuiの「一つのウィンドウ」として表示する！
		ImGui::Begin("Game View");

		// ウィンドウのサイズを取得し、そのサイズに合わせて2枚目のテクスチャを画像として貼り付ける
		ImVec2 availableSize = ImGui::GetContentRegionAvail();
		const float gameViewAspect = static_cast<float>(renderWidth_) / static_cast<float>(renderHeight_);
		ImVec2 imageSize = availableSize;
		if (availableSize.x / availableSize.y > gameViewAspect) {
			imageSize.x = availableSize.y * gameViewAspect;
		} else {
			imageSize.y = availableSize.x / gameViewAspect;
		}

		ImVec2 cursorPos = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(
			cursorPos.x + (availableSize.x - imageSize.x) * 0.5f,
			cursorPos.y + (availableSize.y - imageSize.y) * 0.5f
		));
		ImGui::Image((ImTextureID)postEffectTexture_->GetSrvHandle().ptr, imageSize);
		ImVec2 imageMin = ImGui::GetItemRectMin();
		ImVec2 imageMax = ImGui::GetItemRectMax();
		ImGuiIO &io = ImGui::GetIO();
		bool isMouseInGameView =
			io.MousePos.x >= imageMin.x && io.MousePos.x <= imageMax.x &&
			io.MousePos.y >= imageMin.y && io.MousePos.y <= imageMax.y;
		FlyCamera::SetGameViewBounds(imageMin.x, imageMin.y, imageMax.x, imageMax.y);
		Input::GetInstance()->SetMouseCursorClipRect(imageMin.x, imageMin.y, imageMax.x, imageMax.y);
		FlyCamera::SetGameViewHovered(isMouseInGameView);
		FlyCamera::SubmitGameViewMouseInput(
			isMouseInGameView,
			ImGui::IsMouseClicked(ImGuiMouseButton_Right),
			ImGui::IsMouseClicked(ImGuiMouseButton_Middle),
			ImGui::IsMouseDown(ImGuiMouseButton_Right),
			ImGui::IsMouseDown(ImGuiMouseButton_Middle),
			io.MouseDelta.x,
			io.MouseDelta.y,
			io.MouseWheel);

		ImGui::End();

		// ImGuiの描画コマンドを確定して、コマンドリストに積む
		imGuiManager->EndFrame(DirectXCommon::GetInstance()->GetCommandList());
	} else 
#endif
	{
		Input::GetInstance()->ClearMouseCursorClipRect();
		// ImGuiなし（または非表示）：ポストエフェクトを直接スワップチェーンに描画する
		DirectXCommon::GetInstance()->PreDrawSwapchain();

		// 深度バッファをSRV（読み込み）に切り替え
		DirectXCommon::GetInstance()->SetDepthStateToSRV();

		auto noise0Handle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/noise0.png");
		auto noise1Handle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/noise1.png");

		// 1枚目(renderTexture_)を入力にして、スワップチェーンに直接描画！
		postEffect_->Draw(
			renderTexture_->GetSrvHandle(),
			SrvManager::GetInstance()->GetGPUDescriptorHandle(depthSrvIndex_),
			noise0Handle,
			noise1Handle
		);

		// 深度バッファをDSV（書き込み）に戻す
		DirectXCommon::GetInstance()->SetDepthStateToDSV();

#ifdef ENABLE_IMGUI
		// 通常プレイ用の HUD オーバーレイをスワップチェーンへ重ねる。
		imGuiManager->EndFrame(DirectXCommon::GetInstance()->GetCommandList());
#endif
	}

	//描画後処理
	DirectXCommon::GetInstance()->PostDraw();

}
