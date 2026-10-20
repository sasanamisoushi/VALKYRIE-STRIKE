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
	static int32_t GetClientWidth() { return currentClientWidth_; }
	static int32_t GetClientHeight() { return currentClientHeight_; }

public: //定数
	//クライアント領域のサイズ
	static const int32_t kClientWidth = 1920;
	static const int32_t kClientHeight = 1080;
	static int32_t currentClientWidth_;
	static int32_t currentClientHeight_;
	static bool f1Pressed_;

private:

	//ウィンドウハンドル
	HWND hwnd = nullptr;

	//ウインドウクラスの設定
	WNDCLASS wc{};

	

};

