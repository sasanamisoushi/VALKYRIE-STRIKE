#pragma once

#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "Game/enemy/Enemy.h"
#include <memory>

// カメラ関連の処理を管理するクラス (GameCameraManager)
class GameCameraManager {
public:
	GameCameraManager();
	~GameCameraManager() = default;

	// 初期化
	void Initialize();

	// 毎フレームの更新処理
	void Update(Enemy* lockedEnemy, const Vector3& playerPosition);

	// デバッグカメラの有効・無効を切り替える
	void SetDebugCameraActive(bool isActive) { isDebugCameraActive_ = isActive; }
	bool IsDebugCameraActive() const { return isDebugCameraActive_; }

	void SetEditorPreviewPlaying(bool isPlaying) { isEditorPreviewPlaying_ = isPlaying; }
	bool IsEditorPreviewPlaying() const { return isEditorPreviewPlaying_; }

	Camera* GetCamera() const { return camera_.get(); }
	FlyCamera* GetDebugCamera() const { return debugFlyCamera_.get(); }
	
	// 現在アクティブなカメラ（ゲームカメラ or デバッグカメラ）を取得
	Camera* GetActiveCamera() const;

	// シネマティックロックオンカメラの有効化設定（切替時は補間状態も自動でリセット）
	void SetCinematicLockOnEnabled(bool enabled) {
		isCinematicLockOnCameraEnabled_ = enabled;
		isCinematicLockOnCameraInitialized_ = false;
	}
	bool IsCinematicLockOnEnabled() const { return isCinematicLockOnCameraEnabled_; }

	// シネマティックカメラの状態リセット
	void ResetCinematicCamera() { isCinematicLockOnCameraInitialized_ = false; }

	// 指定カメラへロックオン中のシネマティックカメラ演出を適用する
	void UpdateCinematicLockOnCamera(Camera* targetCamera, Enemy* lockedEnemy, const Vector3& playerPosition);

private:
	// 通常ゲームプレイ時のカメラ更新
	void UpdateGameplayCamera(const Vector3& playerPosition);

	std::unique_ptr<Camera> camera_;
	std::unique_ptr<FlyCamera> debugFlyCamera_;

	bool isDebugCameraActive_ = false;
	bool isEditorPreviewPlaying_ = true;

	// シネマティックカメラ関連の変数
	bool isCinematicLockOnCameraEnabled_ = false;
	bool isCinematicLockOnCameraInitialized_ = false;
	Vector3 cinematicLockOnCameraPosition_ = { 0.0f, 0.0f, 0.0f };
	Quaternion cinematicLockOnCameraRotation_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector3 cinematicLockOnCameraFocus_ = { 0.0f, 0.0f, 0.0f };
	Vector3 cinematicLockOnCameraBackDirection_ = { 0.0f, 0.0f, 1.0f };
	float cinematicLockOnCameraSideSign_ = 1.0f;
	float cinematicLockOnCameraSeparation_ = 0.0f;
};
