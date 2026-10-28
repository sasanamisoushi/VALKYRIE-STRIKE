#include "GameOverScene.h"
#include "2D/SpriteCommon.h"
#include "engine/Input/Input.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Graphics/PostEffect.h"
#include "3D/Object3dCommon.h"
#include "engine/base/WinApp.h"
#include "Game/Scene/ResultOverlay.h"
#include "Game/Scene/TitleScene.h"
#include <cmath>

void GameOverScene::Initialize() {
	// カメラの生成と初期化
	camera_ = std::make_unique<Camera>();
	camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
	camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());

	// リザルトでは画面演出を解除して、戦績を明瞭に表示する。
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	resultBackgroundSprite_ = std::make_unique<Sprite>();
	resultBackgroundSprite_->Initialize(SpriteCommon::GetInstance(), "resources/result_panel_frame_base.png");
	resultBackgroundSprite_->SetPosition({ 640.0f, 360.0f });
	resultBackgroundSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	resultBackgroundSprite_->SetSize({ 1280.0f, 720.0f });
	transitionFadeSprite_ = std::make_unique<Sprite>();
	transitionFadeSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	transitionFadeSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	transitionFadeAlpha_ = 1.0f;
	isReturningToTitle_ = false;
	isAttractDemoResult_ = MissionResultStore::Get().isAttractDemo;
	returnToTitleFrame_ = 0;

	// 地図・格子・雲状の模様は1枚の流動レイヤーとして横へ送る。機体だけは別レイヤーで固定する。
	resultFlowingWorldPrimarySprite_ = std::make_unique<Sprite>();
	resultFlowingWorldPrimarySprite_->Initialize(SpriteCommon::GetInstance(), "resources/result_flowing_world_overlay.png");
	resultFlowingWorldPrimarySprite_->SetAnchorPoint({ 0.5f, 0.5f });
	resultFlowingWorldPrimarySprite_->SetColor({ 0.42f, 0.78f, 1.0f, 0.27f });
	resultFlowingWorldSecondarySprite_ = std::make_unique<Sprite>();
	resultFlowingWorldSecondarySprite_->Initialize(SpriteCommon::GetInstance(), "resources/result_flowing_world_overlay.png");
	resultFlowingWorldSecondarySprite_->SetAnchorPoint({ 0.5f, 0.5f });
	resultFlowingWorldSecondarySprite_->SetColor({ 0.42f, 0.78f, 1.0f, 0.27f });

	resultStaticAircraftSprite_ = std::make_unique<Sprite>();
	resultStaticAircraftSprite_->Initialize(SpriteCommon::GetInstance(), "resources/result_static_aircraft.png");
	resultStaticAircraftSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	resultStaticAircraftSprite_->SetColor({ 0.40f, 0.82f, 1.0f, 0.33f });

	// 雲専用の透明レイヤーは2枚を異なる速度・高さで横切らせる。
	resultCloudFlowNearSprite_ = std::make_unique<Sprite>();
	resultCloudFlowNearSprite_->Initialize(SpriteCommon::GetInstance(), "resources/result_cloud_flow.png");
	resultCloudFlowNearSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	resultCloudFlowNearSprite_->SetColor({ 0.48f, 0.84f, 1.0f, 0.16f });
	resultCloudFlowFarSprite_ = std::make_unique<Sprite>();
	resultCloudFlowFarSprite_->Initialize(SpriteCommon::GetInstance(), "resources/result_cloud_flow.png");
	resultCloudFlowFarSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	resultCloudFlowFarSprite_->SetColor({ 0.34f, 0.72f, 1.0f, 0.09f });

	// 背景から分解した円形HUD。2枚を別回転させ、元絵の左下／右上に戻す。
	resultHudRingTopRightSprite_ = std::make_unique<Sprite>();
	resultHudRingTopRightSprite_->Initialize(SpriteCommon::GetInstance(), "resources/result_hud_ring.png");
	resultHudRingTopRightSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	resultHudRingTopRightSprite_->SetColor({ 0.46f, 0.88f, 1.0f, 0.32f });

	resultHudRingBottomLeftSprite_ = std::make_unique<Sprite>();
	resultHudRingBottomLeftSprite_->Initialize(SpriteCommon::GetInstance(), "resources/result_hud_ring.png");
	resultHudRingBottomLeftSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	resultHudRingBottomLeftSprite_->SetColor({ 0.44f, 0.86f, 1.0f, 0.28f });
}

