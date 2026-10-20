#include "LoadingScene.h"

#include "2D/SpriteCommon.h"
#include "3D/Object3dCommon.h"
#include "3D/ModelManager.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Scene/SceneManager.h"
#include "engine/base/WinApp.h"
#include "Game/Scene/GamePlayScene.h"
#include "engine/math/MyMath.h"
#include <algorithm>
#include <cmath>
#include <random>

void LoadingScene::Initialize() {
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	Object3dCommon* obj3dCommon = Object3dCommon::GetInstance();

	// 画面全体のホワイトアウト用スプライト（フェーズ1/2およびフェーズ4の移行演出）
	whiteoutSprite_ = std::make_unique<Sprite>();
	whiteoutSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	whiteoutSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	whiteoutSprite_->SetPosition({ 0.0f, 0.0f });
	whiteoutAlpha_ = 1.0f; // ホワイトアウト状態から開始

	// 1. ワープ用3Dシーン（ローディングシーン）の構築
	// 内側を向いた巨大シリンダーモデルのロード
	ModelManager* modelManager = ModelManager::GetInstance();
	modelManager->LoadModel("warp_tunnel.obj");
	modelManager->LoadModel("warp_tunnel_inner.obj");
	modelManager->CreatePlaneModel("warp_speedline_quad");
	modelManager->CreatePlaneModel("warp_plasma_quad");
	modelManager->CreatePlaneModel("warp_core_quad");

	if (Model* speedlineModel = modelManager->FindModel("warp_speedline_quad")) {
		speedlineModel->SetTextureFilePath("resources/warp_speedline_particle.png");
		speedlineModel->SetEnableLighting(false);
	}
	if (Model* plasmaModel = modelManager->FindModel("warp_plasma_quad")) {
		plasmaModel->SetTextureFilePath("resources/warp_speedline_particle.png");
		plasmaModel->SetEnableLighting(false);
	}
	if (Model* coreModel = modelManager->FindModel("warp_core_quad")) {
		coreModel->SetTextureFilePath("resources/warp_flare_core.png");
		coreModel->SetEnableLighting(false);
	}

	// 外側シリンダー（ネビュラ＋星屑）
	tunnelObject_ = std::make_unique<Object3d>();
	tunnelObject_->Initialize(obj3dCommon);
	tunnelObject_->SetModel("warp_tunnel.obj");
	tunnelObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
	tunnelObject_->SetScale({ 1.0f, 1.0f, 1.0f });
	tunnelObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
	if (Model* tunnelModel = tunnelObject_->GetModel()) {
		tunnelModel->SetEnableLighting(false);
	}

	// 内側シリンダー（高速ストリームライン・加算半透明）
	innerTunnelObject_ = std::make_unique<Object3d>();
	innerTunnelObject_->Initialize(obj3dCommon);
	innerTunnelObject_->SetModel("warp_tunnel_inner.obj");
	innerTunnelObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
	innerTunnelObject_->SetScale({ 1.0f, 1.0f, 1.0f });
	innerTunnelObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
	if (Model* innerModel = innerTunnelObject_->GetModel()) {
		innerModel->SetEnableLighting(false);
	}

	// 奥の中心光源（ワープゲートコア・特異点）
	warpCoreObject_ = std::make_unique<Object3d>();
	warpCoreObject_->Initialize(obj3dCommon);
	warpCoreObject_->SetModel("warp_core_quad");
	warpCoreObject_->SetTranslate({ 0.0f, 0.0f, 170.0f });
	warpCoreObject_->SetScale({ warpCoreScale_, warpCoreScale_, 1.0f });

	// 2. 自機モデルとカメラのセットアップ
	warpCamera_ = std::make_unique<Camera>();
	warpCamera_->SetTranslate({ 0.0f, 1.15f, -5.4f });
	obj3dCommon->SetDefaultCamera(warpCamera_.get());

	warpPlayer_ = std::make_unique<Player>();
	warpPlayer_->Initialize("vf-15c/scene.gltf");
	warpPlayer_->SetScale({ 0.28f, 0.28f, 0.28f });
	// エンジン部分のスラスターエフェクトを超音速ワープ用に通常時より大幅に強化発光
	if (warpPlayer_->GetBoosterEffect()) {
		BoosterEffect::EffectSettings settings = warpPlayer_->GetBoosterEffect()->GetEffectSettings();
		// 少し引いた後方視点でも、ノズルから続く炎と光跡を読み取れる強さにする。
		settings.brightness = 5.6f;
		settings.glowScale = 3.2f;
		settings.lengthScale = 4.0f;
		settings.boostWidthMultiplier = 1.8f;
		settings.boostLengthMultiplier = 2.2f;
		warpPlayer_->GetBoosterEffect()->SetEffectSettings(settings);
	}

	// 3. パーティクルエフェクトの追加（スピードライン＆プラズマ）
	std::mt19937 rng(1337);
	std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
	std::uniform_real_distribution<float> radiusDist(2.2f, 14.5f);
	std::uniform_real_distribution<float> zDist(-15.0f, 160.0f);
	std::uniform_real_distribution<float> speedDist(220.0f, 320.0f);
	std::uniform_real_distribution<float> lenDist(3.0f, 7.5f);
	std::uniform_real_distribution<float> widthDist(0.06f, 0.12f);
	std::uniform_real_distribution<float> alphaDist(0.55f, 0.95f);

	for (size_t i = 0; i < kSpeedLineCount; ++i) {
		SpeedLine& line = speedLines_[i];
		line.angle = angleDist(rng);
		line.radius = radiusDist(rng);
		line.speed = speedDist(rng);
		line.length = lenDist(rng);
		line.width = widthDist(rng);
		line.alpha = alphaDist(rng);
		line.position = {
			std::cos(line.angle) * line.radius,
			std::sin(line.angle) * line.radius * 0.85f,
			zDist(rng)
		};
		// シアン〜白〜淡い青
		const float t = static_cast<float>(i % 3) / 2.0f;
		line.color = { 0.35f + t * 0.45f, 0.80f + t * 0.20f, 1.0f, line.alpha };

		speedLineObjects_[i] = std::make_unique<Object3d>();
		speedLineObjects_[i]->Initialize(obj3dCommon);
		speedLineObjects_[i]->SetModel("warp_speedline_quad");
		speedLineObjects_[i]->SetTranslate(line.position);
	}

	std::uniform_real_distribution<float> lifeDist(0.15f, 0.35f);
	std::uniform_real_distribution<float> pOffsetDist(-0.7f, 0.7f);
	for (size_t i = 0; i < kPlasmaCount; ++i) {
		PlasmaParticle& p = plasmaParticles_[i];
		p.life = lifeDist(rng) * (static_cast<float>(i) / static_cast<float>(kPlasmaCount));
		p.maxLife = lifeDist(rng);
		p.position = { pOffsetDist(rng) * 0.9f, pOffsetDist(rng) * 0.4f, pOffsetDist(rng) * 0.8f };
		p.velocity = { pOffsetDist(rng) * 0.5f, pOffsetDist(rng) * 0.5f, -35.0f - (i % 5) * 5.0f };
		p.size = 0.12f + (i % 4) * 0.05f;
		p.color = { 0.3f, 0.75f, 1.0f, 0.85f };

		plasmaObjects_[i] = std::make_unique<Object3d>();
		plasmaObjects_[i]->Initialize(obj3dCommon);
		plasmaObjects_[i]->SetModel("warp_plasma_quad");
		plasmaObjects_[i]->SetTranslate(p.position);
	}

	// 4. 非同期ロード（Async Load）の制御
	preparedScene_ = SceneManager::GetInstance()->TakePreparedScene();
	// DirectX12の描画コンテキスト競合を防ぐため、バックグラウンドスレッドでの
	// GPUリソース/デスクリプタヒープ初期化は行わず、演出完了後にメインスレッドで安全に初期化・遷移する。
	isAsyncLoadCompleted_.store(true);

	startTime_ = Clock::now();
	lastFrameTime_ = startTime_;
	warpFrames_ = 0;
	exitFrames_ = 0;
	phase_ = TransitionPhase::WhiteIn;
}

