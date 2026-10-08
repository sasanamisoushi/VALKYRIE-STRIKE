#pragma once
#include "engine/Camera/Camera.h"
#include "3D/Object3d.h"
#include "Game/base/BaseScene.h"
#include "Game/Scene/GamePlayScene.h"
#include "Game/Player/Player.h"
#include "engine/Audio/AudioManager.h"
#include "2D/Sprite.h"
#include <array>
#include <memory>

class TitleScene : public BaseScene {
public:
	void Initialize() override;

	void Finalize() override;

	void Update() override;

	void Draw() override;
private:
	enum class MenuItem {
		Start,
		Settings,
		Exit,
		Count
	};
	enum class SettingsItem {
		MasterVolume,
		MouseSensitivity,
		ControlGuide,
		Back,
		Count
	};

	void DrawMenuOverlay(float screenWidth, float screenHeight);
	void UpdateSettingsInput();
	void UpdateTitleFlight();
	void BeginLaunchSequence();
	void ToggleBackgroundMode();
	void UpdateGameplayBackground();
	void DrawBackgroundModeImGui();
	void DrawFoldTransition(float screenWidth, float screenHeight);
	std::unique_ptr<Camera> camera;

	std::unique_ptr<Sprite> titleSprite;
	std::unique_ptr<Sprite> menuPanelSprite_;
	std::unique_ptr<Sprite> menuRowSprite_;
	// メインメニュー時に右下へ置く、ゲーム内操作一覧。
	std::unique_ptr<Sprite> titleControlsGuideSprite_;
	std::array<std::unique_ptr<Sprite>, 8> menuLabelSprites_;
	std::array<std::unique_ptr<Sprite>, 40> settingMeterSegments_;
	std::array<std::array<std::unique_ptr<Sprite>, 5>, 2> settingValueDigitSprites_;
	// START GAME 後にフレームごとに生成する、中心へ吸い込まれるフォールド粒子・核・ビーム。
	std::array<std::unique_ptr<Sprite>, 240> foldStreakSprites_;
	std::unique_ptr<Sprite> foldVeilSprite_;
	std::unique_ptr<Sprite> foldCoreSprite_;
	std::unique_ptr<Sprite> foldBeamSprite_;
	std::unique_ptr<Sprite> foldFlashSprite_;

	std::unique_ptr<Player> titlePlayer_;
	std::unique_ptr<GamePlayScene> gameplayBackground_;
	SoundData launchSoundData_;
	IXAudio2SourceVoice* launchVoice_ = nullptr;

	enum class TitleFlightState {
		Cruising,
		Launching,
	};
	TitleFlightState flightState_ = TitleFlightState::Cruising;
	int titleFlightFrame_ = 0;
	int launchFrame_ = 0;
	float launchFadeAlpha_ = 0.0f;
	enum class BackgroundMode {
		FlightDemo,
		GameplayBackground,
	};
	// タイトルを開いた直後から本編を背景として動かす。
	BackgroundMode backgroundMode_ = BackgroundMode::GameplayBackground;
	MenuItem selectedMenuItem_ = MenuItem::Start;
	SettingsItem selectedSettingsItem_ = SettingsItem::MasterVolume;
	bool isSettingsOpen_ = false;

};



