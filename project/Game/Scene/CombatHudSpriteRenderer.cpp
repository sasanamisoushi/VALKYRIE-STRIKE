#include "CombatHudSpriteRenderer.h"

#include "AmmoManager.h"
#include "LockOnManager.h"
#include "SongState.h"
#include "SpecialAttackState.h"
#include "2D/SpriteCommon.h"
#include "Game/Player/Player.h"
#include "Game/enemy/Boss.h"
#include "Game/enemy/Enemy.h"
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/Resource/TextureManager.h"
#include "engine/base/WinApp.h"
#include "engine/math/MyMath.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
bool GetHudOverlayBounds(float& minX, float& minY, float& maxX, float& maxY) {
	if (FlyCamera::GetGameViewBounds(minX, minY, maxX, maxY) && maxX > minX && maxY > minY) {
		return true;
	}
	minX = 0.0f;
	minY = 0.0f;
	maxX = static_cast<float>(WinApp::GetClientWidth());
	maxY = static_cast<float>(WinApp::GetClientHeight());
	return maxX > minX && maxY > minY;
}
} // namespace

void CombatHudSpriteRenderer::DrawLabel(Sprite* label, const Vector2& position, const Vector2& size) {
	if (!label) return;
	label->SetPosition(position);
	label->SetSize(size);
	label->Update();
	label->Draw();
}

template <size_t N>
void CombatHudSpriteRenderer::DrawDigitText(
	const std::string& text,
	const std::array<std::unique_ptr<Sprite>, N>& sprites,
	float rightX,
	float y,
	const Vector4& color) {
	constexpr float digitWidth = 22.0f;
	constexpr float digitHeight = 30.0f;
	const float startX = rightX - digitWidth * static_cast<float>(text.size());
	for (size_t index = 0; index < text.size() && index < sprites.size(); ++index) {
		Sprite* digitSprite = sprites[index].get();
		if (!digitSprite) continue;
		const int glyphIndex = (text[index] == '/') ? 10 : text[index] - '0';
		digitSprite->SetTextureLeftTop({ static_cast<float>(glyphIndex * 64), 0.0f });
		digitSprite->SetTextureSize({ 64.0f, 80.0f });
		digitSprite->SetPosition({ startX + digitWidth * static_cast<float>(index), y });
		digitSprite->SetSize({ digitWidth, digitHeight });
		digitSprite->SetColor(color);
		digitSprite->Update();
		digitSprite->Draw();
	}
}