void LoadingScene::Finalize() {
	if (asyncLoadThread_.joinable()) {
		asyncLoadThread_.join();
	}

	tunnelObject_.reset();
	innerTunnelObject_.reset();
	warpCoreObject_.reset();
	for (auto& obj : speedLineObjects_) {
		obj.reset();
	}
	for (auto& obj : plasmaObjects_) {
		obj.reset();
	}
	warpPlayer_.reset();
	warpCamera_.reset();
	whiteoutSprite_.reset();
	if (preparedScene_) {
		preparedScene_->Finalize();
		preparedScene_.reset();
	}

	// 解放されたwarpCamera_のポインタがObject3dCommonに残存するのを確実に防ぐ
	Object3dCommon::GetInstance()->SetDefaultCamera(nullptr);
}

void LoadingScene::Update() {
	const auto now = Clock::now();
	const float elapsedSeconds = std::chrono::duration<float>(now - startTime_).count();
	float dt = std::chrono::duration<float>(now - lastFrameTime_).count();
	lastFrameTime_ = now;
	if (dt <= 0.0f || dt > 0.1f) {
		dt = 1.0f / 60.0f;
	}

	++warpFrames_;

	// シーン遷移処理フロー
	switch (phase_) {
	case TransitionPhase::WhiteIn:
		// フェーズ2: 「3Dワープシーン」をロードしアクティブにした直後、ホワイトアウトが晴れる
		whiteoutAlpha_ = std::clamp(1.0f - static_cast<float>(warpFrames_) / static_cast<float>(kWhiteInFrames), 0.0f, 1.0f);
		if (warpFrames_ >= kWhiteInFrames) {
			whiteoutAlpha_ = 0.0f;
			phase_ = TransitionPhase::WarpLoop;
		}
		break;

	case TransitionPhase::WarpLoop:
		whiteoutAlpha_ = 0.0f;
		// フェーズ3: ワープ演出をループ再生したまま、バックグラウンドで非同期読み込み
		// 最低演出時間（約2.5秒）を満たし、かつ非同期ロードが完了している場合にフェーズ4へ移行
		if (warpFrames_ >= kMinWarpFrames && isAsyncLoadCompleted_.load()) {
			phase_ = TransitionPhase::WhiteOutExit;
			exitFrames_ = 0;
		}
		break;

	case TransitionPhase::WhiteOutExit:
		// フェーズ4: ロード完了後、進行方向の光が拡大し再びホワイトアウト
		++exitFrames_;
		{
			const float exitProgress = std::clamp(
				static_cast<float>(exitFrames_) / static_cast<float>(kExitWhiteOutFrames), 0.0f, 1.0f);
			const float easedExit = exitProgress * exitProgress * (3.0f - 2.0f * exitProgress);
			// 進行方向奥の光源が急激に拡大
			warpCoreScale_ = 28.0f + exitProgress * exitProgress * 320.0f;
			whiteoutAlpha_ = std::clamp(easedExit * 1.15f, 0.0f, 1.0f);

			if (exitFrames_ >= kExitWhiteOutFrames) {
				// ロード完了＆ホワイトアウト最大到達: 常にクリーンな状態でゲーム本編へ新規切り替え
				if (asyncLoadThread_.joinable()) {
					asyncLoadThread_.join();
				}
				if (preparedScene_) {
					preparedScene_->Finalize();
					preparedScene_.reset();
				}
				SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
				return;
			}
		}
		break;
	}

	// 1 & 2. 3Dワープ空間の更新（UVスクロール、自機飛行、カメラシェイク）
	UpdateWarpFlight(elapsedSeconds, dt);

	// 3. パーティクルエフェクトの更新（スピードライン、プラズマ）
	UpdateSpeedLines(dt);
	UpdatePlasmaParticles(dt);

	// 奥の中心光源（ワープコア）の更新
	warpCoreRotation_ += dt * 1.8f;
	if (warpCoreObject_) {
		warpCoreObject_->SetTranslate({ 0.0f, 0.0f, 170.0f });
		warpCoreObject_->SetScale({ warpCoreScale_, warpCoreScale_, 1.0f });
		warpCoreObject_->SetRotate({ 0.0f, 0.0f, warpCoreRotation_ });
		warpCoreObject_->Update(warpCamera_.get());
	}

	// ホワイトアウトスプライトの更新
	if (whiteoutSprite_) {
		const float width = static_cast<float>(WinApp::GetClientWidth());
		const float height = static_cast<float>(WinApp::GetClientHeight());
		whiteoutSprite_->SetSize({ width, height });
		whiteoutSprite_->SetColor({ 1.0f, 1.0f, 1.0f, whiteoutAlpha_ });
		whiteoutSprite_->Update();
	}
}

