#pragma once

class BossEncounterState;

// ボス出現警告の ImGui オーバーレイを描画する。
class BossIntroOverlayRenderer {
public:
	void Draw(const BossEncounterState& state, float screenWidth, float screenHeight) const;
};
