#include "TitleBackgroundEnemyWeaponController.h"

#include "MissilePresetManager.h"
#include "TitleBackgroundEnemyState.h"

#include <cstdlib>

void TitleBackgroundEnemyWeaponController::Update(Enemy& target,
	TitleBackgroundEnemyState& state, MissilePresetManager* missilePresetManager) const {
	if (state.IsEntering()) {
		return;
	}

	state.AdvanceCombatTimer();
	if (!missilePresetManager || !state.ShouldFire()) {
		return;
	}

	const bool isHoming = state.IsFirstCombatShot() || (rand() % 100 < 80);
	const MissileType type = isHoming ? MissileType::MissileWithTrail : MissileType::Normal;
	const float offset = state.IsEarlyCombatShot() ? -0.35f : 0.35f;
	missilePresetManager->FirePlayerMissile(type, &target, offset, false);
}
