#include "TitleScene.h"
#include "TitleOverlayRenderer.h"
#include "3D/Object3dCommon.h"
#include "2D/SpriteCommon.h"
#include "engine/Input/Input.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Audio/AudioManager.h"
#include "engine/Debug/ImGuiManager.h"
#include "engine/base/WinApp.h"
#include "Game/base/GameSettings.h"
#include "Game/base/GameStartTransition.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <shellapi.h>
#include <externals/json.hpp>
#include <externals/imgui/imgui.h>

namespace {
	constexpr int kLaunchDurationFrames = 84;
	// START GAME 決定後も、タイトルUIを約0.33秒だけ残して自然に消す。
	constexpr int kLaunchUiFadeFrames = 20;
	constexpr const char* kRuntimeOptionsPath = "resources/runtime_options.json";

	struct AttractDemoRuntimeOptions {
		bool allowAttractDemo = true;
		bool playClearCinematicDemo = false;
	};

	// 開発版には設定ファイルを置かないため、未配置時は従来どおり通常デモを有効にする。
	// 配布ZIPでファイルが壊れている場合は、意図しないデモの露出を避けるため無効化する。
	AttractDemoRuntimeOptions LoadAttractDemoRuntimeOptions() {
		std::ifstream input(kRuntimeOptionsPath);
		if (!input.is_open()) {
			return {};
		}

		try {
			nlohmann::json options;
			input >> options;
			return {
				options.value("allowAttractDemo", false),
				options.value("playClearCinematicDemo", false),
			};
		} catch (const std::exception& e) {
			OutputDebugStringA((std::string("[TitleScene] Failed to load runtime_options.json: ") + e.what() + "\n").c_str());
			return { false, false };
		}
	}

}

