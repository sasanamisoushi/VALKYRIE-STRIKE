#pragma once

#include <cstddef>

class Camera;
class Player;
class TutorialGoalMarker;
class TutorialPhaseController;
class TutorialProgressTracker;
struct TutorialRequirements;

// チュートリアル中の操縦支援パネル・ハイライト・ゴール矢印を描画する。
// 進行状態は読み取り専用で受け取り、描画だけを担当する。
class TutorialOverlayRenderer {
public:
	void Draw(const TutorialPhaseController& controller, const TutorialProgressTracker& progress,
		const TutorialRequirements& requirements, const TutorialGoalMarker& goalMarker,
		const Player* player, Camera* navigationCamera, std::size_t remainingEnemyCount,
		float screenWidth, float screenHeight) const;
};
