#include "TitleScene.h"
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
#include <shellapi.h>
#include <externals/imgui/imgui.h>

namespace {
	constexpr int kLaunchDurationFrames = 84;

	bool LaunchSimulationExecutable() {
		wchar_t modulePath[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (length == 0) {
			return false;
		}

		const std::filesystem::path currentExe(modulePath);
		const std::filesystem::path simulationExe = currentExe.parent_path() / L"ValkyrieStrikeSimulation.exe";
		const std::filesystem::path launchExe = std::filesystem::exists(simulationExe) ? simulationExe : currentExe;
		const wchar_t *parameters = (launchExe == currentExe) ? L"--simulation" : nullptr;
		const std::filesystem::path workDir = std::filesystem::current_path();

		HINSTANCE result = ShellExecuteW(
			nullptr,
			L"open",
			launchExe.c_str(),
			parameters,
			workDir.c_str(),
			SW_SHOWNORMAL);
		return reinterpret_cast<intptr_t>(result) > 32;
	}
}

void TitleScene::Initialize() {
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
	titleControlsGuideSprite_->Initialize(SpriteCommon::GetInstance(), "resources/title_controls.png");
	for (auto& label : menuLabelSprites_) {
		label = std::make_unique<Sprite>();
		label->Initialize(SpriteCommon::GetInstance(), "resources/title_menu_labels.png");
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
		// 透明縁を持つ発光円。各粒子を重ねて、太い紫の渦を作る。
		streak->Initialize(SpriteCommon::GetInstance(), "resources/circle2.png");
		streak->SetAnchorPoint({ 0.5f, 0.5f });
	}
	foldVeilSprite_ = std::make_unique<Sprite>();
	foldVeilSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	foldCoreSprite_ = std::make_unique<Sprite>();
	foldCoreSprite_->Initialize(SpriteCommon::GetInstance(), "resources/circle2.png");
	foldCoreSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	foldBeamSprite_ = std::make_unique<Sprite>();
	foldBeamSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	foldBeamSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	foldFlashSprite_ = std::make_unique<Sprite>();
	foldFlashSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");

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
			// フォールド開始後だけ背景を少し沈め、航法光を読み取りやすくする。
			launchFadeAlpha_ = GameSettings::GetInstance().IsFoldTransitionEnabled()
				? std::clamp((launchProgress - 0.24f) * 0.26f, 0.0f, 0.16f)
				: 0.0f;
			if (++launchFrame_ >= kLaunchDurationFrames) {
				GameStartTransition::Begin({ gameplayBackground_->GetTitleLaunchForward(), 0.78f, 54 });
				SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
			}
		} else {
			UpdateTitleFlight();
		}
		return;
	}

	Input* input = Input::GetInstance();
	if (isSettingsOpen_) {
		UpdateSettingsInput();
	} else {
		const int itemCount = static_cast<int>(MenuItem::Count);
		int selection = static_cast<int>(selectedMenuItem_);
		if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W)) {
			selection = (selection + itemCount - 1) % itemCount;
		} else if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S)) {
			selection = (selection + 1) % itemCount;
		}
		selectedMenuItem_ = static_cast<MenuItem>(selection);

		if (input->TriggerKey(DIK_SPACE)) {
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
		ImGui::TextDisabled("Saved automatically");
	}
	ImGui::End();
#endif
}