void TitleScene::Initialize() {
	const AttractDemoRuntimeOptions demoOptions = LoadAttractDemoRuntimeOptions();
	isAttractDemoAvailable_ = demoOptions.allowAttractDemo;
	playClearCinematicDemo_ = isAttractDemoAvailable_ && demoOptions.playClearCinematicDemo;
	if (!isAttractDemoAvailable_) {
		StopAttractDemo();
	}

	// ポストエフェクトを通常状態にクリアする
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	// カメラ・シーンリソース
	camera = std::make_unique<Camera>();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());

	titleSprite = std::make_unique<Sprite>();
	titleSprite->Initialize(SpriteCommon::GetInstance(), "resources/title_logo.png");
	titleSprite->SetPosition({ 640.0f, 360.0f });
	titleSprite->SetAnchorPoint({ 0.5f, 0.5f });
	// title_logo.png の透明余白を除き、文字だけを上部へ描く。
	titleSprite->SetTextureLeftTop({ 81.0f, 572.0f });
	titleSprite->SetTextureSize({ 1503.0f, 331.0f });
	titleSprite->SetSize({ 920.0f, 203.0f });

	// タイトルロゴの書体に合わせて作成したメニュー文字スプライト。
	menuPanelSprite_ = std::make_unique<Sprite>();
	menuPanelSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	menuRowSprite_ = std::make_unique<Sprite>();
	menuRowSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	titleControlsGuideSprite_ = std::make_unique<Sprite>();
	titleControlsGuideSprite_->Initialize(SpriteCommon::GetInstance(), "resources/title_controls_simple.png");
	for (auto& label : menuLabelSprites_) {
		label = std::make_unique<Sprite>();
		label->Initialize(SpriteCommon::GetInstance(), "resources/title_menu_labels_simple.png");
	}
	for (auto& segment : settingMeterSegments_) {
		segment = std::make_unique<Sprite>();
		segment->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	}
	for (auto& digitRow : settingValueDigitSprites_) {
		for (auto& digit : digitRow) {
			digit = std::make_unique<Sprite>();
			digit->Initialize(SpriteCommon::GetInstance(), "resources/hud_digits.png");
		}
	}
	for (auto& streak : foldStreakSprites_) {
		streak = std::make_unique<Sprite>();
		// 丸い発光球ではなく、透明な雲・霧ノイズの断片を重ねて空間のねじれを作る。
		streak->Initialize(SpriteCommon::GetInstance(), "resources/fold_cloud_noise.png");
		streak->SetAnchorPoint({ 0.5f, 0.5f });
	}
	foldVeilSprite_ = std::make_unique<Sprite>();
	foldVeilSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	foldDepthSprite_ = std::make_unique<Sprite>();
	foldDepthSprite_->Initialize(SpriteCommon::GetInstance(), "resources/circle2.png");
	foldDepthSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	foldPurpleCoreSprite_ = std::make_unique<Sprite>();
	foldPurpleCoreSprite_->Initialize(SpriteCommon::GetInstance(), "resources/circle2.png");
	foldPurpleCoreSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	foldCoreSprite_ = std::make_unique<Sprite>();
	foldCoreSprite_->Initialize(SpriteCommon::GetInstance(), "resources/fold_cloud_noise.png");
	foldCoreSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	foldBeamSprite_ = std::make_unique<Sprite>();
	foldBeamSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	foldBeamSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	foldFlashSprite_ = std::make_unique<Sprite>();
	foldFlashSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	warpCloudRearSprite_ = std::make_unique<Sprite>();
	warpCloudRearSprite_->Initialize(SpriteCommon::GetInstance(), "resources/warp_cloud_ring.png");
	warpCloudRearSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	warpCloudFrontSprite_ = std::make_unique<Sprite>();
	warpCloudFrontSprite_->Initialize(SpriteCommon::GetInstance(), "resources/warp_cloud_ring.png");
	warpCloudFrontSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	// 本編と同じプレイヤー機を、タイトルでは入力なしの自動飛行として見せる。
	titlePlayer_ = std::make_unique<Player>();
	titlePlayer_->Initialize("vf-15c/scene.gltf");
	launchSoundData_ = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");

	selectedMenuItem_ = MenuItem::Start;
	selectedSettingsItem_ = SettingsItem::MasterVolume;
	isSettingsOpen_ = false;
	flightState_ = TitleFlightState::Cruising;
	titleFlightFrame_ = 0;
	launchFrame_ = 0;
	launchFadeAlpha_ = 0.0f;
	attractDemoTitleFrames_ = 0;
	if (isPendingFadeIn_) {
		titleFadeInAlpha_ = 1.0f;
		isPendingFadeIn_ = false;
	} else {
		titleFadeInAlpha_ = 0.0f;
	}
	// 起動直後のタイトル背景は、専用の飛行デモではなく本編のプレイ背景にする。
	backgroundMode_ = BackgroundMode::GameplayBackground;
	gameplayBackground_ = std::make_unique<GamePlayScene>(GamePlayScene::Mode::TitleBackground);
	gameplayBackground_->Initialize();
	AudioManager::GetInstance()->SetMasterVolume(GameSettings::GetInstance().GetMasterVolume());
}

void TitleScene::Finalize() {
	if (gameplayBackground_) {
		gameplayBackground_->Finalize();
		gameplayBackground_.reset();
	}
	if (launchVoice_) {
		launchVoice_->Stop();
		launchVoice_->DestroyVoice();
		launchVoice_ = nullptr;
	}
	AudioManager::GetInstance()->UnloadWave(launchSoundData_);
}

