#pragma once

#include "BossClearCameraState.h"
#include "BossClearCrashState.h"
#include "BossClearEffectState.h"
#include "BossClearFocusState.h"
#include "BossClearPhaseState.h"
#include "BossClearPlayerExitState.h"
#include "BossClearTimelineState.h"
#include "BossCrashSmokeTrailManager.h"

#include <functional>
#include <list>
#include <memory>

class Enemy;
class ExplosionManager;
class Obstacle;
class ParticleManager;
class Player;

// ボス撃破後の墜落・大爆発・破片飛散・MISSION CLEAR 演出の開始と進行を一括管理する。
class BossClearSequenceController {
public:
	static constexpr float kPhase1DurationSec = 3.0f;
	static constexpr float kPhase2FlashDurationSec = 0.5f;
	static constexpr float kPhase3PlayerEscapeSec = 1.25f;
	static constexpr float kDebrisAftermathDurationSec = 1.6f;
	static constexpr float kMissionClearDurationSec = 2.6f;

	void Initialize();
	void Reset();

	// 撃破されたボスを受け取り、演出を開始できた場合は true を返す。
	bool TryBegin(
		Enemy* defeatedEnemy,
		Player* player,
		const std::function<void(Enemy*)>& clearEnemyReferences);

	// 演出を 1 フレーム進める。全演出が完了し CLEAR シーンへ遷移すべきなら true を返す。
	bool Update(
		bool isPaused,
		std::list<std::unique_ptr<Enemy>>& enemies,
		const std::list<std::unique_ptr<Obstacle>>& obstacles,
		Player* player,
		ExplosionManager* explosionManager,
		ParticleManager* particles,
		const std::function<void(Enemy*)>& clearEnemyReferences);

	bool IsActive() const { return phaseState_.IsActive(); }
	const BossClearPhaseState& GetPhaseState() const { return phaseState_; }
	const BossClearTimelineState& GetTimeline() const { return timeline_; }
	const BossClearEffectState& GetEffectState() const { return effectState_; }
	const BossClearFocusState& GetFocusState() const { return focusState_; }
	BossClearCameraState& GetCameraState() { return cameraState_; }

private:
	BossClearPhaseState phaseState_;
	BossClearFocusState focusState_;
	BossClearCrashState crashState_;
	BossClearTimelineState timeline_;
	BossClearEffectState effectState_;
	BossClearCameraState cameraState_;
	BossClearPlayerExitState playerExitState_;
	BossCrashSmokeTrailManager smokeTrailManager_;
};
