#pragma once

#include <list>
#include <memory>

class AmmoManager;
class Camera;
class Enemy;
class LockOnManager;
class MissilePresetManager;
class MultiLockChargeState;
class ParticleManager;
class Player;
class SongFlightTrailRenderer;
class SongState;
class SpecialAttackState;
class TargetSelectionState;
class TutorialPhaseController;
class TutorialProgressTracker;
struct GameplayBalanceSettings;
struct IXAudio2SourceVoice;

// 必殺技・歌の発動・更新・終了および自動リロードに必要な文脈。
struct PlayerSpecialSongContext {
	bool shouldUpdateGame = false;
	bool canUsePlayerInput = false;
	bool isGameOverActive = false;
	const GameplayBalanceSettings* balanceSettings = nullptr;
	Player* player = nullptr;
	const std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	TargetSelectionState* targetSelection = nullptr;
	AmmoManager* ammoManager = nullptr;
	MissilePresetManager* missilePresetManager = nullptr;
	ParticleManager* particleManager = nullptr;
	SongFlightTrailRenderer* songFlightTrailRenderer = nullptr;
	SpecialAttackState* specialAttackState = nullptr;
	SongState* songState = nullptr;
	IXAudio2SourceVoice* bgmVoice = nullptr;
	IXAudio2SourceVoice* songVoice = nullptr;
};

// 通常射撃・連射クールダウン・ホーミング／マルチロック操作に必要な文脈。
struct PlayerWeaponInputContext {
	bool allowWeaponInput = false;
	bool isGameOverActive = false;
	Player* player = nullptr;
	Camera* activeCamera = nullptr;
	Enemy* lockedEnemy = nullptr;
	Enemy* aimAssistEnemy = nullptr;
	LockOnManager* lockOnManager = nullptr;
	MissilePresetManager* missilePresetManager = nullptr;
	const SpecialAttackState* specialAttackState = nullptr;
	const MultiLockChargeState* multiLockChargeState = nullptr;
	const TutorialPhaseController* tutorialController = nullptr;
	TutorialProgressTracker* tutorialProgress = nullptr;
};

// プレイヤーの武器入力（通常射撃・連射・ホーミング／マルチロック）と
// 必殺技・歌の開始／更新／終了を一括管理する。
class PlayerWeaponController {
public:
	void Reset();
	void UpdateSpecialAndSong(const PlayerSpecialSongContext& context);
	void UpdateWeaponInput(const PlayerWeaponInputContext& context);

private:
	static constexpr int kNormalFireCooldownFrames = 8;

	void AcquireNearestEnemyLockIfNeeded(const PlayerSpecialSongContext& context) const;

	int normalFireCooldownTimer_ = 0;
};
