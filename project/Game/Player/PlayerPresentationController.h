#pragma once

#include "3D/Object3d.h"
#include "engine/math/MyMath.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

class Camera;

// 可変機モデルの各形態姿勢と部位オブジェクト、クリップ面、ガードバリア演出、
// 透過・スラスター表示状態を管理する見た目専用コントローラ。
class PlayerPresentationController {
public:
	struct TransformPlayerPose {
		Vector3 translate{};
		Vector3 scale{};
		Quaternion rotate{};
	};

	struct TransformPlayerPart {
		std::string name;
		std::unique_ptr<Object3d> object;
		std::array<TransformPlayerPose, 3> poses{};
	};

	void InitializeGuardBarrier();
	std::unique_ptr<Object3d> InitializeTransformMechModel();

	void BeginModeTransition(int fromModeIndex);
	void SnapModeTransition();

	void UpdateTransformMech(
		Object3d* coreObject,
		int toModeIndex,
		const Vector3& position,
		const Quaternion& quaternion,
		const Vector3& modelScale,
		int dodgeTimer,
		int dodgeDurationFrames,
		float dodgeDirection,
		float guardPoseWeight);

	void SetClipPlane(Object3d* coreObject, const Vector3& normal, float distance);
	void ClearClipPlane(Object3d* coreObject);

	void UpdateGuardBarrier(
		bool advanceState,
		float guardScale,
		int currentModeIndex,
		const Vector3& position,
		const Quaternion& quaternion,
		const Vector3& forward);

	void DrawTransformParts(Camera* camera) const;
	void DrawGuardBarrier(Camera* camera, float guardScale) const;

	void SetPresentationAlpha(float alpha);
	float GetPresentationAlpha() const { return presentationAlpha_; }
	void SetPresentationThrustersVisible(bool visible) { presentationThrustersVisible_ = visible; }
	bool ArePresentationThrustersVisible() const { return presentationThrustersVisible_; }

private:
	TransformPlayerPart transformCore_;
	std::vector<TransformPlayerPart> transformParts_;
	int transformFromModeIndex_ = 0;
	float transformModeBlend_ = 1.0f;
	std::unique_ptr<Object3d> guardBarrier_;
	std::unique_ptr<Object3d> guardBarrierRing_;
	float guardBarrierPulse_ = 0.0f;
	float presentationAlpha_ = 1.0f;
	bool presentationThrustersVisible_ = true;
};