void GameOverScene::Finalize() {
	// シーン終了時にポストエフェクトを通常に戻す
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}
}

void GameOverScene::Update() {
	// カメラの更新
	camera_->Update();
	++resultAnimationFrame_;

	if (resultBackgroundSprite_) {
		float width = static_cast<float>(WinApp::GetClientWidth());
		float height = static_cast<float>(WinApp::GetClientHeight());
		// ベース画像は固定し、背景から分解した円形HUDだけを動かす。
		resultBackgroundSprite_->SetPosition({ width * 0.5f, height * 0.5f });
		resultBackgroundSprite_->SetSize({ width, height });
		resultBackgroundSprite_->Update();

		const float minDimension = (std::min)(width, height);
		const float frame = static_cast<float>(resultAnimationFrame_);
		const float worldWidth = width * 1.28f;
		const float worldOffset = std::fmod(frame * 0.12f, worldWidth);
		const float worldFirstX = worldOffset - worldWidth * 0.5f;
		if (resultFlowingWorldPrimarySprite_) {
			resultFlowingWorldPrimarySprite_->SetPosition({ worldFirstX, height * 0.5f });
			resultFlowingWorldPrimarySprite_->SetSize({ worldWidth, height * 1.06f });
			resultFlowingWorldPrimarySprite_->Update();
		}
		if (resultFlowingWorldSecondarySprite_) {
			resultFlowingWorldSecondarySprite_->SetPosition({ worldFirstX + worldWidth, height * 0.5f });
			resultFlowingWorldSecondarySprite_->SetSize({ worldWidth, height * 1.06f });
			resultFlowingWorldSecondarySprite_->Update();
		}
		if (resultStaticAircraftSprite_) {
			resultStaticAircraftSprite_->SetPosition({ width * 0.83f, height * 0.29f });
			resultStaticAircraftSprite_->SetSize({ width * 0.28f, height * 0.25f });
			resultStaticAircraftSprite_->Update();
		}
		const float cloudWidth = width * 0.92f;
		const float cloudHeight = height * 0.34f;
		const float cloudTravel = width + cloudWidth;
		const float nearCloudX = std::fmod(frame * 0.36f, cloudTravel) - cloudWidth * 0.5f;
		const float farCloudX = std::fmod(frame * 0.22f + cloudTravel * 0.52f, cloudTravel) - cloudWidth * 0.5f;
		if (resultCloudFlowNearSprite_) {
			resultCloudFlowNearSprite_->SetPosition({ nearCloudX, height * 0.79f });
			resultCloudFlowNearSprite_->SetSize({ cloudWidth, cloudHeight });
			resultCloudFlowNearSprite_->Update();
		}
		if (resultCloudFlowFarSprite_) {
			resultCloudFlowFarSprite_->SetPosition({ farCloudX, height * 0.36f });
			resultCloudFlowFarSprite_->SetSize({ cloudWidth * 0.82f, cloudHeight * 0.72f });
			resultCloudFlowFarSprite_->Update();
		}
		if (resultHudRingTopRightSprite_) {
			resultHudRingTopRightSprite_->SetPosition({ width + minDimension * 0.065f, -minDimension * 0.075f });
			resultHudRingTopRightSprite_->SetSize({ minDimension * 0.82f, minDimension * 0.82f });
			resultHudRingTopRightSprite_->SetRotation(frame * 0.00105f);
			resultHudRingTopRightSprite_->Update();
		}
		if (resultHudRingBottomLeftSprite_) {
			resultHudRingBottomLeftSprite_->SetPosition({ -minDimension * 0.11f, height + minDimension * 0.10f });
			resultHudRingBottomLeftSprite_->SetSize({ minDimension * 1.08f, minDimension * 1.08f });
			resultHudRingBottomLeftSprite_->SetRotation(-frame * 0.00082f);
			resultHudRingBottomLeftSprite_->Update();
		}
	}

	// CRT シャットダウン演出を最後まで表示してからタイトルへ戻る。
	if (isReturningToTitle_) {
		++returnToTitleFrame_;
		const float progress = std::clamp(
			static_cast<float>(returnToTitleFrame_) / static_cast<float>(kShutdownDurationFrames), 0.0f, 1.0f);
		const float glitchIntensity = progress > 0.85f
			? std::clamp((1.0f - progress) / 0.15f, 0.0f, 1.0f)
			: 1.0f;
		if (PostEffect::GetInstance()) {
			PostEffect::GetInstance()->SetSystemShutdown(progress, glitchIntensity);
		}
		if (returnToTitleFrame_ >= kShutdownDurationFrames) {
			TitleScene::SetPendingFadeIn(true);
			SceneManager::GetInstance()->ChangeScene("TITLE");
			return;
		}
	} else {
		transitionFadeAlpha_ = (std::max)(0.0f, transitionFadeAlpha_ - 1.0f / 24.0f);
		const bool isAttractDemoCancelled = isAttractDemoResult_ && Input::GetInstance()->TriggerKey(DIK_TAB);
		if (isAttractDemoCancelled) {
			TitleScene::StopAttractDemo();
		}
		// アトラクトは TAB が押されるまで終了しない。リザルト後はいったんタイトルへ戻り、
		// タイトルから次のゲームプレイを開始する。
		const bool shouldReturnToTitle =
			(!isAttractDemoResult_ && Input::GetInstance()->TriggerKey(DIK_SPACE)) ||
			isAttractDemoCancelled ||
			(isAttractDemoResult_ && resultAnimationFrame_ >= 300);
		if (transitionFadeAlpha_ <= 0.0f && shouldReturnToTitle) {
			isReturningToTitle_ = true;
			returnToTitleFrame_ = 0;
			if (PostEffect::GetInstance()) {
				PostEffect::GetInstance()->SetSystemShutdown(0.0f, 1.0f);
			}
		}
	}
}

