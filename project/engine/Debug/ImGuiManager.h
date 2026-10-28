#pragma once
#include<Windows.h>
#include <d3d12.h>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

class ImGuiManager {
public:
	//初期化
	//　hwnd:ウインドハンドル
	// device:DirectX12のデバイス
	// numFramesInFlight:スワップチェーンのバッファ数
	// rtvFormat:レンダーターゲットのフォーマット
	// srvHeap: ImGuiのフォントテクスチャ等を配置するSRVデスクリプタヒープ
	void Initialize(HWND hwnd, ID3D12Device *device, int numFramesInFlight, DXGI_FORMAT rtvFormat, ID3D12DescriptorHeap *srvHeap);

	void Shutdown();

	//毎フレームの処理
	void BeginFrame();

	// CommandList: ImGuiの描画コマンドを起動するコマンドリスト
	void EndFrame(ID3D12GraphicsCommandList *commandList);

	// 表示フラグの取得・設定 (静的)
	static bool IsVisible() { return sIsVisible_; }
	static void SetVisible(bool visible) { sIsVisible_ = visible; }
	// リザルトのランク表示用。小さいデバッグ用フォントを拡大せず、専用の高解像度フォントを使う。
	static ImFont* GetResultRankFont() { return sResultRankFont_; }
	// ゲームオーバー演出用。大きな警告文字を輪郭まで鮮明に描くための専用フォント。
	static ImFont* GetGameOverFont() { return sGameOverFont_; }

private:
	static bool sIsVisible_;
	static ImFont* sResultRankFont_;
	static ImFont* sGameOverFont_;
};

