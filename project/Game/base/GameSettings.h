#pragma once

#include <algorithm>
#include <fstream>
#include <string>

// タイトル・ゲームプレイ間で共有する、実行中のゲーム設定。
class GameSettings final {
public:
	static GameSettings& GetInstance() {
		static GameSettings instance;
		return instance;
	}

	float GetMasterVolume() const { return masterVolume_; }
	void SetMasterVolume(float volume) {
		masterVolume_ = std::clamp(volume, 0.0f, 1.0f);
		Save();
	}

	float GetMouseSensitivity() const { return mouseSensitivity_; }
	void SetMouseSensitivity(float sensitivity) {
		mouseSensitivity_ = std::clamp(sensitivity, 0.0005f, 0.0100f);
		Save();
	}

	bool IsControlGuideVisible() const { return isControlGuideVisible_; }
	void SetControlGuideVisible(bool visible) {
		isControlGuideVisible_ = visible;
		Save();
	}

	// ゲームプレイ中に流すBGM。既定では有効にしておく。
	bool IsGameplayBgmEnabled() const { return isGameplayBgmEnabled_; }
	void SetGameplayBgmEnabled(bool enabled) {
		isGameplayBgmEnabled_ = enabled;
		Save();
	}

	// タイトル・演出など、ゲームプレイ外で流すBGM。既定では無効。
	bool IsNonGameplayBgmEnabled() const { return isNonGameplayBgmEnabled_; }
	void SetNonGameplayBgmEnabled(bool enabled) {
		isNonGameplayBgmEnabled_ = enabled;
		Save();
	}

	// 効果音。既定では無効にして、必要なときだけ設定画面から有効化する。
	bool IsSoundEffectsEnabled() const { return isSoundEffectsEnabled_; }
	void SetSoundEffectsEnabled(bool enabled) {
		isSoundEffectsEnabled_ = enabled;
		Save();
	}

	// タイトルから本編へ入る際のフォールド画面遷移。変更時に即座に保存する。
	bool IsFoldTransitionEnabled() const { return isFoldTransitionEnabled_; }
	void SetFoldTransitionEnabled(bool enabled) {
		isFoldTransitionEnabled_ = enabled;
		Save();
	}

private:
	GameSettings() { Load(); }

	void Load() {
		std::ifstream input(kSettingsPath);
		if (!input.is_open()) {
			return;
		}
		std::string name;
		float value = 0.0f;
		while (input >> name >> value) {
			if (name == "master_volume") {
				masterVolume_ = std::clamp(value, 0.0f, 1.0f);
			} else if (name == "mouse_sensitivity") {
				mouseSensitivity_ = std::clamp(value, 0.0005f, 0.0100f);
			} else if (name == "control_guide_visible") {
				isControlGuideVisible_ = value != 0.0f;
			} else if (name == "gameplay_bgm_enabled") {
				isGameplayBgmEnabled_ = value != 0.0f;
			} else if (name == "non_gameplay_bgm_enabled") {
				isNonGameplayBgmEnabled_ = value != 0.0f;
			} else if (name == "sound_effects_enabled") {
				isSoundEffectsEnabled_ = value != 0.0f;
			} else if (name == "fold_transition_enabled") {
				isFoldTransitionEnabled_ = value != 0.0f;
			}
		}
	}

	void Save() const {
		std::ofstream output(kSettingsPath, std::ios::trunc);
		if (output.is_open()) {
			output << "master_volume " << masterVolume_ << '\n';
			output << "mouse_sensitivity " << mouseSensitivity_ << '\n';
			output << "control_guide_visible " << (isControlGuideVisible_ ? 1 : 0) << '\n';
			output << "gameplay_bgm_enabled " << (isGameplayBgmEnabled_ ? 1 : 0) << '\n';
			output << "non_gameplay_bgm_enabled " << (isNonGameplayBgmEnabled_ ? 1 : 0) << '\n';
			output << "sound_effects_enabled " << (isSoundEffectsEnabled_ ? 1 : 0) << '\n';
			output << "fold_transition_enabled " << (isFoldTransitionEnabled_ ? 1 : 0) << '\n';
		}
	}

	static constexpr const char* kSettingsPath = "resources/game_settings.cfg";
	float masterVolume_ = 1.0f;
	float mouseSensitivity_ = 0.0020f;
	bool isControlGuideVisible_ = true;
	bool isGameplayBgmEnabled_ = true;
	bool isNonGameplayBgmEnabled_ = false;
	bool isSoundEffectsEnabled_ = false;
	bool isFoldTransitionEnabled_ = true;
};
