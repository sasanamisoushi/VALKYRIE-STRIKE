#include "ImGuiManager.h"
#include <cstdint>
#include "engine/Graphics/SrvManager.h"

bool ImGuiManager::sIsVisible_ = true;
ImFont* ImGuiManager::sResultRankFont_ = nullptr;
ImFont* ImGuiManager::sGameOverFont_ = nullptr;

void ImGuiManager::Initialize(HWND hwnd, ID3D12Device *device, int numFramesInFlight, DXGI_FORMAT rtvFormat, ID3D12DescriptorHeap *srvHeap) {
#ifdef ENABLE_IMGUI
	// ImGuiのコンテキストを作成
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// スタイル設定
	ImGui::StyleColorsDark();

	// 日本語フォントのロード
	ImGuiIO &io = ImGui::GetIO();
	if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msgothic.ttc", 13.0f, NULL, io.Fonts->GetGlyphRangesJapanese()) == nullptr) {
		io.Fonts->AddFontDefault();
	}
	// ランクは D/C/B/A/S/SS だけなので、ASCII 範囲だけを大きく焼き込み、
	// 日本語フォント全体を巨大なアトラスにせず高精細な文字を確保する。
	static const ImWchar rankGlyphRanges[] = { 0x0020, 0x007E, 0 };
	sResultRankFont_ = io.Fonts->AddFontFromFileTTF(
		"C:\\Windows\\Fonts\\msgothic.ttc", 144.0f, nullptr, rankGlyphRanges);
	// 失敗演出は画面の主役になるため、HUD用の小さいフォントを拡大せず、
	// 読みやすいコンデンスド寄りの UI フォントを大きなサイズで個別に焼き込む。
	sGameOverFont_ = io.Fonts->AddFontFromFileTTF(
		"C:\\Windows\\Fonts\\bahnschrift.ttf", 176.0f, nullptr, rankGlyphRanges);
	if (!sGameOverFont_) {
		sGameOverFont_ = sResultRankFont_;
	}

	// ドッキング機能を有効にする
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// フォントアトラスを明示的にビルドする
	// これにより、最初のフレームの計算時にフォントデータが不足してクラッシュするのを防ぎます
	io.Fonts->Build();

	// WIn32とDX12のバックエンドを初期化
	ImGui_ImplWin32_Init(hwnd);
	// SrvManagerから、ImGuiフォント用の空きインデックスを1つもらう
	uint32_t srvIndex = SrvManager::GetInstance()->Allocate();

	// そのインデックスのハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);

	ImGui_ImplDX12_Init(device, numFramesInFlight, rtvFormat, srvHeap, cpuHandle, gpuHandle);
#endif
}

void ImGuiManager::Shutdown() {
#ifdef ENABLE_IMGUI
	// 逆順で終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	sResultRankFont_ = nullptr;
	sGameOverFont_ = nullptr;
#endif
}

void ImGuiManager::BeginFrame() {
#ifdef ENABLE_IMGUI
	// 編集UIが非表示でも、ゲーム内HUD用の描画リストは毎フレーム作成する。
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void ImGuiManager::EndFrame(ID3D12GraphicsCommandList *commandList) {
#ifdef ENABLE_IMGUI
	//描画データを生成
	ImGui::Render();

	//デスクリプタヒープをセット
	//commandList->SetDescriptorHeaps(1, &srvHeap);

	//コマンドリストにImGuiの描画コマンドを積む
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}