void TitleScene::Update() {
	if (flightState_ == TitleFlightState::Launching) {
		if (backgroundMode_ == BackgroundMode::GameplayBackground && gameplayBackground_) {
			// 出撃中もタイトルで表示していた地形を描画し続ける。
			UpdateGameplayBackground();
			const float launchProgress = static_cast<float>(launchFrame_) /
				static_cast<float>(kLaunchDurationFrames);
			// フォールド航法光が収束し始めるのに合わせ、中心から外へ流れる残像を強める。
			// 最後の白フラッシュでシーンの作り直しを自然に隠す。
			if (PostEffect::GetInstance() && GameSettings::GetInstance().IsFoldTransitionEnabled()) {
				const float blurProgress = std::clamp((launchProgress - 0.18f) / 0.60f, 0.0f, 1.0f);
				const float easedProgress = blurProgress * blurProgress * (3.0f - 2.0f * blurProgress);
				PostEffect::GetInstance()->SetRadialBlur(1.5f + easedProgress * 12.5f);
			}
			// フォールド開始後だけ背景を少し沈め、航法光を読み取りやすくする。
			launchFadeAlpha_ = GameSettings::GetInstance().IsFoldTransitionEnabled()
				? std::clamp((launchProgress - 0.24f) * 0.26f, 0.0f, 0.16f)
				: 0.0f;
			if (++launchFrame_ >= kLaunchDurationFrames) {
				// ワープ演出（LOADING）へ移行。本編へはワープ後にクリーンな状態で新規初期化する。
				GameStartTransition::Begin({ gameplayBackground_->GetTitleLaunchForward(), 0.0f, 0 });
				SceneManager::GetInstance()->ChangeScene("LOADING");
			}
		} else {
			UpdateTitleFlight();
		}
		return;
	}

	if (titleFadeInAlpha_ > 0.0f) {
		constexpr float kFadeSpeed = 1.0f / 22.0f; // 約0.36秒でスッと明転
		titleFadeInAlpha_ -= kFadeSpeed;
		if (titleFadeInAlpha_ <= 0.0f) {
			titleFadeInAlpha_ = 0.0f;
		}
	}

	Input* input = Input::GetInstance();
	const bool canAcceptInput = (titleFadeInAlpha_ <= 0.15f);
	if (isSettingsOpen_) {
		if (canAcceptInput) {
			UpdateSettingsInput();
		}
	} else if (canAcceptInput) {
		const int itemCount = static_cast<int>(MenuItem::Count);
		int selection = static_cast<int>(selectedMenuItem_);
		const bool navUp = input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W) ||
			input->TriggerControllerInput(ControllerInput::DPadUp) || input->TriggerControllerInput(ControllerInput::LeftStickUp);
		const bool navDown = input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S) ||
			input->TriggerControllerInput(ControllerInput::DPadDown) || input->TriggerControllerInput(ControllerInput::LeftStickDown);

		if (navUp) {
			selection = (selection + itemCount - 1) % itemCount;
		} else if (navDown) {
			selection = (selection + 1) % itemCount;
		}
		selectedMenuItem_ = static_cast<MenuItem>(selection);

		const bool confirm = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) ||
			input->TriggerControllerButton(XINPUT_GAMEPAD_A) || input->TriggerControllerButton(XINPUT_GAMEPAD_START);

		if (confirm) {
			switch (selectedMenuItem_) {
			case MenuItem::Start:
				BeginLaunchSequence();
				return;
			case MenuItem::Settings:
				isSettingsOpen_ = true;
				selectedSettingsItem_ = SettingsItem::MasterVolume;
				break;
			case MenuItem::Exit:
				PostQuitMessage(0);
				return;
			default:
				break;
			}
		}
	}

#if defined(CG2_RELEASE_BUILD)
	if (isAttractDemoAvailable_ && !isSettingsOpen_ && canAcceptInput) {
		if (Input::GetInstance()->TriggerKey(DIK_TAB)) {
			// 通常タイトルでは開始、デモ周回中に表示されたタイトルでは終了する。
			if (isAttractDemoEnabled_) {
				StopAttractDemo();
			} else {
				StartAttractDemo();
				SceneManager::GetInstance()->ChangeScene("ATTRACT_DEMO");
				return;
			}
		}

		// リザルト後はタイトルを一瞬表示して、タイトル→ゲームプレイの順を保ったまま
		// 次のデモ周回を開始する。最初のTAB開始時にはこの待機を通らない。
		if (isAttractDemoEnabled_ && ++attractDemoTitleFrames_ >= 45) {
			SceneManager::GetInstance()->ChangeScene("ATTRACT_DEMO");
			return;
		}
	}