void CombatHudSpriteRenderer::Initialize() {
	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();
	const char* hudTexturePaths[] = {
		"resources/hud_panel_frame.png",
		"resources/hud_label_hp.png",
		"resources/hud_label_ammo.png",
		"resources/hud_label_sp.png",
		"resources/hud_digits.png",
		"resources/hud_ammo_icons.png"
	};
	for (const char* path : hudTexturePaths) {
		TextureManager::GetInstance()->LoadTexture(path);
	}

	spGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	spGaugeBackgroundSprite_->Initialize(spriteCommon, "resources/white1x1.png");
	spGaugeFillSprite_ = std::make_unique<Sprite>();
	spGaugeFillSprite_->Initialize(spriteCommon, "resources/white1x1.png");
	spGaugeCostMarkerSprite_ = std::make_unique<Sprite>();
	spGaugeCostMarkerSprite_->Initialize(spriteCommon, "resources/white1x1.png");
	songGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	songGaugeBackgroundSprite_->Initialize(spriteCommon, "resources/white1x1.png");
	songGaugeFillSprite_ = std::make_unique<Sprite>();
	songGaugeFillSprite_->Initialize(spriteCommon, "resources/white1x1.png");
	songGaugeStateSprite_ = std::make_unique<Sprite>();
	songGaugeStateSprite_->Initialize(spriteCommon, "resources/white1x1.png");

	hudPanelSprite_ = std::make_unique<Sprite>();
	hudPanelSprite_->Initialize(spriteCommon, hudTexturePaths[0]);
	hudNormalAmmoPanelSprite_ = std::make_unique<Sprite>();
	hudNormalAmmoPanelSprite_->Initialize(spriteCommon, hudTexturePaths[0]);
	hudHomingAmmoPanelSprite_ = std::make_unique<Sprite>();
	hudHomingAmmoPanelSprite_->Initialize(spriteCommon, hudTexturePaths[0]);
	hpGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	hpGaugeBackgroundSprite_->Initialize(spriteCommon, "resources/white1x1.png");
	hpGaugeFillSprite_ = std::make_unique<Sprite>();
	hpGaugeFillSprite_->Initialize(spriteCommon, "resources/white1x1.png");
	bossHpGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	bossHpGaugeBackgroundSprite_->Initialize(spriteCommon, "resources/white1x1.png");
	bossHpGaugeFillSprite_ = std::make_unique<Sprite>();
	bossHpGaugeFillSprite_->Initialize(spriteCommon, "resources/white1x1.png");

	hudHpLabelSprite_ = std::make_unique<Sprite>();
	hudHpLabelSprite_->Initialize(spriteCommon, hudTexturePaths[1]);
	hudSpLabelSprite_ = std::make_unique<Sprite>();
	hudSpLabelSprite_->Initialize(spriteCommon, hudTexturePaths[3]);
	hudNormalAmmoIconSprite_ = std::make_unique<Sprite>();
	hudNormalAmmoIconSprite_->Initialize(spriteCommon, hudTexturePaths[5]);
	hudHomingAmmoIconSprite_ = std::make_unique<Sprite>();
	hudHomingAmmoIconSprite_->Initialize(spriteCommon, hudTexturePaths[5]);
	hudNormalReloadGaugeSprite_ = std::make_unique<Sprite>();
	hudNormalReloadGaugeSprite_->Initialize(spriteCommon, "resources/white1x1.png");
	hudHomingReloadGaugeSprite_ = std::make_unique<Sprite>();
	hudHomingReloadGaugeSprite_->Initialize(spriteCommon, "resources/white1x1.png");

	for (auto& digitSprite : lockedTargetHpDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(spriteCommon, hudTexturePaths[4]);
	}
	for (auto& digitSprite : hudNormalAmmoDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(spriteCommon, hudTexturePaths[4]);
	}
	for (auto& digitSprite : hudHomingAmmoDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(spriteCommon, hudTexturePaths[4]);
	}
}

