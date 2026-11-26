#pragma once

#include "engine/math/MyMath.h"
#include <cstddef>

class Boss;

// ボス側面六角形エネルギーシールドと発生装置の初期化・状態遷移・当たり判定・描画を担当する。
class BossSideShieldController {
public:
	void Initialize(Boss& boss) const;
	void Update(Boss& boss) const;
	void Draw(const Boss& boss) const;
	void AdvanceState(Boss& boss) const;
	bool IsVisible(const Boss& boss) const;
	bool IsProtectingFace(const Boss& boss, size_t face) const;
	bool IsGeneratorVulnerable(const Boss& boss, size_t face) const;
	bool ApplyGeneratorDamage(Boss& boss, size_t face, int damage) const;
	bool GetActiveImpact(const Boss& boss, const Vector3& hitPosition, float impactRadius, Vector3& outNormal) const;
	bool TryGetFaceAtWorldPosition(const Boss& boss, const Vector3& worldPosition, float radius, size_t& outFace) const;
};
