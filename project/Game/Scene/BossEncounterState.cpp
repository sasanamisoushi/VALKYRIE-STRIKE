#include "BossEncounterState.h"

void BossEncounterState::Reset() {
	bossSpawned_ = false;
	introFrames_ = 0;
}

void BossEncounterState::MarkBossSpawned() {
	bossSpawned_ = true;
}

void BossEncounterState::BeginIntro() {
	bossSpawned_ = true;
	introFrames_ = kIntroDurationFrames;
}

void BossEncounterState::AdvanceIntro() {
	if (introFrames_ > 0) {
		--introFrames_;
	}
}