void TitleScene::BeginLaunchSequence() {
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
	// 手持ち素材は出撃警報として使い、ブーストの視覚演出と同期させる。
	launchVoice_ = AudioManager::GetInstance()->PlayWave(launchSoundData_);
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
			GameStartTransition::Begin({ { 0.0f, 0.0f, 1.0f }, 0.78f, 54 });
			SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
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
	if (input->TriggerKey(DIK_ESCAPE)) {
		isSettingsOpen_ = false;
		return;
	}

	const int itemCount = static_cast<int>(SettingsItem::Count);
	int selection = static_cast<int>(selectedSettingsItem_);
	if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W)) {
		selection = (selection + itemCount - 1) % itemCount;
	} else if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S)) {
		selection = (selection + 1) % itemCount;
	}
	selectedSettingsItem_ = static_cast<SettingsItem>(selection);

	const bool decrease = input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_A);
	const bool increase = input->TriggerKey(DIK_RIGHT) || input->TriggerKey(DIK_D);
	GameSettings& settings = GameSettings::GetInstance();
	if (selectedSettingsItem_ == SettingsItem::MasterVolume && (decrease || increase)) {
		settings.SetMasterVolume(settings.GetMasterVolume() + (increase ? 0.05f : -0.05f));
		AudioManager::GetInstance()->SetMasterVolume(settings.GetMasterVolume());
	} else if (selectedSettingsItem_ == SettingsItem::MouseSensitivity && (decrease || increase)) {
		settings.SetMouseSensitivity(settings.GetMouseSensitivity() + (increase ? 0.0005f : -0.0005f));
	} else if (selectedSettingsItem_ == SettingsItem::ControlGuide && (decrease || increase ||
		input->TriggerKey(DIK_SPACE))) {
		settings.SetControlGuideVisible(!settings.IsControlGuideVisible());
	}

	if (input->TriggerKey(DIK_SPACE) && selectedSettingsItem_ == SettingsItem::Back) {
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
	// 背景の飛行演出を見せるため、待機中は暗幕を半透明にする。
	if (menuPanelSprite_) {
		menuPanelSprite_->SetPosition({ 0.0f, 0.0f });
		menuPanelSprite_->SetSize({
			static_cast<float>(WinApp::GetClientWidth()),
			static_cast<float>(WinApp::GetClientHeight())
		});
		const float panelAlpha = flightState_ == TitleFlightState::Launching
			? launchFadeAlpha_
			: (isSettingsOpen_ ? 0.88f : 0.48f);
		menuPanelSprite_->SetColor({ 0.0f, 0.0f, 0.0f, panelAlpha });
		menuPanelSprite_->Update();
		menuPanelSprite_->Draw();
	}
	if (titleSprite && !isSettingsOpen_ && flightState_ == TitleFlightState::Cruising) {
		titleSprite->Draw();
	}
	if (flightState_ == TitleFlightState::Launching && GameSettings::GetInstance().IsFoldTransitionEnabled()) {
		DrawFoldTransition(
			static_cast<float>(WinApp::GetClientWidth()),
			static_cast<float>(WinApp::GetClientHeight()));
	}

	if (flightState_ == TitleFlightState::Cruising) {
		DrawMenuOverlay(static_cast<float>(WinApp::GetClientWidth()), static_cast<float>(WinApp::GetClientHeight()));
		// タイトルメニュー専用の選択・決定操作を、選択肢と重ならない右下へ表示する。
		if (!isSettingsOpen_ && titleControlsGuideSprite_) {
			constexpr float kGuideWidth = 360.0f;
			constexpr float kGuideHeight = 206.0f;
			constexpr float kGuideMargin = 18.0f;
			titleControlsGuideSprite_->SetPosition({
				(static_cast<float>(WinApp::GetClientWidth()) - kGuideWidth - kGuideMargin),
				(static_cast<float>(WinApp::GetClientHeight()) - kGuideHeight - kGuideMargin),
			});
			titleControlsGuideSprite_->SetSize({ kGuideWidth, kGuideHeight });
			titleControlsGuideSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.96f });
			titleControlsGuideSprite_->Update();
			titleControlsGuideSprite_->Draw();
		}
	}
}