CombatHudLayout CombatHudSpriteRenderer::DrawBaseHud(const CombatHudSpriteContext& context) const {
	const float screenWidth = context.screenWidth;
	const float screenHeight = context.screenHeight;
	const float statusPanelX = 22.0f;
	// 左下は機体の生存・必殺技・歌をまとめた戦闘ステータスパネルにする。
	const float statusPanelY = screenHeight - 200.0f;
	constexpr float kStatusPanelWidth = 420.0f;
	constexpr float kStatusPanelHeight = 180.0f;
	// 通常弾・誘導弾をそれぞれ独立した大型カードにする。
	const float ammoPanelX = screenWidth - 440.0f;
	const float normalAmmoPanelY = screenHeight - 350.0f;
	const float homingAmmoPanelY = normalAmmoPanelY + 165.0f;
	const float normalAmmoTextY = normalAmmoPanelY + 73.0f;
	const float homingAmmoTextY = homingAmmoPanelY + 73.0f;
	const float statusGaugeX = statusPanelX + 100.0f;
	const float statusGaugeWidth = 282.0f;
	const float statusGaugeHeight = 20.0f;
	const float gaugeX = statusGaugeX;
	const float gaugeY = statusPanelY + 80.0f;
	const float gaugeWidth = statusGaugeWidth;
	const float gaugeHeight = 18.0f;
	const float songGaugeY = statusPanelY + 130.0f;
	const float songGaugeWidth = gaugeWidth - 40.0f;

	const CombatHudLayout layout{
		statusPanelX,
		statusPanelY,
		kStatusPanelWidth,
		kStatusPanelHeight,
		ammoPanelX,
		normalAmmoPanelY,
		homingAmmoPanelY,
		normalAmmoTextY,
		homingAmmoTextY,
		gaugeX,
		songGaugeY,
		songGaugeWidth,
	};

	const bool isSongActive = context.songState && context.songState->IsActive();
	const int songFrame = context.songState ? context.songState->GetFrame() : 0;
	const float songGauge = context.songState ? context.songState->GetGauge() : 0.0f;
	const float songHudPulse = isSongActive
		? 0.84f + 0.16f * (0.5f + 0.5f * std::sin(static_cast<float>(songFrame) * 0.19f))
		: 1.0f;

	if (hudPanelSprite_) {
		hudPanelSprite_->SetPosition({ statusPanelX, statusPanelY });
		hudPanelSprite_->SetSize({ kStatusPanelWidth, kStatusPanelHeight });
		hudPanelSprite_->SetColor(isSongActive
			? Vector4{ 1.0f, 0.72f + songHudPulse * 0.20f, 1.0f, 0.96f }
			: Vector4{ 1.0f, 1.0f, 1.0f, 0.92f });
		hudPanelSprite_->Update();
		hudPanelSprite_->Draw();
	}
	if (hudNormalAmmoPanelSprite_ && hudHomingAmmoPanelSprite_) {
		auto drawAmmoPanelBackground = [&](Sprite* panelSprite, float panelY) {
			panelSprite->SetPosition({ ammoPanelX, panelY });
			panelSprite->SetSize({ 420.0f, 150.0f });
			panelSprite->SetColor(isSongActive
				? Vector4{ 1.0f, 0.78f + songHudPulse * 0.14f, 0.92f, 0.96f }
				: Vector4{ 1.0f, 1.0f, 1.0f, 0.92f });
			panelSprite->Update();
			panelSprite->Draw();
		};
		drawAmmoPanelBackground(hudNormalAmmoPanelSprite_.get(), normalAmmoPanelY);
		drawAmmoPanelBackground(hudHomingAmmoPanelSprite_.get(), homingAmmoPanelY);
	}

	DrawLabel(hudHpLabelSprite_.get(), { statusPanelX + 27.0f, statusPanelY + 27.0f }, { 48.0f, 28.0f });
	DrawLabel(hudSpLabelSprite_.get(), { statusPanelX + 27.0f, statusPanelY + 76.0f }, { 45.0f, 28.0f });

	if (hpGaugeBackgroundSprite_ && hpGaugeFillSprite_) {
		hpGaugeBackgroundSprite_->SetPosition({ statusGaugeX - 2.0f, statusPanelY + 30.0f });
		hpGaugeBackgroundSprite_->SetSize({ statusGaugeWidth + 4.0f, statusGaugeHeight + 4.0f });
		hpGaugeBackgroundSprite_->SetColor({ 0.04f, 0.03f, 0.08f, 0.92f });
		hpGaugeBackgroundSprite_->Update();
		hpGaugeBackgroundSprite_->Draw();
		const float hpRatio = std::clamp(
			static_cast<float>(context.player ? context.player->GetHP() : 0) / static_cast<float>(Player::kMaxHP),
			0.0f,
			1.0f);
		hpGaugeFillSprite_->SetPosition({ statusGaugeX, statusPanelY + 32.0f });
		hpGaugeFillSprite_->SetSize({ statusGaugeWidth * hpRatio, statusGaugeHeight });
		hpGaugeFillSprite_->SetColor({ 1.0f, 0.12f, 0.18f, 1.0f });
		hpGaugeFillSprite_->Update();
		hpGaugeFillSprite_->Draw();
	}

	// ボスが生存している間だけ、画面上部中央に専用HPバーを表示する。
	Boss* activeBoss = nullptr;
	if (context.enemies) {
		for (const auto& enemy : *context.enemies) {
			if (enemy && enemy->IsBoss() && !enemy->IsDead()) {
				activeBoss = static_cast<Boss*>(enemy.get());
				break;
			}
		}
	}
	if (activeBoss && bossHpGaugeBackgroundSprite_ && bossHpGaugeFillSprite_) {
		const float bossGaugeWidth = (std::min)(560.0f, (std::max)(screenWidth - 80.0f, 160.0f));
		constexpr float kBossGaugeHeight = 22.0f;
		const float bossGaugeX = (screenWidth - bossGaugeWidth) * 0.5f;
		constexpr float kBossGaugeY = 42.0f;
		const float hpRatio = std::clamp(
			static_cast<float>(activeBoss->GetHP()) / static_cast<float>(Boss::kMaxHP),
			0.0f,
			1.0f);

		bossHpGaugeBackgroundSprite_->SetPosition({ bossGaugeX - 4.0f, kBossGaugeY - 4.0f });
		bossHpGaugeBackgroundSprite_->SetSize({ bossGaugeWidth + 8.0f, kBossGaugeHeight + 8.0f });
		bossHpGaugeBackgroundSprite_->SetColor({ 0.05f, 0.07f, 0.15f, 0.94f });
		bossHpGaugeBackgroundSprite_->Update();
		bossHpGaugeBackgroundSprite_->Draw();

		bossHpGaugeFillSprite_->SetPosition({ bossGaugeX, kBossGaugeY });
		bossHpGaugeFillSprite_->SetSize({ bossGaugeWidth * hpRatio, kBossGaugeHeight });
		bossHpGaugeFillSprite_->SetColor({ 1.0f, 0.18f, 0.10f, 1.0f });
		bossHpGaugeFillSprite_->Update();
		bossHpGaugeFillSprite_->Draw();
	}

	if (hudNormalAmmoIconSprite_ && hudHomingAmmoIconSprite_) {
		hudNormalAmmoIconSprite_->SetTextureLeftTop({ 0.0f, 0.0f });
		hudNormalAmmoIconSprite_->SetTextureSize({ 887.0f, 887.0f });
		hudNormalAmmoIconSprite_->SetPosition({ ammoPanelX + 50.0f, normalAmmoPanelY + 67.0f });
		hudNormalAmmoIconSprite_->SetSize({ 42.0f, 42.0f });
		hudNormalAmmoIconSprite_->Update();
		hudNormalAmmoIconSprite_->Draw();
		hudHomingAmmoIconSprite_->SetTextureLeftTop({ 887.0f, 0.0f });
		hudHomingAmmoIconSprite_->SetTextureSize({ 887.0f, 887.0f });
		hudHomingAmmoIconSprite_->SetPosition({ ammoPanelX + 50.0f, homingAmmoPanelY + 67.0f });
		hudHomingAmmoIconSprite_->SetSize({ 42.0f, 42.0f });
		hudHomingAmmoIconSprite_->Update();
		hudHomingAmmoIconSprite_->Draw();
	}

	if (context.ammoManager) {
		const Vector4 normalAmmoColor = isSongActive
			? Vector4{ 1.0f, 0.88f, 0.12f, 1.0f }
			: (context.ammoManager->IsNormalReloading()
			? Vector4{ 1.0f, 1.0f, 0.65f, 1.0f }
			: Vector4{ 1.0f, 0.78f, 0.08f, 1.0f });
		const Vector4 homingAmmoColor = isSongActive
			? Vector4{ 1.0f, 0.88f, 0.12f, 1.0f }
			: (context.ammoManager->IsHomingReloading()
			? Vector4{ 1.0f, 0.65f, 0.65f, 1.0f }
			: Vector4{ 1.0f, 0.18f, 0.12f, 1.0f });
		// 歌の効果中は数字スプライトを隠し、ImGuiオーバーレイで ∞ を描く。
		if (!isSongActive) {
			DrawDigitText(
				std::to_string(context.ammoManager->GetNormalMagazine()) + "/" + std::to_string(context.ammoManager->GetNormalMagazineCapacity()),
				hudNormalAmmoDigitSprites_, ammoPanelX + 400.0f, normalAmmoTextY, normalAmmoColor);
			DrawDigitText(
				std::to_string(context.ammoManager->GetHomingMagazine()) + "/" + std::to_string(context.ammoManager->GetHomingMagazineCapacity()),
				hudHomingAmmoDigitSprites_, ammoPanelX + 400.0f, homingAmmoTextY, homingAmmoColor);
		}
		if (context.ammoManager->IsNormalReloading() && hudNormalReloadGaugeSprite_) {
			hudNormalReloadGaugeSprite_->SetPosition({ ammoPanelX + 142.0f, normalAmmoPanelY + 118.0f });
			hudNormalReloadGaugeSprite_->SetSize({ 150.0f * static_cast<float>(context.ammoManager->GetNormalReloadFrame()) / static_cast<float>(AmmoManager::kReloadDurationFrames), 5.0f });
			hudNormalReloadGaugeSprite_->SetColor({ 1.0f, 0.78f, 0.08f, 1.0f });
			hudNormalReloadGaugeSprite_->Update();
			hudNormalReloadGaugeSprite_->Draw();
		}
		if (context.ammoManager->IsHomingReloading() && hudHomingReloadGaugeSprite_) {
			hudHomingReloadGaugeSprite_->SetPosition({ ammoPanelX + 142.0f, homingAmmoPanelY + 118.0f });
			hudHomingReloadGaugeSprite_->SetSize({ 150.0f * static_cast<float>(context.ammoManager->GetHomingReloadFrame()) / static_cast<float>(AmmoManager::kReloadDurationFrames), 5.0f });
			hudHomingReloadGaugeSprite_->SetColor({ 1.0f, 0.18f, 0.12f, 1.0f });
			hudHomingReloadGaugeSprite_->Update();
			hudHomingReloadGaugeSprite_->Draw();
		}
	}

	if (spGaugeBackgroundSprite_ && spGaugeFillSprite_ && spGaugeCostMarkerSprite_ && context.specialAttackState) {
		spGaugeBackgroundSprite_->SetPosition({ gaugeX - 2.0f, gaugeY - 2.0f });
		spGaugeBackgroundSprite_->SetSize({ gaugeWidth + 4.0f, gaugeHeight + 4.0f });
		spGaugeBackgroundSprite_->SetColor({ 0.03f, 0.05f, 0.12f, 0.90f });
		spGaugeBackgroundSprite_->Update();
		spGaugeBackgroundSprite_->Draw();

		spGaugeFillSprite_->SetPosition({ gaugeX, gaugeY });
		spGaugeFillSprite_->SetSize({ gaugeWidth * std::clamp(context.specialAttackState->GetGauge() / 100.0f, 0.0f, 1.0f), gaugeHeight });
		spGaugeFillSprite_->SetColor(context.specialAttackState->IsActive()
			? Vector4{ 1.0f, 0.35f, 0.05f, 1.0f }
			: Vector4{ 0.05f, 0.75f, 1.0f, 1.0f });
		spGaugeFillSprite_->Update();
		spGaugeFillSprite_->Draw();

		spGaugeCostMarkerSprite_->SetPosition({ gaugeX + gaugeWidth * 0.5f - 1.0f, gaugeY });
		spGaugeCostMarkerSprite_->SetSize({ 2.0f, gaugeHeight });
		spGaugeCostMarkerSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.9f });
		spGaugeCostMarkerSprite_->Update();
		spGaugeCostMarkerSprite_->Draw();
	}

	// 歌ゲージは、蓄積中は撃破で得た残量、発動中は残り時間を表示する。
	const float songRatio = isSongActive
		? std::clamp(1.0f - static_cast<float>(songFrame) / static_cast<float>((std::max)(context.songDurationFrames, 1)), 0.0f, 1.0f)
		: std::clamp(songGauge / 100.0f, 0.0f, 1.0f);
	if (songGaugeBackgroundSprite_ && songGaugeFillSprite_ && songGaugeStateSprite_) {
		songGaugeBackgroundSprite_->SetPosition({ gaugeX - 2.0f, songGaugeY - 2.0f });
		songGaugeBackgroundSprite_->SetSize({ songGaugeWidth + 4.0f, gaugeHeight + 4.0f });
		songGaugeBackgroundSprite_->SetColor({ 0.06f, 0.02f, 0.12f, 0.94f });
		songGaugeBackgroundSprite_->Update();
		songGaugeBackgroundSprite_->Draw();

		const float songPulse = isSongActive ? 0.72f + 0.28f * std::sin(static_cast<float>(songFrame) * 0.24f) : 1.0f;
		songGaugeFillSprite_->SetPosition({ gaugeX, songGaugeY });
		songGaugeFillSprite_->SetSize({ songGaugeWidth * songRatio, gaugeHeight });
		songGaugeFillSprite_->SetColor(isSongActive
			? Vector4{ 1.0f, 0.16f, 0.76f, songPulse }
			: (songGauge >= SongState::kMaximumGauge
				? Vector4{ 0.42f, 1.0f, 0.30f, 1.0f }
				: Vector4{ 0.08f, 0.85f, 0.92f, 1.0f }));
		songGaugeFillSprite_->Update();
		songGaugeFillSprite_->Draw();

		// 右端の発光インジケータは READY / LIVE / CHARGE の状態を一目で区別させる。
		songGaugeStateSprite_->SetPosition({ gaugeX + songGaugeWidth + 6.0f, songGaugeY + 2.0f });
		songGaugeStateSprite_->SetSize({ 14.0f, 14.0f });
		songGaugeStateSprite_->SetColor(isSongActive
			? Vector4{ 1.0f, 0.22f, 0.78f, songPulse }
			: (songGauge >= SongState::kMaximumGauge
				? Vector4{ 0.42f, 1.0f, 0.30f, 1.0f }
				: Vector4{ 0.10f, 0.55f, 0.72f, 0.9f }));
		songGaugeStateSprite_->Update();
		songGaugeStateSprite_->Draw();
	}

	return layout;
}

