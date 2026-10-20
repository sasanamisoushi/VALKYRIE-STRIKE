#pragma once

#include "2D/Sprite.h"
#include "3D/Object3d.h"
#include "3D/Model.h"
#include "engine/Camera/Camera.h"
#include "Game/base/BaseScene.h"
#include "Game/Player/Player.h"
#include <array>
#include <chrono>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>

// 3Dワープ・トランジション（専用ローディングシーン）
// 非同期ロード時間を利用し、TPS視点でのワープトンネル超高速飛行演出を行う。
class LoadingScene : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	using Clock = std::chrono::steady_clock;

	// スピードライン構造体（進行方向奥から手前へ飛来する3D光条）
	struct SpeedLine {
		Vector3 position;
		float speed = 220.0f;
		float length = 3.5f;
		float width = 0.08f;
		float radius = 6.0f;
		float angle = 0.0f;
		float alpha = 0.8f;
		Vector4 color = { 0.4f, 0.85f, 1.0f, 0.8f };
	};

	// プラズマ粒子構造体（機体表面から発生するエネルギー摩擦粒子）
	struct PlasmaParticle {
		Vector3 position;
		Vector3 velocity;
		float life = 0.0f;
		float maxLife = 0.3f;
		float size = 0.15f;
		Vector4 color = { 0.3f, 0.7f, 1.0f, 0.9f };
	};

	void UpdateWarpFlight(float elapsedSeconds, float dt);
	void UpdateSpeedLines(float dt);
	void UpdatePlasmaParticles(float dt);

	// 3Dワープトンネル（外側ネビュラ星屑シリンダー＆内側スピードラインシリンダー）
	std::unique_ptr<Object3d> tunnelObject_;
	std::unique_ptr<Object3d> innerTunnelObject_;
	float uvScrollOffsetOuter_ = 0.0f;
	float uvScrollOffsetInner_ = 0.0f;
	float innerTunnelRotation_ = 0.0f;

	// 奥の中心光源（特異点・ワープゲートコア）
	std::unique_ptr<Object3d> warpCoreObject_;
	float warpCoreRotation_ = 0.0f;
	float warpCoreScale_ = 28.0f;

	// スピードラインとプラズマ用オブジェクト
	static constexpr size_t kSpeedLineCount = 72;
	std::array<SpeedLine, kSpeedLineCount> speedLines_{};
	std::array<std::unique_ptr<Object3d>, kSpeedLineCount> speedLineObjects_;

	static constexpr size_t kPlasmaCount = 48;
	std::array<PlasmaParticle, kPlasmaCount> plasmaParticles_{};
	std::array<std::unique_ptr<Object3d>, kPlasmaCount> plasmaObjects_;

	// カメラと自機
	std::unique_ptr<Camera> warpCamera_;
	std::unique_ptr<Player> warpPlayer_;

	// 画面全体のホワイトアウト制御（フェーズ1/2およびフェーズ4用）
	std::unique_ptr<Sprite> whiteoutSprite_;
	float whiteoutAlpha_ = 1.0f;

	// 演出進行状態
	enum class TransitionPhase {
		WhiteIn,        // フェーズ2: シーン開始直後、ホワイトアウトが晴れて自機が現れる
		WarpLoop,       // フェーズ3: ワープ演出をループ再生しつつ非同期ロード進行
		WhiteOutExit,   // フェーズ4: ロード完了後、進行方向の光が拡大し再びホワイトアウト
	};
	TransitionPhase phase_ = TransitionPhase::WhiteIn;

	int warpFrames_ = 0;
	static constexpr int kMinWarpFrames = 150;      // 最低ワープ演出時間（約2.5秒）
	static constexpr int kWhiteInFrames = 25;       // ホワイトイン完了までのフレーム数（約0.4秒）
	static constexpr int kExitWhiteOutFrames = 35;  // 終了ホワイトアウトまでのフレーム数（約0.6秒）
	int exitFrames_ = 0;

	// 非同期ロード関連
	std::unique_ptr<BaseScene> preparedScene_;
	std::atomic<bool> isAsyncLoadCompleted_{ false };
	std::thread asyncLoadThread_;

	Clock::time_point startTime_{};
	Clock::time_point lastFrameTime_{};
};