void TitleScene::DrawFoldTransition(float screenWidth, float screenHeight) {
	if (!foldVeilSprite_ || !foldCoreSprite_ || !foldBeamSprite_ || !foldFlashSprite_) {
		return;
	}

	const float progress = std::clamp(
		static_cast<float>(launchFrame_) / static_cast<float>(kLaunchDurationFrames), 0.0f, 1.0f);
	// 0.22 から航法光が集まり、0.75 付近で跳躍光へ変化する。
	const float streakProgress = std::clamp((progress - 0.22f) / 0.54f, 0.0f, 1.0f);
	const float streakAlpha = std::clamp(streakProgress * 2.8f, 0.0f, 1.0f) *
		std::clamp((0.86f - progress) * 7.0f, 0.0f, 1.0f);
	const float flashAlpha = std::clamp((progress - 0.73f) / 0.22f, 0.0f, 1.0f);
	const float centerX = screenWidth * 0.50f;
	const float centerY = screenHeight * 0.47f;
	const float maxRadius = (std::min)(screenWidth, screenHeight) * 0.78f;

	// 静止画は使わず、薄い水色の空間色と多数の発光球を毎フレーム計算して重ねる。
	foldVeilSprite_->SetPosition({ 0.0f, 0.0f });
	foldVeilSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	foldVeilSprite_->SetSize({ screenWidth, screenHeight });
	foldVeilSprite_->SetColor({ 0.02f, 0.34f, 0.58f, streakAlpha * 0.30f });
	foldVeilSprite_->Update();
	foldVeilSprite_->Draw();

	for (size_t i = 0; i < foldStreakSprites_.size(); ++i) {
		Sprite* particle = foldStreakSprites_[i].get();
		if (!particle) {
			continue;
		}
		constexpr size_t kParticlesPerSpiral = 10;
		constexpr float kSpiralCount = 24.0f;
		const float spiralIndex = static_cast<float>(i / kParticlesPerSpiral);
		const float particleIndex = static_cast<float>(i % kParticlesPerSpiral);
		const float radialSeed = (particleIndex + 0.35f) / static_cast<float>(kParticlesPerSpiral);
		// 全粒子を外側から核へ寄せつつ、螺旋の位相を回す。遠い粒子ほど大きくして隙間を埋める。
		const float inwardFactor = 1.0f - streakProgress * 0.84f;
		const float radius = 16.0f + maxRadius * radialSeed * inwardFactor;
		const float wavePhase = spiralIndex * 1.19f + radialSeed * 5.8f + progress * 10.5f;
		const float angle = spiralIndex * (6.28318531f / kSpiralCount) + progress * 7.0f +
			std::sin(wavePhase) * (0.20f + radialSeed * 0.22f);
		const float curl = std::cos(wavePhase * 1.31f) * maxRadius * 0.052f * radialSeed;
		const float x = centerX + std::cos(angle) * radius - std::sin(angle) * curl;
		const float y = centerY + std::sin(angle) * radius + std::cos(angle) * curl;
		const float diameter = 38.0f + radialSeed * 82.0f + std::sin(wavePhase * 1.9f) * 9.0f;
		const float cyanVariation = 0.28f + std::fmod(spiralIndex * 0.11f, 0.24f);
		particle->SetPosition({ x, y });
		particle->SetSize({ diameter, diameter });
		particle->SetRotation(0.0f);
		particle->SetColor({ cyanVariation, 0.72f + radialSeed * 0.24f, 1.0f,
			streakAlpha * (0.22f + radialSeed * 0.22f) });
		particle->Update();
		particle->Draw();
	}

	// 核と水平ビームを最後に重ね、太い光のうねりが一点へ収束しているように見せる。
	const float coreSize = 54.0f + streakProgress * (screenHeight * 0.17f);
	foldCoreSprite_->SetPosition({ centerX, centerY });
	foldCoreSprite_->SetSize({ coreSize, coreSize });
	foldCoreSprite_->SetRotation(progress * 2.2f);
	foldCoreSprite_->SetColor({ 0.76f, 1.0f, 1.0f, streakAlpha * 0.76f });
	foldCoreSprite_->Update();
	foldCoreSprite_->Draw();

	foldBeamSprite_->SetPosition({ centerX, centerY });
	foldBeamSprite_->SetSize({ screenWidth * (0.08f + streakProgress * 1.04f), 2.0f + streakProgress * 11.0f });
	foldBeamSprite_->SetRotation(std::sin(progress * 7.0f) * 0.025f);
	foldBeamSprite_->SetColor({ 0.58f, 0.96f, 1.0f, streakAlpha * 0.74f });
	foldBeamSprite_->Update();
	foldBeamSprite_->Draw();

	// 中心から外へ広がる水色の跳躍光。最後は白へ寄せて場面切替を隠す。
	foldFlashSprite_->SetPosition({ 0.0f, 0.0f });
	foldFlashSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	foldFlashSprite_->SetSize({ screenWidth, screenHeight });
	foldFlashSprite_->SetColor({
		0.46f + flashAlpha * 0.54f,
		0.86f + flashAlpha * 0.14f,
		1.00f,
		flashAlpha * 0.88f,
	});
	foldFlashSprite_->Update();
	foldFlashSprite_->Draw();
}