#endif
	DrawBackgroundModeImGui();

	if (titleSprite) {
		float width = static_cast<float>(WinApp::GetClientWidth());
		float height = static_cast<float>(WinApp::GetClientHeight());
		// ロゴは画面上部へ固定し、中央の広い領域を機体の飛行演出に使う。
		const float titleWidth = isSettingsOpen_ ? width * 0.72f : width * 0.78f;
		const float titleHeight = titleWidth * (331.0f / 1503.0f);
		const float titleCenterY = isSettingsOpen_ ? height * 0.22f : height * 0.17f;
		titleSprite->SetPosition({ width * 0.5f, titleCenterY });
		titleSprite->SetSize({ titleWidth, titleHeight });
		titleSprite->Update();
	}

	if (backgroundMode_ == BackgroundMode::GameplayBackground) {
		UpdateGameplayBackground();
	} else {
		UpdateTitleFlight();
	}
}

void TitleScene::ToggleBackgroundMode() {
	if (backgroundMode_ == BackgroundMode::FlightDemo) {
		backgroundMode_ = BackgroundMode::GameplayBackground;
		gameplayBackground_ = std::make_unique<GamePlayScene>(GamePlayScene::Mode::TitleBackground);
		gameplayBackground_->Initialize();
	} else {
		if (gameplayBackground_) {
			gameplayBackground_->Finalize();
			gameplayBackground_.reset();
		}
		backgroundMode_ = BackgroundMode::FlightDemo;
		Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
		UpdateTitleFlight();
	}
}

void TitleScene::UpdateGameplayBackground() {
	if (gameplayBackground_) {
		gameplayBackground_->Update();
	}
}

void TitleScene::DrawBackgroundModeImGui() {
#ifdef ENABLE_IMGUI
	if (!ImGuiManager::IsVisible() || ImGui::GetCurrentContext() == nullptr) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(330.0f, 0.0f), ImGuiCond_Once);
	ImGui::SetNextWindowBgAlpha(0.88f);
	if (ImGui::Begin("Title Background Preview")) {
		ImGui::Text("Title background");
		ImGui::Separator();

		const bool isFlightDemo = backgroundMode_ == BackgroundMode::FlightDemo;
		if (ImGui::RadioButton("FLIGHT DEMO", isFlightDemo) && !isFlightDemo) {
			ToggleBackgroundMode();
		}
		ImGui::TextDisabled("Title-only aircraft flight sequence");

		const bool isGameplayBackground = backgroundMode_ == BackgroundMode::GameplayBackground;
		if (ImGui::RadioButton("GAMEPLAY BACKGROUND", isGameplayBackground) && !isGameplayBackground) {
			ToggleBackgroundMode();
		}
		ImGui::TextDisabled("Run the gameplay world behind the title");

		ImGui::Separator();
		bool foldTransitionEnabled = GameSettings::GetInstance().IsFoldTransitionEnabled();
		if (ImGui::Checkbox("Fold transition", &foldTransitionEnabled)) {
			GameSettings::GetInstance().SetFoldTransitionEnabled(foldTransitionEnabled);
		}

		ImGui::Separator();
		ImGui::Text("Audio (saved automatically)");
		GameSettings& settings = GameSettings::GetInstance();
		bool gameplayBgmEnabled = settings.IsGameplayBgmEnabled();
		if (ImGui::Checkbox("Gameplay BGM", &gameplayBgmEnabled)) {
			settings.SetGameplayBgmEnabled(gameplayBgmEnabled);
		}
		bool nonGameplayBgmEnabled = settings.IsNonGameplayBgmEnabled();
		if (ImGui::Checkbox("Non-gameplay BGM", &nonGameplayBgmEnabled)) {
			settings.SetNonGameplayBgmEnabled(nonGameplayBgmEnabled);
		}
		bool soundEffectsEnabled = settings.IsSoundEffectsEnabled();
		if (ImGui::Checkbox("Sound effects", &soundEffectsEnabled)) {
			settings.SetSoundEffectsEnabled(soundEffectsEnabled);
		}
		ImGui::TextDisabled("Saved automatically");
	}
	ImGui::End();
