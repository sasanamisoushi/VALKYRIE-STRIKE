#pragma once

#include "2D/Sprite.h"
#include "engine/math/MyMath.h"

#include <array>
#include <cstddef>
#include <list>
#include <memory>

class Enemy;
class MissileManager;
class MultiLockChargeState;
class Player;

// 右上レーダー本体・スイープ線・敵ブリップ・相対高度目盛りの描画を管理する。
class RadarRenderer {
public:
	static constexpr size_t kMaxBlips = 32;

	void Initialize();
	void Draw(
		const Player* player,
		const std::list<std::unique_ptr<Enemy>>& enemies,
		const Enemy* lockedEnemy,
		const MultiLockChargeState& multiLockChargeState,
		const MissileManager* missileManager);

private:
	void DrawAltitudeSpriteElement(const Vector2& position, const Vector2& size, const Vector4& color) const;

	std::unique_ptr<Sprite> frameSprite_;
	std::unique_ptr<Sprite> sweepSprite_;
	std::array<std::unique_ptr<Sprite>, kMaxBlips> blipSprites_;
	float sweepAngle_ = 0.0f;
};