void LoadingScene::UpdateWarpFlight(float elapsedSeconds, float dt) {
	// シリンダーのUVスクロール（奥から手前へ光の筋と星屑を高速で流す）
	uvScrollOffsetOuter_ -= dt * 3.8f;
	uvScrollOffsetInner_ -= dt * 6.8f;
	innerTunnelRotation_ += dt * 0.40f;

	if (tunnelObject_) {
		if (Model* tunnelModel = tunnelObject_->GetModel()) {
			tunnelModel->SetUvTransform(MyMath::MakeTranslateMatrix({ 0.0f, uvScrollOffsetOuter_, 0.0f }));
		}
		tunnelObject_->Update(warpCamera_.get());
	}
	if (innerTunnelObject_) {
		if (Model* innerModel = innerTunnelObject_->GetModel()) {
			innerModel->SetUvTransform(MyMath::MakeTranslateMatrix({ 0.0f, uvScrollOffsetInner_, 0.0f }));
		}
		innerTunnelObject_->SetRotate({ 0.0f, 0.0f, innerTunnelRotation_ });
		innerTunnelObject_->Update(warpCamera_.get());
	}

	if (!warpPlayer_ || !warpCamera_) {
		return;
	}

	// 超音速飛行の負荷表現: カメラシェイクと機体微小ランダムノイズ
	const float shakeIntensity = 0.032f;
	const float shakeX = (std::sin(elapsedSeconds * 47.0f) * 0.55f + std::sin(elapsedSeconds * 83.0f) * 0.45f) * shakeIntensity;
	const float shakeY = (std::cos(elapsedSeconds * 53.0f) * 0.55f + std::sin(elapsedSeconds * 71.0f) * 0.45f) * shakeIntensity;
	const float shakeRoll = std::sin(elapsedSeconds * 39.0f) * 0.007f;

	// 自機モデルの微小な乱気流・G負荷揺らぎ
	// カメラは変えずに自機だけをトンネルの奥・画面下側へ置く。後部ノズルが
	// 完全に見え、さらにその先のワープコアを中央に残せる配置にする。
	const Vector3 playerPosition = {
		std::sin(elapsedSeconds * 1.6f) * 0.14f + shakeX * 0.35f,
		-0.15f + std::sin(elapsedSeconds * 2.2f) * 0.09f + std::sin(elapsedSeconds * 31.0f) * 0.015f,
		7.0f
	};
	const Vector3 playerRotation = {
		-0.035f + std::cos(elapsedSeconds * 1.9f) * 0.02f,
		std::sin(elapsedSeconds * 1.6f) * 0.035f,
		-std::sin(elapsedSeconds * 1.6f) * 0.07f + std::sin(elapsedSeconds * 33.0f) * 0.008f
	};
	// 通常時より強く発光するスラスターと共に機体を描画更新
	warpPlayer_->UpdatePresentation(playerPosition, playerRotation, 2.5f, true);

	// TPS視点カメラのセットアップ（機体後方から進行方向奥の光源を見据える）
	const Vector3 cameraPosition = { shakeX, 1.15f + shakeY, -5.4f };
	const Vector3 toTarget = {
		playerPosition.x * 0.45f - cameraPosition.x,
		(playerPosition.y * 0.45f + 0.15f) - cameraPosition.y,
		25.0f - cameraPosition.z
	};
	const float length = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
	if (length > 0.0001f) {
		const Vector3 direction = { toTarget.x / length, toTarget.y / length, toTarget.z / length };
		const Quaternion pitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, -std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
		const Quaternion yaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, std::atan2(direction.x, direction.z));
		const Quaternion roll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, shakeRoll);
		warpCamera_->SetQuaternion(MyMath::Normalize(MyMath::Multiply(roll, MyMath::Multiply(yaw, pitch))));
	}
	warpCamera_->SetTranslate(cameraPosition);
	warpCamera_->Update();
}

