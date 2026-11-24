#pragma once

#include "engine/math/MyMath.h"

// ボス撃破後のカメラ演出で使う爆発中心・カメラ基準位置・スライド方向を管理する。
class BossClearCameraState {
public:
	void Reset();
	void SetExplosionCenter(const Vector3& center) { explosionCenter_ = center; }
	void SetAnchorPosition(const Vector3& anchorPosition) { anchorPosition_ = anchorPosition; }

	// 現在の基準位置から爆発中心へ向く正規化ベクトルと、水平右方向（スライド方向）を計算する。
	Vector3 ComputeForwardDirection() const;
	Vector3 ComputeSlideDirection() const;
	Vector3 RefreshSlideDirectionFromAnchor();

	// 爆散後のズーム＋スライド演出におけるカメラ位置と FOV を評価する。
	Vector3 EvaluateAftermathCameraPosition(float phaseElapsedSeconds) const;
	float EvaluateAftermathFovY(float phaseElapsedSeconds) const;

	const Vector3& GetExplosionCenter() const { return explosionCenter_; }
	const Vector3& GetAnchorPosition() const { return anchorPosition_; }
	const Vector3& GetSlideDirection() const { return slideDirection_; }

private:
	Vector3 explosionCenter_ = { 0.0f, 0.0f, 0.0f };
	Vector3 anchorPosition_ = { 0.0f, 0.0f, 0.0f };
	Vector3 slideDirection_ = { 1.0f, 0.0f, 0.0f };
};