#endif
}

void TitleScene::BeginLaunchSequence() {
	// 手動開始を選んだら、以後はアトラクトデモを再開しない。
#if defined(CG2_RELEASE_BUILD)
	StopAttractDemo();
#endif
	// タイトル背景の地形を消さずに、そのまま出撃演出へ移る。
	if (gameplayBackground_) {
		gameplayBackground_->BeginTitleLaunch();
	}
	flightState_ = TitleFlightState::Launching;
	launchFrame_ = 0;
	launchFadeAlpha_ = 0.0f;
	if (launchVoice_) {
		launchVoice_->Stop();
		launchVoice_->DestroyVoice();
	}
	// 手持ち素材は出撃警報として使う。SEが無効なら鳴らさない。
	if (GameSettings::GetInstance().IsSoundEffectsEnabled()) {
		launchVoice_ = AudioManager::GetInstance()->PlayWave(launchSoundData_);
	}
}

void TitleScene::UpdateTitleFlight() {
	if (!titlePlayer_ || !camera) {
		return;
	}

	Vector3 position{};
	Vector3 rotation{};
	Vector3 cameraPosition{};
	Vector3 cameraTarget{};
	float speed = 0.08f;
	bool isBoosting = false;

	if (flightState_ == TitleFlightState::Cruising) {
		const float time = static_cast<float>(titleFlightFrame_++) * (1.0f / 60.0f);
		position = {
			std::sin(time * 0.45f) * 4.0f,
			0.35f + std::sin(time * 0.82f) * 0.95f,
			4.2f + std::cos(time * 0.30f) * 0.65f,
		};
		rotation = {
			std::sin(time * 0.82f) * 0.09f,
			std::sin(time * 0.45f) * 0.22f,
			-std::sin(time * 0.45f) * 0.16f,
		};
		// 横方向は画面端近くまで見せ、縦方向は強めに追従させる。
		// これにより飛行範囲を広く保ちながら、機体の大半が画面内に残る。
		cameraPosition = { position.x * 0.25f, 1.15f + (position.y - 0.35f) * 0.20f, -5.2f };
		cameraTarget = {
			position.x * 0.50f,
			0.60f + (position.y - 0.35f) * 0.80f,
			4.2f,
		};
	} else {
		const float progress = std::clamp(
			static_cast<float>(launchFrame_) / static_cast<float>(kLaunchDurationFrames), 0.0f, 1.0f);
		if (PostEffect::GetInstance() && GameSettings::GetInstance().IsFoldTransitionEnabled()) {
			const float blurProgress = std::clamp((progress - 0.18f) / 0.60f, 0.0f, 1.0f);
			const float easedProgress = blurProgress * blurProgress * (3.0f - 2.0f * blurProgress);
			PostEffect::GetInstance()->SetRadialBlur(1.5f + easedProgress * 12.5f);
		}
		const float acceleration = progress * progress;
		speed = 0.18f + acceleration * 1.35f;
		position = {
			(1.0f - progress) * std::sin(static_cast<float>(titleFlightFrame_) * 0.015f) * 1.6f,
			0.35f + progress * 0.55f,
			4.0f + acceleration * 112.0f,
		};
		rotation = {
			-progress * 0.06f,
			(1.0f - progress) * std::sin(static_cast<float>(titleFlightFrame_) * 0.015f) * 0.52f,
			(1.0f - progress) * -std::sin(static_cast<float>(titleFlightFrame_) * 0.015f) * 0.32f,
		};
		cameraPosition = { 0.0f, 1.15f + progress * 0.45f, -5.2f + acceleration * 55.0f };
		cameraTarget = position;
		cameraTarget.y += 0.25f;
		launchFadeAlpha_ = std::clamp((progress - 0.58f) / 0.42f, 0.0f, 1.0f);
		isBoosting = true;

		if (++launchFrame_ >= kLaunchDurationFrames) {
			// 専用飛行デモから開始する場合も、自動ブーストで入力を奪わない。
			GameStartTransition::Begin({ { 0.0f, 0.0f, 1.0f }, 0.0f, 0 });
			SceneManager::GetInstance()->ChangeScene("LOADING");
		}
	}

	titlePlayer_->UpdatePresentation(position, rotation, speed, isBoosting);

	const Vector3 toTarget = {
		cameraTarget.x - cameraPosition.x,
		cameraTarget.y - cameraPosition.y,
		cameraTarget.z - cameraPosition.z,
	};
	const float distance = std::sqrt(
		toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
	if (distance > 0.0001f) {
		const Vector3 direction = { toTarget.x / distance, toTarget.y / distance, toTarget.z / distance };
		const float pitch = -std::asin(std::clamp(direction.y, -1.0f, 1.0f));
		const float yaw = std::atan2(direction.x, direction.z);
		const Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
		const Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
		camera->SetQuaternion(MyMath::Normalize(MyMath::Multiply(qYaw, qPitch)));
	}
	camera->SetTranslate(cameraPosition);
	camera->Update();
}

void TitleScene::UpdateSettingsInput() {
	Input* input = Input::GetInstance();
	if (input->TriggerKey(DIK_ESCAPE) || input->TriggerControllerButton(XINPUT_GAMEPAD_B)) {
		isSettingsOpen_ = false;
		return;
	}

	const int itemCount = static_cast<int>(SettingsItem::Count);
	int selection = static_cast<int>(selectedSettingsItem_);
	const bool navUp = input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W) ||
		input->TriggerControllerInput(ControllerInput::DPadUp) || input->TriggerControllerInput(ControllerInput::LeftStickUp);
	const bool navDown = input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S) ||
		input->TriggerControllerInput(ControllerInput::DPadDown) || input->TriggerControllerInput(ControllerInput::LeftStickDown);

	if (navUp) {
		selection = (selection + itemCount - 1) % itemCount;
	} else if (navDown) {
		selection = (selection + 1) % itemCount;
	}
	selectedSettingsItem_ = static_cast<SettingsItem>(selection);

	const bool decrease = input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_A) ||
		input->TriggerControllerInput(ControllerInput::DPadLeft) || input->TriggerControllerInput(ControllerInput::LeftStickLeft);
	const bool increase = input->TriggerKey(DIK_RIGHT) || input->TriggerKey(DIK_D) ||
		input->TriggerControllerInput(ControllerInput::DPadRight) || input->TriggerControllerInput(ControllerInput::LeftStickRight);
	const bool confirm = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) ||
		input->TriggerControllerButton(XINPUT_GAMEPAD_A);

	GameSettings& settings = GameSettings::GetInstance();
	if (selectedSettingsItem_ == SettingsItem::MasterVolume && (decrease || increase)) {
		settings.SetMasterVolume(settings.GetMasterVolume() + (increase ? 0.05f : -0.05f));
		AudioManager::GetInstance()->SetMasterVolume(settings.GetMasterVolume());
	} else if (selectedSettingsItem_ == SettingsItem::MouseSensitivity && (decrease || increase)) {
		settings.SetMouseSensitivity(settings.GetMouseSensitivity() + (increase ? 0.0005f : -0.0005f));
	} else if (selectedSettingsItem_ == SettingsItem::ControlGuide && (decrease || increase || confirm)) {
		settings.SetControlGuideVisible(!settings.IsControlGuideVisible());
	}

	if (confirm && selectedSettingsItem_ == SettingsItem::Back) {
		isSettingsOpen_ = false;
	}
}

