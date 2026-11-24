#include "CinematicCameraCoordinator.h"

#include "BossClearCameraState.h"
#include "BossClearFocusState.h"
#include "BossClearPhaseState.h"
#include "BossClearTimelineState.h"
#include "GameOverSequence.h"
#include "engine/Camera/Camera.h"
#include "engine/math/MyMath.h"

#include <algorithm>
#include <cmath>

namespace {
float SmoothStep01(float value) {
	value = std::clamp(value, 0.0f, 1.0f);
	return value * value * (3.0f - 2.0f * value);
}

void SetCameraLookAt(Camera* camera, const Vector3& position, const Vector3& target) {
	if (!camera) {
		return;
	}
	const Vector3 toTarget = {
		target.x - position.x,
		target.y - position.y,
		target.z - position.z,
	};
	const float length = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
	if (length <= 0.0001f) {
		return;
	}
	const Vector3 direction = { toTarget.x / length, toTarget.y / length, toTarget.z / length };
	const Quaternion pitch = MyMath::MakeAxisAngle(
		{ 1.0f, 0.0f, 0.0f }, -std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
	const Quaternion yaw = MyMath::MakeAxisAngle(
		{ 0.0f, 1.0f, 0.0f }, std::atan2(direction.x, direction.z));
	camera->SetTranslate(position);
	camera->SetQuaternion(MyMath::Normalize(MyMath::Multiply(yaw, pitch)));
}
}

bool CinematicCameraCoordinator::TryUpdate(
	Camera* renderCamera,
	bool isDebugCameraActive,
	const GameOverSequence& gameOverSequence,
	const BossClearPhaseState& bossClearPhaseState,
	const BossClearTimelineState& bossClearTimeline,
	const BossClearFocusState& bossClearFocusState,
	BossClearCameraState& bossClearCameraState,
	float phase1DurationSec,
	float phase2FlashDurationSec) const {
	if (!renderCamera || isDebugCameraActive) {
		return false;
	}

	if (gameOverSequence.IsActive()) {
		UpdateGameOverCamera(renderCamera, gameOverSequence);
		return true;
	}
	if (bossClearPhaseState.IsActive()) {
		UpdateBossClearCamera(
			renderCamera, bossClearPhaseState, bossClearTimeline,
			bossClearFocusState, bossClearCameraState,
			phase1DurationSec, phase2FlashDurationSec);
		return true;
	}
	return false;
}

void CinematicCameraCoordinator::UpdateGameOverCamera(Camera* renderCamera, const GameOverSequence& gameOverSequence) const {
	if (!renderCamera) {
		return;
	}

	const float progress = std::clamp(
		static_cast<float>(gameOverSequence.GetFrame()) / static_cast<float>(GameOverSequence::kCinematicFrames), 0.0f, 1.0f);
	const float easedProgress = SmoothStep01(progress);
	// 追従カメラを外し、機体を中心にゆっくり横切る軌道へ切り替える。
	const float orbitAngle = -0.85f + easedProgress * 2.55f;
	const float orbitRadius = 10.0f + easedProgress * 7.0f;
	const Vector3 cameraPosition = {
		gameOverSequence.GetFocusPosition().x + std::sin(orbitAngle) * orbitRadius,
		gameOverSequence.GetFocusPosition().y + 2.8f + std::sin(easedProgress * 3.14159265f) * 2.4f,
		gameOverSequence.GetFocusPosition().z - std::cos(orbitAngle) * orbitRadius,
	};
	const Vector3 focus = {
		gameOverSequence.GetFocusPosition().x,
		gameOverSequence.GetFocusPosition().y + 0.6f,
		gameOverSequence.GetFocusPosition().z,
	};
	SetCameraLookAt(renderCamera, cameraPosition, focus);
	renderCamera->SetFovY(0.72f + easedProgress * 0.22f);
}

void CinematicCameraCoordinator::UpdateBossClearCamera(
	Camera* renderCamera,
	const BossClearPhaseState& phaseState,
	const BossClearTimelineState& timeline,
	const BossClearFocusState& focusState,
	BossClearCameraState& cameraState,
	float phase1DurationSec,
	float phase2FlashDurationSec) const {
	if (!renderCamera || !phaseState.IsActive()) {
		return;
	}

	if (phaseState.Is(BossClearPhase::DefeatCinematic) ||
		phaseState.Is(BossClearPhase::FinalExplosion)) {
		const float phase1Progress = std::clamp(timeline.GetSequenceSeconds() / phase1DurationSec, 0.0f, 1.0f);
		const float phase2Progress = phaseState.Is(BossClearPhase::FinalExplosion)
			? std::clamp(timeline.GetPhaseSeconds() / phase2FlashDurationSec, 0.0f, 1.0f)
			: 0.0f;
		const float totalProgress = std::clamp(phase1Progress * 0.80f + phase2Progress * 0.20f, 0.0f, 1.0f);
		const float easedProgress = SmoothStep01(totalProgress);
		// 撃破直後は少し引いた位置から墜落を捉え、大爆発に合わせて側面へ回り込む。
		const float orbitAngle = -0.95f + easedProgress * 1.45f;
		const float baseOrbitRadius = (std::max)(24.0f, focusState.GetRadius() * 3.1f);
		const float orbitRadius = baseOrbitRadius * (1.0f - easedProgress * 0.12f);
		const Vector3 cameraPosition = {
			focusState.GetPosition().x + std::sin(orbitAngle) * orbitRadius,
			focusState.GetPosition().y + focusState.GetRadius() * 0.42f +
				std::sin(easedProgress * 3.14159265f) * focusState.GetRadius() * 0.16f,
			focusState.GetPosition().z - std::cos(orbitAngle) * orbitRadius,
		};
		SetCameraLookAt(renderCamera, cameraPosition, focusState.GetPosition());
		renderCamera->SetFovY(0.78f - easedProgress * 0.08f);
		cameraState.SetAnchorPosition(cameraPosition);
		return;
	}

	// フェーズ3（および MissionClear 表示中）：
	// 爆発地点と飛び散るデブリを映しつつ、わずかに前進（ズーム）＋スライドし続け、画面の完全停止を防ぐ。
	// プレイヤー離脱を挟まず、爆発直後からクリア表示へつなぐ。
	const float phase3Elapsed = timeline.GetPhaseSeconds();
	const Vector3 cameraPosition = cameraState.EvaluateAftermathCameraPosition(phase3Elapsed);
	SetCameraLookAt(renderCamera, cameraPosition, cameraState.GetExplosionCenter());
	renderCamera->SetFovY(cameraState.EvaluateAftermathFovY(phase3Elapsed));
}
