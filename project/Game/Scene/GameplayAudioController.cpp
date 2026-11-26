#include "GameplayAudioController.h"

#include "Game/base/GameSettings.h"

void GameplayAudioController::StopAndDestroyVoice(IXAudio2SourceVoice*& voice) {
	if (voice) {
		voice->Stop();
		voice->DestroyVoice();
		voice = nullptr;
	}
}

void GameplayAudioController::Initialize(bool isTitleBackgroundMode, bool songIsActive) {
	alarmSoundData_ = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");
	bgmSoundData_ = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");
	songSoundData_ = AudioManager::GetInstance()->LoadAudio("resources/song_bgm.mp3");

	// タイトル背景では無音。本編では保存済み設定に応じて再生する。
	AudioManager::GetInstance()->SetMasterVolume(GameSettings::GetInstance().GetMasterVolume());
	ApplySettings(isTitleBackgroundMode, songIsActive);
}

void GameplayAudioController::Finalize() {
	StopAndDestroyVoice(alarmVoice_);
	StopAndDestroyVoice(bgmVoice_);
	StopAndDestroyVoice(songVoice_);

	AudioManager::GetInstance()->UnloadWave(alarmSoundData_);
	AudioManager::GetInstance()->UnloadWave(bgmSoundData_);
	AudioManager::GetInstance()->UnloadWave(songSoundData_);
}

void GameplayAudioController::ApplySettings(bool isTitleBackgroundMode, bool songIsActive) {
	// タイトル背景は常に無音。本編・デモだけが音声設定の対象になる。
	if (isTitleBackgroundMode) {
		StopAndDestroyVoice(alarmVoice_);
		StopAndDestroyVoice(bgmVoice_);
		StopAndDestroyVoice(songVoice_);
		return;
	}

	const GameSettings& settings = GameSettings::GetInstance();
	if (settings.IsSoundEffectsEnabled()) {
		if (!alarmVoice_) {
			alarmVoice_ = AudioManager::GetInstance()->PlayWave(alarmSoundData_, true);
		}
	} else {
		StopAndDestroyVoice(alarmVoice_);
	}

	if (!settings.IsGameplayBgmEnabled()) {
		StopAndDestroyVoice(bgmVoice_);
		StopAndDestroyVoice(songVoice_);
		return;
	}

	if (!bgmVoice_) {
		bgmVoice_ = AudioManager::GetInstance()->PlayWave(bgmSoundData_, true);
	}
	if (!songVoice_) {
		songVoice_ = AudioManager::GetInstance()->PlayWave(songSoundData_, true);
	}

	if (bgmVoice_) {
		bgmVoice_->SetVolume(songIsActive ? 0.0f : 1.0f);
	}
	if (songVoice_) {
		songVoice_->SetVolume(songIsActive ? 1.0f : 0.0f);
	}
}

void GameplayAudioController::SetSongPaused(bool songIsActive, bool paused) {
	if (!songIsActive || !songVoice_) {
		return;
	}
	if (paused) {
		songVoice_->Stop();
	} else {
		songVoice_->Start();
	}
}

void GameplayAudioController::StopBgmForGameOver() {
	if (bgmVoice_) {
		bgmVoice_->Stop();
	}
}

void GameplayAudioController::MuteAllForShutdown() {
	if (alarmVoice_) {
		alarmVoice_->SetVolume(0.0f);
	}
	if (bgmVoice_) {
		bgmVoice_->SetVolume(0.0f);
	}
	if (songVoice_) {
		songVoice_->SetVolume(0.0f);
	}
}
