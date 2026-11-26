#pragma once

#include "engine/Audio/AudioManager.h"

// ゲームプレイ中の BGM・歌・効果音ボイスの読み込み・設定反映・ポーズ／終了時制御を管理する。
class GameplayAudioController {
public:
	void Initialize(bool isTitleBackgroundMode, bool songIsActive);
	void Finalize();
	void ApplySettings(bool isTitleBackgroundMode, bool songIsActive);
	void SetSongPaused(bool songIsActive, bool paused);
	void StopBgmForGameOver();
	void MuteAllForShutdown();

	IXAudio2SourceVoice* GetBgmVoice() const { return bgmVoice_; }
	IXAudio2SourceVoice* GetSongVoice() const { return songVoice_; }

private:
	static void StopAndDestroyVoice(IXAudio2SourceVoice*& voice);

	SoundData alarmSoundData_{};
	SoundData bgmSoundData_{};
	SoundData songSoundData_{};
	IXAudio2SourceVoice* alarmVoice_ = nullptr;
	IXAudio2SourceVoice* bgmVoice_ = nullptr;
	IXAudio2SourceVoice* songVoice_ = nullptr;
};
