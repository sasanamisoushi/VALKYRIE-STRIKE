#pragma once
#include "engine/Particle/ParticleManager.h"
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include <array>
#include <cstddef>
#include <memory>
#include <vector>

class ExplosionManager {
public:
	// 撃破対象ごとに設定を分けられる。通常敵の設定を共通値として初期値にする。
	enum class EffectTarget : int {
		StandardEnemy = 0,
		GroundEnemy,
		JammerEnemy,
		Boss,
		Player,
		Count,
	};

	// 撃破時に使用するパーティクルのプリセット。ImGui の選択値としても使う。
	enum class DestructionParticleType : int {
		FlameToSmoke = 0,
		Fireball,
		SmokeCloud,
		Sparks,
	};

    struct ExplosionConfig {
        int count = 100;
        float color[4] = { 1.0f, 0.5f, 0.1f, 1.0f };
        float speed = 0.5f;
        float speedVariance = 0.2f;
        float scale = 0.1f;
        float scaleVariance = 0.05f;
        float lifeTimeMin = 0.5f;
        float lifeTimeMax = 1.0f;
        float posVariance = 0.1f;
		int destructionParticleType = static_cast<int>(DestructionParticleType::FlameToSmoke);
		float smokeColor[4] = { 0.10f, 0.10f, 0.12f, 0.60f };
		float flameToSmokeScale = 2.8f;
		float flameToSmokeTransition = 0.35f;
    };

    // 初期化（ParticleManagerのポインタを受け取っておく）
    void Initialize(ParticleManager *particleManager);

    // 毎フレームの更新（寿命のカウントダウンと削除）
    void Update();

    // RingとCylinderを描画
    void Draw();

    // 報告された座標リストを受け取り、爆発を一気に発生させる
    void CreateExplosions(const std::vector<Vector3> &hitPositions,
		EffectTarget target = EffectTarget::StandardEnemy);
    void CreateHitEffects(const std::vector<Vector3> &hitPositions);
	// 誘導弾が敵を撃破しきれなかった時の、小さな高熱収束ヒット演出。
	void CreateHomingHitEffects(const std::vector<Vector3>& hitPositions);
    void CreateDestructionEffects(const std::vector<Vector3> &hitPositions,
		EffectTarget target = EffectTarget::StandardEnemy);
	// 爆炎を出さず、リングだけを中心へ収束させる消滅演出。
	void CreateImplosionEffects(const std::vector<Vector3>& hitPositions);
	// エディタ専用。粒子は同じ設定で再生し、3D衝撃波メッシュだけを任意に隠せる。
	void CreateDestructionPreview(const std::vector<Vector3>& hitPositions, EffectTarget target,
		bool includeShockwaveMeshes);
	// エフェクトエディタの再生停止／状態表示に用いる。
	void ClearEffects();
	std::size_t GetActiveEffectCount() const;

    // 設定の取得
    ExplosionConfig& GetConfig() { return commonConfig_; }
    ExplosionConfig& GetConfig(EffectTarget target);
    const ExplosionConfig& GetConfig(EffectTarget target) const;
    bool UsesCommonConfig(EffectTarget target) const;
    void SetUseCommonConfig(EffectTarget target, bool useCommon);

	// 敵の実型から、適用する撃破設定を求める。
	static EffectTarget ClassifyEnemy(const class Enemy& enemy);

    // JSON保存/読込
    void SaveToJson(const std::string &filepath);
    void LoadFromJson(const std::string &filepath);

private:
    static constexpr std::size_t kMaxExplosions = 16;

    // Plane粒子と同時に表示する立体エフェクト
    struct Explosion {
        std::array<std::unique_ptr<Object3d>, 3> rings;
        std::unique_ptr<Object3d> cylinder;
		std::unique_ptr<Object3d> core;
        Vector3 position{};
        float age = 0.0f;
        float duration = 0.7f;
        bool active = false;
		bool isImplosion = false;
		bool isHomingHit = false;
		int activeRingCount = 3;
    };

    void InitializeExplosionObjects(Explosion &explosion);
    Explosion *AcquireExplosion();
	void CreateEffects(const std::vector<Vector3> &hitPositions, bool useDestructionParticle,
		bool includeMeshes,
		const ExplosionConfig& config);

    std::array<Explosion, kMaxExplosions> explosions_;
    ParticleManager *particleManager_ = nullptr; // エミッター生成時に必要な大元
    ExplosionConfig commonConfig_;
    std::array<ExplosionConfig, static_cast<size_t>(EffectTarget::Count)> targetConfigs_{};
    std::array<bool, static_cast<size_t>(EffectTarget::Count)> useCommonConfig_{};
};