void TitleScene::Draw() {
	// 3Dオブジェクトの描画準備
	Object3dCommon::GetInstance()->SetCommonDrawSettings();
	if (backgroundMode_ == BackgroundMode::GameplayBackground && gameplayBackground_) {
		gameplayBackground_->Draw();
	} else if (titlePlayer_) {
		titlePlayer_->Draw(camera.get());
	}

	SpriteCommon::GetInstance()->SetCommonPipelineState();
	const float launchUiAlpha = flightState_ == TitleFlightState::Launching
		? std::clamp(1.0f - static_cast<float>(launchFrame_) / static_cast<float>(kLaunchUiFadeFrames), 0.0f, 1.0f)
		: 1.0f;
	const bool isLaunchUiVisible = flightState_ == TitleFlightState::Launching && launchUiAlpha > 0.0f;
	// 背景の飛行演出を見せるため、待機中は暗幕を半透明にする。
	if (menuPanelSprite_) {
		menuPanelSprite_->SetPosition({ 0.0f, 0.0f });
		menuPanelSprite_->SetSize({
			static_cast<float>(WinApp::GetClientWidth()),
			static_cast<float>(WinApp::GetClientHeight())
		});
		const float panelAlpha = flightState_ == TitleFlightState::Launching
			? (std::max)(launchUiAlpha * 0.48f, launchFadeAlpha_)
			: (isSettingsOpen_ ? 0.88f : 0.48f);
		menuPanelSprite_->SetColor({ 0.0f, 0.0f, 0.0f, panelAlpha });
		menuPanelSprite_->Update();
		menuPanelSprite_->Draw();
	}
	if (titleSprite && !isSettingsOpen_ &&
		(flightState_ == TitleFlightState::Cruising || isLaunchUiVisible)) {
		titleSprite->SetColor({ 1.0f, 1.0f, 1.0f, launchUiAlpha });
		titleSprite->Update();
		titleSprite->Draw();
	}
	if (flightState_ == TitleFlightState::Launching && GameSettings::GetInstance().IsFoldTransitionEnabled()) {
		DrawWarpCloudLayers(
			static_cast<float>(WinApp::GetClientWidth()),
			static_cast<float>(WinApp::GetClientHeight()));
		DrawFoldTransition(
			static_cast<float>(WinApp::GetClientWidth()),
			static_cast<float>(WinApp::GetClientHeight()));
	}

	if (flightState_ == TitleFlightState::Cruising || isLaunchUiVisible) {
		DrawMenuOverlay(static_cast<float>(WinApp::GetClientWidth()), static_cast<float>(WinApp::GetClientHeight()), launchUiAlpha);
		DrawAttractDemoStartPrompt(
			static_cast<float>(WinApp::GetClientWidth()),
			static_cast<float>(WinApp::GetClientHeight()));
		// タイトルメニュー専用の選択・決定操作を、選択肢と重ならない右下へ表示する。
		if (!isSettingsOpen_ && titleControlsGuideSprite_) {
			constexpr float kGuideWidth = 330.0f;
			constexpr float kGuideHeight = 103.0f;
			constexpr float kGuideMargin = 18.0f;
			titleControlsGuideSprite_->SetPosition({
				(static_cast<float>(WinApp::GetClientWidth()) - kGuideWidth - kGuideMargin),
				(static_cast<float>(WinApp::GetClientHeight()) - kGuideHeight - kGuideMargin),
			});
			titleControlsGuideSprite_->SetSize({ kGuideWidth, kGuideHeight });
			titleControlsGuideSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.96f * launchUiAlpha });
			titleControlsGuideSprite_->Update();
			titleControlsGuideSprite_->Draw();
		}
	}

	// シャットダウン遷移から戻った際の黒からのフェードイン
	if (titleFadeInAlpha_ > 0.0f && menuPanelSprite_) {
		menuPanelSprite_->SetPosition({ 0.0f, 0.0f });
		menuPanelSprite_->SetSize({
			static_cast<float>(WinApp::GetClientWidth()),
			static_cast<float>(WinApp::GetClientHeight())
		});
		menuPanelSprite_->SetColor({ 0.0f, 0.0f, 0.0f, titleFadeInAlpha_ });
		menuPanelSprite_->Update();
		menuPanelSprite_->Draw();
	}
}

void TitleScene::DrawAttractDemoStartPrompt(float screenWidth, float screenHeight) {
	TitleOverlayRenderer{}.DrawAttractDemoStartPrompt(*this, screenWidth, screenHeight);
}
