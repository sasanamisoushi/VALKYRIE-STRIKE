#include "WinApp.h"
#include <WinUser.h>
#include <combaseapi.h>
#include <cstdint>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>


#pragma comment(lib,"winmm.lib")

#include "externals/imgui/imgui.h"
#include <strsafe.h>
#include <DbgHelp.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int32_t WinApp::currentClientWidth_ = WinApp::kClientWidth;
int32_t WinApp::currentClientHeight_ = WinApp::kClientHeight;
bool WinApp::f1Pressed_ = false;


//ウィンドウプロシージャ
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// ImGuiのWin32ハンドラーより先に取得する。ImGui表示中にキーボードを
	// 捕捉していても、F1だけは編集画面を閉じるために必ず使えるようにする。
	if (msg == WM_KEYDOWN && wparam == VK_F1 && (lparam & (1LL << 30)) == 0) {
		f1Pressed_ = true;
	}

	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}

	//メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
	case WM_SIZE:
		if (wparam != SIZE_MINIMIZED) {
			int32_t width = static_cast<int32_t>(LOWORD(lparam));
			int32_t height = static_cast<int32_t>(HIWORD(lparam));
			if (width > 0 && height > 0) {
				currentClientWidth_ = width;
				currentClientHeight_ = height;
			}
		}
		return 0;
		//ウィンドウが破棄された
	case WM_DESTROY:
		//OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	//標準のメッセージを処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

bool WinApp::ConsumeF1Pressed() {
	const bool pressed = f1Pressed_;
	f1Pressed_ = false;
	return pressed;
}

LONG WINAPI WinApp::ExportDump(EXCEPTION_POINTERS *exception) {
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	//設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	//最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
	return EXCEPTION_EXECUTE_HANDLER;
}

void WinApp::Initialize() {

	CoInitializeEx(0, COINIT_MULTITHREADED);

	
	//ウインドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	//ウインドウクラス名
	wc.lpszClassName = L"Valkyrie Strike";
	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウインドウクラスを登録する
	RegisterClass(&wc);

	

	//ウインドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,kClientWidth,kClientHeight };

	//クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);


	//システムタイマーの分解能を上げる
	timeBeginPeriod(1);



	//ウィンドウの生成
	wchar_t modulePath[MAX_PATH] = {};
	std::wstring windowTitle = L"VALKYRIE STRIKE";
	if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) != 0) {
		std::wstring exeName = std::filesystem::path(modulePath).stem().wstring();
		std::transform(exeName.begin(), exeName.end(), exeName.begin(), [](wchar_t c) {
			return static_cast<wchar_t>(std::towlower(c));
		});
		if (exeName.find(L"simulation") != std::wstring::npos) {
			windowTitle = L"VALKYRIE STRIKE Simulation";
		}
	}
	hwnd = CreateWindow(
		wc.lpszClassName,
		windowTitle.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr);

	//ウインドウを最大化表示する
	ShowWindow(hwnd, SW_MAXIMIZE);
	RECT clientRect{};
	GetClientRect(hwnd, &clientRect);
	currentClientWidth_ = clientRect.right - clientRect.left;
	currentClientHeight_ = clientRect.bottom - clientRect.top;
}

void WinApp::Update() {
}

void WinApp::Finalize() {

	CloseWindow(hwnd);
	//COMの終了処理
	CoUninitialize();
}

bool WinApp::ProcessMessage() {
	// OS のイベントはフレームごとにまとめて処理する。1件だけ処理すると、
	// マウス移動やリサイズのメッセージが多い場面でキューが滞留し、入力や
	// ウィンドウ応答が数フレーム遅れることがある。
	MSG msg{};
	bool receivedQuit = false;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			receivedQuit = true;
			continue;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return receivedQuit;
}
