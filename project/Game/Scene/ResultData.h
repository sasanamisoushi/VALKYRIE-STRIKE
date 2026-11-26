#pragma once

#include <algorithm>

enum class MissionResultOutcome {
	Clear,
	Failed,
};

// リザルトは訓練と本編で、撃破数・達成点・ランクの基準を分ける。
enum class MissionStage {
	Standard,
	Tutorial,
};

// シーン切り替え後も、直前のミッションで得た数値をリザルト画面へ渡す。
struct MissionResultData {
	MissionResultOutcome outcome = MissionResultOutcome::Failed;
	int defeatedEnemyCount = 0;
	int remainingHp = 0;
	int elapsedFrames = 0;
	// 自動再生デモから渡された結果か。リザルト後は自動でタイトルへ戻す。
	bool isAttractDemo = false;
	MissionStage stage = MissionStage::Standard;
};

class MissionResultStore {
public:
	void Set(const MissionResultData& result) {
		result_ = result;
	}

	const MissionResultData& Get() const {
		return result_;
	}

private:
	MissionResultData result_{};
};