void TitleScene::DrawMenuOverlay(float screenWidth, float screenHeight) {
	if (!menuPanelSprite_ || !menuRowSprite_) {
		return;
	}

	struct LabelRegion {
		float x;
		float y;
		float width;
		float height;
	};
	// 1214 x 1295 の title_menu_labels.png 内にある各ラベルの領域。
	constexpr std::array<LabelRegion, 8> kLabelRegions = {{
		{ 75.0f, 45.0f, 1065.0f, 140.0f }, // START GAME
		{ 170.0f, 195.0f, 880.0f, 130.0f }, // SETTINGS
		{ 145.0f, 345.0f, 930.0f, 140.0f }, // EXIT GAME
		{ 35.0f, 500.0f, 1145.0f, 140.0f }, // MASTER VOLUME
		{ 10.0f, 650.0f, 1195.0f, 140.0f }, // MOUSE SENSITIVITY
		{ 75.0f, 805.0f, 1060.0f, 140.0f }, // CONTROL GUIDE
		{ 330.0f, 960.0f, 555.0f, 135.0f }, // BACK
		{ 245.0f, 1100.0f, 725.0f, 145.0f }, // ON / OFF
	}};
	const auto drawLabel = [&](size_t labelIndex, float centerX, float topY, float targetHeight, float alpha = 1.0f) {
		if (labelIndex >= menuLabelSprites_.size() || !menuLabelSprites_[labelIndex]) {
			return;
		}
		const LabelRegion& region = kLabelRegions[labelIndex];
		const float targetWidth = targetHeight * region.width / region.height;
		Sprite* label = menuLabelSprites_[labelIndex].get();
		label->SetTextureLeftTop({ region.x, region.y });
		label->SetTextureSize({ region.width, region.height });
		label->SetPosition({ centerX - targetWidth * 0.5f, topY });
		label->SetSize({ targetWidth, targetHeight });
		label->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
		label->Update();
		label->Draw();
	};
	if (!isSettingsOpen_) {
		// メインメニューはロゴの下に文字だけを置く。選択枠や背景パネルは描かない。
		const auto drawMainMenuLabel = [&](size_t labelIndex, MenuItem item, float topY) {
			const bool selected = selectedMenuItem_ == item;
			const float labelHeight = selected ? 52.0f : 44.0f;
			drawLabel(labelIndex, screenWidth * 0.5f,
				topY - (labelHeight - 44.0f) * 0.5f, labelHeight,
				selected ? 1.0f : 0.62f);
		};
		drawMainMenuLabel(0, MenuItem::Start, screenHeight - 225.0f);
		drawMainMenuLabel(1, MenuItem::Settings, screenHeight - 145.0f);
		drawMainMenuLabel(2, MenuItem::Exit, screenHeight - 75.0f);
		return;
	}

	const GameSettings& settings = GameSettings::GetInstance();
	// 設定は黒背景へ直接表示する。選択枠・パネルは使わず、文字の強調で選択状態を示す。
	drawLabel(1, screenWidth * 0.5f, screenHeight * 0.10f, 60.0f);
	const float rowStartY = screenHeight * 0.27f;
	const float rowSpacing = 82.0f;
	const float labelCenterX = screenWidth * 0.32f;
	const float sliderX = screenWidth * 0.56f;
	const float sliderWidth = screenWidth * 0.30f;

	const auto drawSettingLabel = [&](int index, size_t labelIndex, float topY) {
		const bool selected = index == static_cast<int>(selectedSettingsItem_);
		const float labelHeight = selected ? 54.0f : 46.0f;
		drawLabel(labelIndex, labelCenterX, topY - (labelHeight - 46.0f) * 0.5f,
			labelHeight, selected ? 1.0f : 0.58f);
	};
	const auto drawLevelMeter = [&](int meterIndex, float topY, float valueRatio,
		int currentStep, int maxStep, bool selected) {
		constexpr int kSegmentCount = 20;
		const float ratio = std::clamp(valueRatio, 0.0f, 1.0f);
		const int litSegmentCount = static_cast<int>(ratio * static_cast<float>(kSegmentCount) + 0.5f);
		const float segmentGap = 4.0f;
		const float segmentWidth = (sliderWidth - segmentGap * static_cast<float>(kSegmentCount - 1)) /
			static_cast<float>(kSegmentCount);
		const float segmentY = topY + 17.0f;
		for (int index = 0; index < kSegmentCount; ++index) {
			Sprite* segment = settingMeterSegments_[meterIndex * kSegmentCount + index].get();
			segment->SetPosition({ sliderX + static_cast<float>(index) * (segmentWidth + segmentGap), segmentY });
			segment->SetSize({ segmentWidth, 20.0f });
			const bool isLit = index < litSegmentCount;
			const bool isCurrent = isLit && index == litSegmentCount - 1;
			segment->SetColor(isCurrent
				? Vector4{ 0.92f, 0.98f, 1.0f, 1.0f }
				: (isLit
					? (selected ? Vector4{ 1.0f, 0.46f, 0.08f, 1.0f } : Vector4{ 0.58f, 0.27f, 0.07f, 0.92f })
					: Vector4{ 0.26f, 0.29f, 0.32f, 1.0f }));
			segment->Update();
			segment->Draw();
		}

		const std::string levelText = std::to_string(currentStep) + "/" + std::to_string(maxStep);
		const float digitWidth = 20.0f;
		const float digitHeight = 26.0f;
		const float digitStartX = sliderX + sliderWidth + 18.0f;
		for (size_t index = 0; index < levelText.size() && index < settingValueDigitSprites_[meterIndex].size(); ++index) {
			Sprite* digit = settingValueDigitSprites_[meterIndex][index].get();
			const int glyphIndex = levelText[index] == '/' ? 10 : levelText[index] - '0';
			digit->SetTextureLeftTop({ static_cast<float>(glyphIndex * 64), 0.0f });
			digit->SetTextureSize({ 64.0f, 80.0f });
			digit->SetPosition({ digitStartX + digitWidth * static_cast<float>(index), topY + 13.0f });
			digit->SetSize({ digitWidth, digitHeight });
			digit->SetColor(selected ? Vector4{ 1.0f, 0.82f, 0.24f, 1.0f } : Vector4{ 0.62f, 0.66f, 0.70f, 0.86f });
			digit->Update();
			digit->Draw();
		}
	};

	const int volumeStep = static_cast<int>(settings.GetMasterVolume() * 20.0f + 0.5f);
	const float sensitivityRatio = (settings.GetMouseSensitivity() - 0.0005f) / 0.0095f;
	const int sensitivityStep = static_cast<int>(sensitivityRatio * 19.0f + 0.5f);
	drawSettingLabel(0, 3, rowStartY);
	drawLevelMeter(0, rowStartY, settings.GetMasterVolume(), volumeStep, 20,
		selectedSettingsItem_ == SettingsItem::MasterVolume);
	drawSettingLabel(1, 4, rowStartY + rowSpacing);
	drawLevelMeter(1, rowStartY + rowSpacing, sensitivityRatio, sensitivityStep, 19,
		selectedSettingsItem_ == SettingsItem::MouseSensitivity);
	drawSettingLabel(2, 5, rowStartY + rowSpacing * 2.0f);
	const bool guideSelected = selectedSettingsItem_ == SettingsItem::ControlGuide;
	drawLabel(7, screenWidth * 0.75f, rowStartY + rowSpacing * 2.0f + 3.0f,
		guideSelected ? 42.0f : 36.0f,
		guideSelected ? 1.0f : (settings.IsControlGuideVisible() ? 0.74f : 0.40f));
	const bool backSelected = selectedSettingsItem_ == SettingsItem::Back;
	drawLabel(6, screenWidth * 0.5f, rowStartY + rowSpacing * 3.0f,
		backSelected ? 54.0f : 46.0f, backSelected ? 1.0f : 0.58f);
}