void CombatHudSpriteRenderer::DrawLockedTargetOverheadHp(
	const Enemy* lockedEnemy,
	const LockOnManager* lockOnManager,
	const Camera* activeCamera,
	float screenWidth,
	float screenHeight) const {
	// ロックオン中の敵の頭上に、敵と一緒に動く HP バーを表示する。
	if (!lockedEnemy || lockedEnemy->IsDead() || !lockOnManager || !activeCamera ||
		!bossHpGaugeBackgroundSprite_ || !bossHpGaugeFillSprite_) {
		return;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetHudOverlayBounds(minX, minY, maxX, maxY)) {
		return;
	}

	const float overlayWidth = maxX - minX;
	const float overlayHeight = maxY - minY;
	const int lockedTurretIndex = lockOnManager->GetLockedBossTurretIndex();
	const float collisionRadius = lockOnManager->GetLockedTargetRadius();
	Vector3 hpBarWorldPosition = lockOnManager->GetLockedTargetPosition();
	int currentHp = lockedEnemy->GetHP();
	int maxHp = lockedEnemy->GetMaxHP();
	if (lockedTurretIndex >= 0 && lockedEnemy->GetLockOnPartCount() > 0) {
		currentHp = lockedEnemy->GetLockOnPartHP(static_cast<size_t>(lockedTurretIndex));
		maxHp = lockedEnemy->GetLockOnPartMaxHP(static_cast<size_t>(lockedTurretIndex));
	} else {
		hpBarWorldPosition.y += collisionRadius * 1.35f;
	}
	const Vector3 screenPosition = MyMath::WorldToScreen(
		hpBarWorldPosition, activeCamera->GetViewProjectionMatrix(), overlayWidth, overlayHeight);

	if (screenPosition.z < 0.0f || screenPosition.z > 1.0f ||
		screenPosition.x < 0.0f || screenPosition.x > overlayWidth ||
		screenPosition.y < 0.0f || screenPosition.y > overlayHeight) {
		return;
	}

	const float reticleSize = std::clamp(76.0f + collisionRadius * 12.0f, 76.0f, 116.0f);
	const float gaugeWidth = std::clamp(120.0f + collisionRadius * 24.0f, 140.0f, 250.0f);
	constexpr float kGaugeHeight = 10.0f;
	const float gaugeX = std::clamp(
		screenPosition.x * screenWidth / overlayWidth - gaugeWidth * 0.5f,
		48.0f,
		screenWidth - gaugeWidth - 8.0f);
	const float gaugeY = std::clamp(
		screenPosition.y * screenHeight / overlayHeight - reticleSize * 0.5f - 20.0f,
		30.0f,
		screenHeight - 20.0f);
	maxHp = (std::max)(1, maxHp);
	const float hpRatio = std::clamp(
		static_cast<float>(currentHp) / static_cast<float>(maxHp),
		0.0f,
		1.0f);

	bossHpGaugeBackgroundSprite_->SetPosition({ gaugeX - 2.0f, gaugeY - 2.0f });
	bossHpGaugeBackgroundSprite_->SetSize({ gaugeWidth + 4.0f, kGaugeHeight + 4.0f });
	bossHpGaugeBackgroundSprite_->SetColor({ 0.04f, 0.03f, 0.05f, 0.92f });
	bossHpGaugeBackgroundSprite_->Update();
	bossHpGaugeBackgroundSprite_->Draw();

	bossHpGaugeFillSprite_->SetPosition({ gaugeX, gaugeY });
	bossHpGaugeFillSprite_->SetSize({ gaugeWidth * hpRatio, kGaugeHeight });
	bossHpGaugeFillSprite_->SetColor({ 1.0f, 0.16f, 0.10f, 1.0f });
	bossHpGaugeFillSprite_->Update();
	bossHpGaugeFillSprite_->Draw();

	DrawLabel(hudHpLabelSprite_.get(), { gaugeX - 42.0f, gaugeY - 5.0f }, { 34.0f, 20.0f });
	DrawDigitText(std::to_string(currentHp) + "/" + std::to_string(maxHp),
		lockedTargetHpDigitSprites_, gaugeX + gaugeWidth, gaugeY - 24.0f,
		{ 1.0f, 0.88f, 0.32f, 1.0f });
}
