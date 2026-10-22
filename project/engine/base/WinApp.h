#pragma once
#include <Windows.h>
#include <cstdint>

class WinApp {
public: //静的メンバ関数

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	static LONG WINAPI ExportDump(EXCEPTION_POINTERS *exception);

public:

	//初期化
	void Initialize();

	//更新
	void Update();

	//終了
	void Finalize();

	//メッセージの処理
	bool ProcessMessage();
	
	//getter
	HWND GetHwnd() const { return hwnd; }
	HINSTANCE GetHinstance() const { return wc.hInstance; }
	// ImGuiの入力捕捉状態に関係なく、ウィンドウが受け取ったF1押下を1回だけ取得する。
	bool ConsumeF1Pressed();
	// ゲーム操作中はOSカーソルを隠し、編集UIを使う時だけ表示する。
	// 非アクティブなウィンドウでは常に表示へ戻す。
	void SetCursorVisible(bool visible);
	// Codex デモ実行ファイルだけで有効にする、Windows メッセージ経由のマウス入力。
	// 通常版の DirectInput 操作および制作時の操作には影響しない。
	static bool IsCodexDemoInputEnabled();
	static void ConsumeCodexDemoMouseState(long &deltaX, long &deltaY, uint8_t (&buttons)[4]);
	static int32_t GetClientWidth() { return currentClientWidth_; }
	static int32_t GetClientHeight() { return currentClientHeight_; }

public: //定数
	//クライアント領域のサイズ
	static const int32_t kClientWidth = 1920;
	static const int32_t kClientHeight = 1080;
	static int32_t currentClientWidth_;
	static int32_t currentClientHeight_;
	static bool f1Pressed_;
	bool isCursorVisible_ = true;
	static bool codexDemoInputEnabled_;
	static bool hasCodexDemoMousePosition_;
	static POINT codexDemoMousePosition_;
	static long codexDemoMouseDeltaX_;
	static long codexDemoMouseDeltaY_;
	static uint8_t codexDemoMouseButtons_[4];

private:

	//ウィンドウハンドル
	HWND hwnd = nullptr;

	//ウインドウクラスの設定
	WNDCLASS wc{};

	

};