void LoadingScene::UpdateSpeedLines(float dt) {
	std::mt19937 rng(42 + warpFrames_);
	std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
	std::uniform_real_distribution<float> radiusDist(2.2f, 14.5f);
	std::uniform_real_distribution<float> speedDist(220.0f, 320.0f);
	std::uniform_real_distribution<float> lenDist(3.5f, 8.0f);

	for (size_t i = 0; i < kSpeedLineCount; ++i) {
		SpeedLine& line = speedLines_[i];
		// 奥から手前（-Z方向）へ高速前進
		line.position.z -= line.speed * dt;

		// 手前を通過したら奥（Z = 160m付近）へ再配置してループ
		if (line.position.z < -16.0f) {
			line.position.z = 155.0f + (i % 8) * 2.0f;
			line.angle = angleDist(rng);
			line.radius = radiusDist(rng);
			line.speed = speedDist(rng);
			line.length = lenDist(rng);
			line.position.x = std::cos(line.angle) * line.radius;
			line.position.y = std::sin(line.angle) * line.radius * 0.85f;
		}

		if (speedLineObjects_[i]) {
			speedLineObjects_[i]->SetTranslate(line.position);
			speedLineObjects_[i]->SetScale({ line.width, line.length, 1.0f });
			// 進行方向（Z軸）に沿って細長く見えるようにX軸回転90度
			speedLineObjects_[i]->SetRotate({ 1.5707963f, 0.0f, line.angle });
			if (Model* m = speedLineObjects_[i]->GetModel()) {
				m->SetColor(line.color);
			}
			speedLineObjects_[i]->Update(warpCamera_.get());
		}
	}
}

