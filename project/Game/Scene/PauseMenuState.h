#pragma once

// ポーズメニューの開閉と選択項目だけを管理する。
// 入力取得、音声制御、タイトルへ戻る遷移はGamePlaySceneが担当する。
class PauseMenuState {
public:
	void Reset();
	bool Toggle();
	void Resume();
	void ToggleSelection();

	bool IsPaused() const { return paused_; }
	bool IsTitleSelected() const { return titleSelected_; }

private:
	bool paused_ = false;
	bool titleSelected_ = false;
};