void GameOverScene::Draw() {
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	if (resultBackgroundSprite_) {
		resultBackgroundSprite_->Draw();
	}
	if (resultFlowingWorldPrimarySprite_) {
		resultFlowingWorldPrimarySprite_->Draw();
	}
	if (resultFlowingWorldSecondarySprite_) {
		resultFlowingWorldSecondarySprite_->Draw();
	}
	if (resultCloudFlowFarSprite_) {
		resultCloudFlowFarSprite_->Draw();
	}
	if (resultCloudFlowNearSprite_) {
		resultCloudFlowNearSprite_->Draw();
	}
	if (resultStaticAircraftSprite_) {
		resultStaticAircraftSprite_->Draw();
	}
	if (resultHudRingTopRightSprite_) {
		resultHudRingTopRightSprite_->Draw();
	}
	if (resultHudRingBottomLeftSprite_) {
		resultHudRingBottomLeftSprite_->Draw();
	}
	// ImGui のリザルト UI はポストエフェクト後段で描かれるため、黒いマスクで
	// 隠すと画面に黒枠が残る。遷移開始と同時に UI 自体を描かない。
	if (!isReturningToTitle_) {
		ResultOverlay::Draw(MissionResultStore::Get(), resultAnimationFrame_);
	}
	if (transitionFadeSprite_ && !isReturningToTitle_ && transitionFadeAlpha_ > 0.0f) {
		transitionFadeSprite_->SetPosition({ 0.0f, 0.0f });
		transitionFadeSprite_->SetSize({ static_cast<float>(WinApp::GetClientWidth()), static_cast<float>(WinApp::GetClientHeight()) });
		transitionFadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, transitionFadeAlpha_ });
		transitionFadeSprite_->Update();
		transitionFadeSprite_->Draw();
	}
}