void LoadingScene::UpdatePlasmaParticles(float dt) {
	std::mt19937 rng(100 + warpFrames_);
	std::uniform_real_distribution<float> pOffsetDist(-0.75f, 0.75f);
	std::uniform_real_distribution<float> lifeDist(0.18f, 0.35f);

	for (size_t i = 0; i < kPlasmaCount; ++i) {
		PlasmaParticle& p = plasmaParticles_[i];
		p.life += dt;
		p.position.x += p.velocity.x * dt;
		p.position.y += p.velocity.y * dt;
		p.position.z += p.velocity.z * dt;

		if (p.life >= p.maxLife) {
			p.life = 0.0f;
			p.maxLife = lifeDist(rng);
			// 機体周囲（主翼・機首近傍）から再発生
			const float side = (i % 2 == 0) ? 1.0f : -1.0f;
			p.position = {
				side * (0.25f + std::abs(pOffsetDist(rng)) * 0.65f),
				pOffsetDist(rng) * 0.22f,
				pOffsetDist(rng) * 0.7f
			};
			p.velocity = {
				pOffsetDist(rng) * 0.8f,
				pOffsetDist(rng) * 0.8f,
				-28.0f - (i % 6) * 6.0f
			};
			p.size = 0.10f + (i % 4) * 0.04f;
		}

		if (plasmaObjects_[i]) {
			const float lifeRatio = 1.0f - (p.life / p.maxLife);
			const float alpha = lifeRatio * 0.85f;
			plasmaObjects_[i]->SetTranslate(p.position);
			plasmaObjects_[i]->SetScale({ p.size * lifeRatio, p.size * 2.0f, 1.0f });
			plasmaObjects_[i]->SetRotate({ 1.5707963f, 0.0f, 0.0f });
			if (Model* m = plasmaObjects_[i]->GetModel()) {
				m->SetColor({ p.color.x, p.color.y, p.color.z, alpha });
			}
			plasmaObjects_[i]->Update(warpCamera_.get());
		}
	}
}

void LoadingScene::Draw() {
	if (!warpCamera_) {
		return;
	}

	Object3dCommon* obj3dCommon = Object3dCommon::GetInstance();
	obj3dCommon->SetDefaultCamera(warpCamera_.get());

	// 1. ワープトンネル（外側シリンダー: 不透明・星屑とネビュラ）
	obj3dCommon->SetCommonDrawSettings();
	if (tunnelObject_) {
		tunnelObject_->Draw();
	}

	// 2. 内側シリンダー（加算ブレンド: 高速ストリームライン）
	obj3dCommon->SetEffectDrawSettings();
	if (innerTunnelObject_) {
		innerTunnelObject_->Draw();
	}

	// 3. 進行方向奥の中心光源（特異点・ワープゲートコア）
	if (warpCoreObject_) {
		warpCoreObject_->Draw();
	}

	// 4. スピードライン（進行方向奥から手前へ疾走する光の筋）
	for (auto& obj : speedLineObjects_) {
		if (obj) {
			obj->Draw();
		}
	}

	// 5. 機体周囲のプラズマパーティクル
	for (auto& obj : plasmaObjects_) {
		if (obj) {
			obj->Draw();
		}
	}

	// 6. 戦闘機モデルと強力なスラスターエフェクト（手前に最前面描画）
	obj3dCommon->SetCommonDrawSettings();
	if (warpPlayer_) {
		warpPlayer_->Draw(warpCamera_.get(), true);
	}

	// 7. 画面全体の強い発光エフェクト（ホワイトアウト / フェードイン・フェードアウト）
	if (whiteoutSprite_ && whiteoutAlpha_ > 0.001f) {
		SpriteCommon::GetInstance()->SetCommonPipelineState();
		whiteoutSprite_->Draw();
	}
}
