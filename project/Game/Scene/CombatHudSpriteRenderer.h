#pragma once

#include "2D/Sprite.h"

#include <array>
#include <list>
#include <memory>

class AmmoManager;
class Camera;
class Enemy;
class LockOnManager;
class Player;
class SongState;
class SpecialAttackState;

// 戦闘HUDの座標レイアウト（ImGuiオーバーレイ描画へ共有する）。
struct CombatHudLayout {
	float statusPanelX = 0.0f;
	float statusPanelY = 0.0f;
	float statusPanelWidth = 0.0f;
	float statusPanelHeight = 0.0f;
	float ammoPanelX = 0.0f;
	float normalAmmoPanelY = 0.0f;
	float homingAmmoPanelY = 0.0f;
	float normalAmmoTextY = 0.0f;
	float homingAmmoTextY = 0.0f;
	float gaugeX = 0.0f;
	float songGaugeY = 0.0f;
	float songGaugeWidth = 0.0f;
};

// 戦闘HUDスプライト描画に必要な読み取り専用データ。
struct CombatHudSpriteContext {
	float screenWidth = 0.0f;
	float screenHeight = 0.0f;
	int songDurationFrames = 900;
	const Player* player = nullptr;
	const std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	const AmmoManager* ammoManager = nullptr;
	const SongState* songState = nullptr;
	const SpecialAttackState* specialAttackState = nullptr;
};

// 戦闘中のスプライトHUD（ステータス枠・HP/SP/歌ゲージ・ボスHPバー・弾薬カード・
// デジタル残弾・リロードゲージ・ロックオン敵頭上HPバー）を管理・描画する。
class CombatHudSpriteRenderer {
public:
	void Initialize();

	CombatHudLayout DrawBaseHud(const CombatHudSpriteContext& context) const;

	void DrawLockedTargetOverheadHp(
		const Enemy* lockedEnemy,
		const LockOnManager* lockOnManager,
		const Camera* activeCamera,
		float screenWidth,
		float screenHeight) const;

private:
	static void DrawLabel(Sprite* label, const Vector2& position, const Vector2& size);

	template <size_t N>
	static void DrawDigitText(
		const std::string& text,
		const std::array<std::unique_ptr<Sprite>, N>& sprites,
		float rightX,
		float y,
		const Vector4& color);

	std::unique_ptr<Sprite> spGaugeBackgroundSprite_;
	std::unique_ptr<Sprite> spGaugeFillSprite_;
	std::unique_ptr<Sprite> spGaugeCostMarkerSprite_;
	std::unique_ptr<Sprite> songGaugeBackgroundSprite_;
	std::unique_ptr<Sprite> songGaugeFillSprite_;
	std::unique_ptr<Sprite> songGaugeStateSprite_;
	std::unique_ptr<Sprite> hudPanelSprite_;
	std::unique_ptr<Sprite> hudNormalAmmoPanelSprite_;
	std::unique_ptr<Sprite> hudHomingAmmoPanelSprite_;
	std::unique_ptr<Sprite> hpGaugeBackgroundSprite_;
	std::unique_ptr<Sprite> hpGaugeFillSprite_;
	std::unique_ptr<Sprite> bossHpGaugeBackgroundSprite_;
	std::unique_ptr<Sprite> bossHpGaugeFillSprite_;
	std::unique_ptr<Sprite> hudHpLabelSprite_;
	std::unique_ptr<Sprite> hudSpLabelSprite_;
	std::unique_ptr<Sprite> hudNormalAmmoIconSprite_;
	std::unique_ptr<Sprite> hudHomingAmmoIconSprite_;
	std::unique_ptr<Sprite> hudNormalReloadGaugeSprite_;
	std::unique_ptr<Sprite> hudHomingReloadGaugeSprite_;
	std::array<std::unique_ptr<Sprite>, 7> lockedTargetHpDigitSprites_;
	std::array<std::unique_ptr<Sprite>, 7> hudNormalAmmoDigitSprites_;
	std::array<std::unique_ptr<Sprite>, 7> hudHomingAmmoDigitSprites_;
};
