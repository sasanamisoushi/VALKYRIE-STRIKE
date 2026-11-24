#pragma once

class BossClearEffectState;
class BossClearPhaseState;
class BossClearTimelineState;
class Sprite;

// ボス撃破演出中の上下シネマティック帯・ホワイトフラッシュ・MISSION CLEAR 表示を描画する。
class BossClearOverlayRenderer {
public:
	void Draw(
		const BossClearPhaseState& phaseState,
		const BossClearTimelineState& timeline,
		const BossClearEffectState& effectState,
		float flashDurationSec,
		float missionClearDurationSec,
		Sprite* topBarSprite,
		Sprite* bottomBarSprite,
		Sprite* whiteoutSprite,
		Sprite* missionClearSprite,
		float screenWidth,
		float screenHeight) const;
};
