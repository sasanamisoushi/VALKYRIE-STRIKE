#include "WarpExitPresentationState.h"

#include "3D/Model.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "Game/Player/Player.h"
#include "engine/Camera/Camera.h"
#include "engine/Particle/ParticleManager.h"

#include <algorithm>
#include <cmath>

namespace {
Vector3 AddVector3(const Vector3& a, const Vector3& b) {
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 ScaleVector3(const Vector3& v, float scale) {
	return { v.x * scale, v.y * scale, v.z * scale };
}

Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t) {
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t,
	};
}

Vector3 NormalizeOrVector3(const Vector3& v, const Vector3& fallback) {
	const float length = MyMath::Length(v);
	if (length <= 0.0001f) {
		return fallback;
	}
	return { v.x / length, v.y / length, v.z / length };
}

float SmoothStep01(float value) {
	const float t = std::clamp(value, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
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

void WarpExitPresentationState::Reset() {
	timeline_.Reset();
	gatePosition_ = { 0.0f, 0.0f, 0.0f };
	startPosition_ = { 0.0f, 0.0f, 0.0f };
	endPosition_ = { 0.0f, 0.0f, 0.0f };
	forward_ = { 0.0f, 0.0f, 1.0f };
	right_ = { 1.0f, 0.0f, 0.0f };
	cinematicCameraPosition_ = { 0.0f, 0.0f, 0.0f };
	yaw_ = 0.0f;
}

void WarpExitPresentationState::Start(Player& player) {
	const Vector3 stageSpawnPosition = player.GetPosition();
	const Vector3 forward = NormalizeOrVector3(player.GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	const float horizontalLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);
	if (horizontalLength > 0.0001f) {
		const float yaw = std::atan2(forward.x, forward.z);
		player.SetRotation({ 0.0f, yaw, 0.0f });
	}

	// ワープ出口をスポーン地点から十分に後方へ置き、機体がステージへ
	// 飛び込んでくる距離を確保する。
	forward_ = forward;
	right_ = NormalizeOrVector3({ forward.z, 0.0f, -forward.x }, { 1.0f, 0.0f, 0.0f });
	yaw_ = std::atan2(forward.x, forward.z);
	gatePosition_ = AddVector3(stageSpawnPosition, ScaleVector3(forward, -8.0f));
	startPosition_ = AddVector3(gatePosition_, ScaleVector3(forward, -4.0f));
	// 操作開始は終了時の後方カメラに対して少し手前になるよう、Blender の
	// スポーン地点から進行方向と逆へ 3.5 ユニット寄せる。
	endPosition_ = AddVector3(stageSpawnPosition, ScaleVector3(forward, -3.5f));
	// 出現まではゲート前方の斜め位置から捉え、出口通過後にだけ
	// 通常の後方追従カメラへ切り替える。
	cinematicCameraPosition_ = AddVector3(
		AddVector3(gatePosition_, ScaleVector3(forward, 2.8f)),
		AddVector3(ScaleVector3(right_, 4.8f), { 0.0f, 2.1f, 0.0f }));
	timeline_.Start();

	player.SetPresentationAlpha(0.0f);
	player.SetPresentationThrustersVisible(false);
	const float clipPlaneDistance = -(forward_.x * gatePosition_.x +
		forward_.y * gatePosition_.y + forward_.z * gatePosition_.z);
	player.SetPresentationClipPlane(forward_, clipPlaneDistance);
	player.UpdatePresentation(startPosition_, { -0.04f, yaw_, 0.0f }, 2.0f, true);
}

void WarpExitPresentationState::InitializeGateObjects() {
	if (!timeline_.IsActive() && !timeline_.IsGateVisible()) {
		return;
	}

	ModelManager* modelManager = ModelManager::GetInstance();
	modelManager->CreatePlaneModel("warp_exit_ring_quad");
	modelManager->CreatePlaneModel("warp_exit_fill_quad");
	modelManager->CreatePlaneModel("warp_exit_core_quad");
	modelManager->CreatePlaneModel("warp_exit_spiral_outer_quad");
	modelManager->CreatePlaneModel("warp_exit_spiral_inner_quad");
	if (Model* ringModel = modelManager->FindModel("warp_exit_ring_quad")) {
		ringModel->SetTextureFilePath("resources/warp_cloud_ring.png");
		ringModel->SetEnableLighting(false);
	}
	if (Model* fillModel = modelManager->FindModel("warp_exit_fill_quad")) {
		fillModel->SetTextureFilePath("resources/circle2.png");
		fillModel->SetEnableLighting(false);
	}
	if (Model* coreModel = modelManager->FindModel("warp_exit_core_quad")) {
		coreModel->SetTextureFilePath("resources/warp_flare_core.png");
		coreModel->SetEnableLighting(false);
	}
	if (Model* spiralOuterModel = modelManager->FindModel("warp_exit_spiral_outer_quad")) {
		spiralOuterModel->SetTextureFilePath("resources/fold_cloud_noise.png");
		spiralOuterModel->SetEnableLighting(false);
	}
	if (Model* spiralInnerModel = modelManager->FindModel("warp_exit_spiral_inner_quad")) {
		spiralInnerModel->SetTextureFilePath("resources/fold_cloud_noise.png");
		spiralInnerModel->SetEnableLighting(false);
	}

	ringObject_ = std::make_unique<Object3d>();
	ringObject_->Initialize(Object3dCommon::GetInstance());
	ringObject_->SetModel("warp_exit_ring_quad");
	fillObject_ = std::make_unique<Object3d>();
	fillObject_->Initialize(Object3dCommon::GetInstance());
	fillObject_->SetModel("warp_exit_fill_quad");
	coreObject_ = std::make_unique<Object3d>();
	coreObject_->Initialize(Object3dCommon::GetInstance());
	coreObject_->SetModel("warp_exit_core_quad");
	spiralOuterObject_ = std::make_unique<Object3d>();
	spiralOuterObject_->Initialize(Object3dCommon::GetInstance());
	spiralOuterObject_->SetModel("warp_exit_spiral_outer_quad");
	spiralInnerObject_ = std::make_unique<Object3d>();
	spiralInnerObject_->Initialize(Object3dCommon::GetInstance());
	spiralInnerObject_->SetModel("warp_exit_spiral_inner_quad");
}

void WarpExitPresentationState::UpdatePlayer(Player& player, ParticleManager* particles) {
	if (!timeline_.IsActive()) {
		return;
	}

	const float fadeProgress = SmoothStep01(
		static_cast<float>(timeline_.GetFrame() - WarpExitTimelineState::kPlayerFadeStartFrame) /
		static_cast<float>(WarpExitTimelineState::kPlayerFadeFrames));
	player.SetPresentationAlpha(fadeProgress);
	const float progress = std::clamp(
		static_cast<float>(timeline_.GetFrame()) / static_cast<float>(WarpExitTimelineState::kDurationFrames), 0.0f, 1.0f);
	// 出現直後はワープの慣性を残して素早く飛び出し、ステージへ近づくほど
	// 減速する Ease-Out。開始と終了の両方が遅い SmoothStep を使わないことで、
	// ポータルから押し出され続けるような印象をなくす。
	const float easedProgress = 1.0f - (1.0f - progress) * (1.0f - progress);
	Vector3 position = {
		startPosition_.x + (endPosition_.x - startPosition_.x) * easedProgress,
		startPosition_.y + (endPosition_.y - startPosition_.y) * easedProgress,
		startPosition_.z + (endPosition_.z - startPosition_.z) * easedProgress,
	};
	// ワープ面の中心を通る軌道へ寄せ、後端のスラスターまで画面内に収める。
	// 始点・終点はスポーンラインへ正確に戻す。
	const float flyByOffset = std::sin(progress * 3.14159265f) * 0.32f;
	position = AddVector3(position, ScaleVector3(right_, flyByOffset));
	const float roll = std::sin(progress * 3.14159265f) * 0.18f;
	// 噴射・ノズルは独立描画なので、クリップ面より先に見えないよう
	// 機体後端が出口を通る頃から有効にする。
	player.SetPresentationThrustersVisible(timeline_.GetFrame() >= WarpExitTimelineState::kGateFadeStartFrame);
	// 高速で飛び出した直後から、巡航速度とスラスター出力を一緒に落とす。
	const float exitSpeed = 0.82f - progress * 0.40f;
	const float thrusterPower = 3.35f - progress * 1.55f;
	player.SetVelocity(ScaleVector3(forward_, exitSpeed));
	player.UpdatePresentation(position, { -0.05f, yaw_, roll }, thrusterPower, true);
	// 機体が出口面を突き破る瞬間に、前方へ短く細かな発光粒子を散らす。
	// 既存の test グループを使うため、新しい描画用ディスクリプタは消費しない。
	if (timeline_.GetFrame() >= WarpExitTimelineState::kGatePassageFrame && timeline_.TryTriggerImpact()) {
		if (particles) {
			const Vector3 burstPosition = AddVector3(position, ScaleVector3(forward_, 0.72f));
			particles->EmitDirectionalBurst(
				"test", burstPosition, forward_, 56,
				{ 0.55f, 0.92f, 1.0f, 1.0f }, 0.62f,
				0.52f, 0.22f, 0.052f, 0.026f, 0.22f, 0.46f, 0.34f);
		}
	}
	timeline_.AdvanceFrame();
	if (timeline_.GetFrame() == WarpExitTimelineState::kCameraHandoffStartFrame) {
		// 後方カメラへ回る前に出口を消す。これでワープがカメラと自機の間に
		// 残らず、機体後方から進行方向を見通せる。ここまでにクリップ面で
		// 自機の後端も出口を通り抜けている。
		timeline_.HideGate();
	}
	if (timeline_.GetFrame() >= WarpExitTimelineState::kDurationFrames) {
		// 操作開始へ速度を引き継ぐ。ここで停止させないことで、演出終了と
		// 本編開始の間に機体・カメラが一拍止まるのを防ぐ。
		const Vector3 handoffVelocity = ScaleVector3(forward_, 0.42f);
		player.SetVelocity(handoffVelocity);
		player.UpdatePresentation(endPosition_, { 0.0f, yaw_, 0.0f }, 2.2f, true);
		player.ClearPresentationClipPlane();
		player.SetPresentationThrustersVisible(true);
		timeline_.Finish();
	}
}

void WarpExitPresentationState::UpdateCamera(Camera* renderCamera, const Player* player) const {
	if (!timeline_.IsActive() || !renderCamera || !player) {
		return;
	}

	const float progress = std::clamp(
		static_cast<float>(timeline_.GetFrame()) / static_cast<float>(WarpExitTimelineState::kDurationFrames), 0.0f, 1.0f);
	// UpdateCamera が先に作った通常追従カメラを終点に使うことで、演出終了時に
	// 位置・画角ともゲームプレイ用カメラへ自然につながる。
	const Vector3 followCameraPosition = renderCamera->GetTranslate();
	// 機体がゲートを抜けた瞬間からカメラを引き始める。従来のように後半まで
	// 固定したままにせず、飛び出す勢いと連動して画を広げる。
	const float cameraPullbackDuration = static_cast<float>(
		WarpExitTimelineState::kCameraHandoffStartFrame - WarpExitTimelineState::kGatePassageFrame + WarpExitTimelineState::kCameraHandoffFrames);
	const float handoffProgress = SmoothStep01(
		(static_cast<float>(timeline_.GetFrame() - WarpExitTimelineState::kGatePassageFrame)) /
		cameraPullbackDuration);
	const Vector3 cinematicPosition = {
		cinematicCameraPosition_.x + std::sin(progress * 5.2f) * 0.12f,
		cinematicCameraPosition_.y + std::sin(progress * 4.1f) * 0.06f,
		cinematicCameraPosition_.z,
	};
	const Vector3 cameraPosition = LerpVector3(cinematicPosition, followCameraPosition, handoffProgress);
	// 出口面を抜ける一瞬だけ、減衰の速い小さな振動を重ねて質量感を出す。
	const float impactTime = static_cast<float>(timeline_.GetFrame() - WarpExitTimelineState::kGatePassageFrame);
	const float impactEnvelope = std::clamp(1.0f - std::abs(impactTime) / 11.0f, 0.0f, 1.0f);
	const float impactStrength = impactEnvelope * impactEnvelope;
	const Vector3 impactShake = {
		std::sin(impactTime * 3.8f) * 0.16f * impactStrength,
		std::cos(impactTime * 5.1f) * 0.095f * impactStrength,
		-0.07f * impactStrength,
	};
	const Vector3 shakenCameraPosition = AddVector3(cameraPosition, impactShake);

	// カメラの移動と注視点の切り替えに同じ補間率を使い、ポータルを抜ける
	// 機体を捉えながら自然に通常の後方追従へ渡す。
	const float trackingProgress = handoffProgress;
	const Vector3 gateFocus = AddVector3(gatePosition_, { 0.0f, 0.65f, 0.0f });
	const Vector3 playerFocus = AddVector3(player->GetPosition(), ScaleVector3(forward_, 4.2f));
	const Vector3 focus = LerpVector3(gateFocus, playerFocus, trackingProgress);
	SetCameraLookAt(renderCamera, shakenCameraPosition, focus);
	// 出現時は広角、減速に合わせて通常画角へ戻して速度感を保つ。
	renderCamera->SetFovY(0.80f - handoffProgress * 0.32f);
}

void WarpExitPresentationState::UpdateGate(Camera* renderCamera) {
	if (!timeline_.IsGateVisible() || !renderCamera) {
		return;
	}

	const float fadeOut = 1.0f - SmoothStep01(
		static_cast<float>(timeline_.GetFrame() - WarpExitTimelineState::kGateFadeStartFrame) /
		static_cast<float>(WarpExitTimelineState::kCameraHandoffStartFrame - WarpExitTimelineState::kGateFadeStartFrame));
	const float animationProgress = std::clamp(
		static_cast<float>(timeline_.GetFrame()) / static_cast<float>(WarpExitTimelineState::kDurationFrames), 0.0f, 1.0f);
	const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(timeline_.GetFrame()) * 0.36f);

	if (ringObject_) {
		ringObject_->SetTranslate(gatePosition_);
		ringObject_->SetRotate({ 0.0f, yaw_, static_cast<float>(timeline_.GetFrame()) * 0.055f });
		const float scale = 4.4f + animationProgress * 1.5f + pulse * 0.24f;
		ringObject_->SetScale({ scale, scale, 1.0f });
		if (Model* ringModel = ringObject_->GetModel()) {
			ringModel->SetColor({ 0.56f, 0.90f, 1.0f, fadeOut * (0.72f + pulse * 0.24f) });
		}
		ringObject_->Update(renderCamera);
	}
	if (fillObject_) {
		fillObject_->SetTranslate(AddVector3(gatePosition_, ScaleVector3(forward_, -0.02f)));
		fillObject_->SetRotate({ 0.0f, yaw_, static_cast<float>(timeline_.GetFrame()) * 0.018f });
		const float fillScale = 4.05f + animationProgress * 1.34f + pulse * 0.14f;
		fillObject_->SetScale({ fillScale, fillScale, 1.0f });
		if (Model* fillModel = fillObject_->GetModel()) {
			fillModel->SetColor({ 0.30f, 0.72f, 1.0f, fadeOut * (0.56f + pulse * 0.13f) });
		}
		fillObject_->Update(renderCamera);
	}
	if (coreObject_) {
		coreObject_->SetTranslate(AddVector3(gatePosition_, ScaleVector3(forward_, -0.03f)));
		coreObject_->SetRotate({ 0.0f, yaw_, -static_cast<float>(timeline_.GetFrame()) * 0.032f });
		const float coreScale = 2.25f + animationProgress * 1.25f + pulse * 0.18f;
		coreObject_->SetScale({ coreScale, coreScale, 1.0f });
		if (Model* coreModel = coreObject_->GetModel()) {
			coreModel->SetColor({ 0.48f, 0.84f, 1.0f, fadeOut * (0.25f + pulse * 0.12f) });
		}
		coreObject_->Update(renderCamera);
	}
	// 外周のリングとは別に、出口の奥で二枚の雲を逆方向へ回す。中心にも
	// 回転の流れを作ることで、平らな輪ではなく奥へ続く螺旋の出口に見せる。
	const float spiralPulse = 0.5f + 0.5f * std::sin(static_cast<float>(timeline_.GetFrame()) * 0.19f);
	if (spiralOuterObject_) {
		spiralOuterObject_->SetTranslate(
			AddVector3(gatePosition_, ScaleVector3(forward_, -0.045f)));
		spiralOuterObject_->SetRotate({
			0.0f, yaw_, -static_cast<float>(timeline_.GetFrame()) * 0.118f + animationProgress * 1.4f });
		const float outerScale = 4.90f - animationProgress * 1.20f + spiralPulse * 0.18f;
		spiralOuterObject_->SetScale({ outerScale, outerScale * 0.68f, 1.0f });
		if (Model* spiralOuterModel = spiralOuterObject_->GetModel()) {
			spiralOuterModel->SetColor({ 0.24f, 0.72f, 1.0f,
				fadeOut * (0.30f + (1.0f - animationProgress) * 0.26f) });
		}
		spiralOuterObject_->Update(renderCamera);
	}
	if (spiralInnerObject_) {
		spiralInnerObject_->SetTranslate(
			AddVector3(gatePosition_, ScaleVector3(forward_, -0.055f)));
		spiralInnerObject_->SetRotate({
			0.0f, yaw_, static_cast<float>(timeline_.GetFrame()) * 0.164f - animationProgress * 2.1f });
		const float innerScale = 3.10f - animationProgress * 0.58f + spiralPulse * 0.14f;
		spiralInnerObject_->SetScale({ innerScale, innerScale * 0.68f, 1.0f });
		if (Model* spiralInnerModel = spiralInnerObject_->GetModel()) {
			spiralInnerModel->SetColor({ 0.56f, 0.94f, 1.0f,
				fadeOut * (0.22f + (1.0f - animationProgress) * 0.24f) });
		}
		spiralInnerObject_->Update(renderCamera);
	}

	timeline_.AdvanceGateFrame();
}

void WarpExitPresentationState::DrawGate() const {
	if (!timeline_.IsGateVisible()) {
		return;
	}
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (fillObject_) {
		fillObject_->Draw();
	}
	if (spiralOuterObject_) {
		spiralOuterObject_->Draw();
	}
	if (spiralInnerObject_) {
		spiralInnerObject_->Draw();
	}
	if (coreObject_) {
		coreObject_->Draw();
	}
	if (ringObject_) {
		ringObject_->Draw();
	}
}

bool WarpExitPresentationState::ShouldDrawMissionStartOverlay() const {
	return timeline_.IsActive() && timeline_.GetFrame() >= WarpExitTimelineState::kMissionStartFrame;
}

float WarpExitPresentationState::GetMissionStartOverlayProgress() const {
	return std::clamp(
		static_cast<float>(timeline_.GetFrame() - WarpExitTimelineState::kMissionStartFrame) /
			static_cast<float>(WarpExitTimelineState::kDurationFrames - WarpExitTimelineState::kMissionStartFrame),
		0.0f, 1.0f);
}
