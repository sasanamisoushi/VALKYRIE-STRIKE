#pragma once

#include <algorithm>

enum class MissionResultOutcome {
	Clear,
	Failed,
};

// シーン切り替え後も、直前のミッションで得た数値をリザルト画面へ渡す。
struct MissionResultData {
	MissionResultOutcome outcome = MissionResultOutcome::Failed;
	int defeatedEnemyCount = 0;
	int remainingHp = 0;
	int elapsedFrames = 0;
	// リリース版の放置デモから渡された結果か。リザルト後は自動でタイトルへ戻す。
	bool isAttractDemo = false;
};

class MissionResultStore {
public:
	static void Set(const MissionResultData& result) {
		GetMutable() = result;
	}

	static const MissionResultData& Get() {
		return GetMutable();
	}

private:
	static MissionResultData& GetMutable() {
		static MissionResultData result;
		return result;
	}
};
