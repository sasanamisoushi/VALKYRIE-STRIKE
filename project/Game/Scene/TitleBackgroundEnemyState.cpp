#include "TitleBackgroundEnemyState.h"

void TitleBackgroundEnemyState::Reset() {
	respawnTimer_ = 0;
	entering_ = false;
	enterFrame_ = 0;
	spawnSide_ = 1;
	combatTimer_ = 0;
}

void TitleBackgroundEnemyState::SetRespawnDelay(int frames) {
	respawnTimer_ = frames;
}

void TitleBackgroundEnemyState::AdvanceRespawnTimer() {
	if (respawnTimer_ > 0) {
		--respawnTimer_;
	}
}

void TitleBackgroundEnemyState::BeginEntering() {
	entering_ = true;
	enterFrame_ = 0;
	spawnSide_ = -spawnSide_;
	combatTimer_ = 0;
}

bool TitleBackgroundEnemyState::AdvanceEntering() {
	if (!entering_) {
		return false;
	}
	++enterFrame_;
	if (enterFrame_ < kEnterDurationFrames) {
		return false;
	}
	entering_ = false;
	combatTimer_ = 0;
	return true;
}

void TitleBackgroundEnemyState::AdvanceCombatTimer() {
	++combatTimer_;
}

bool TitleBackgroundEnemyState::ShouldFire() const {
	return combatTimer_ == 25 || combatTimer_ == 60 ||
		(combatTimer_ > 60 && (combatTimer_ - 60) % 45 == 0);
}
