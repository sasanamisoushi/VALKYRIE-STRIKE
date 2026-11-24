#pragma once

#include "engine/math/MyMath.h"

#include <string>

class Player;

// 境界シールドの見た目・警告距離と、その永続化をまとめて管理する。
// シーンは設定値を直接検証・保存せず、このクラスの API を通して利用する。
struct BoundaryShieldSettings {
	float warningDistance = 50.0f;
	float wallWidth = 18.0f;
	float wallHeight = 12.0f;
	float ceilingWidth = 22.0f;
	float ceilingHeight = 18.0f;
	float maxOpacity = 0.68f;
	float pulseSpeed = 0.05f;
	float pulseStrength = 0.14f;
	float surfaceOffset = 0.5f;
	Vector3 color = { 1.0f, 0.30f, 0.24f };
};

class BoundaryShieldSettingsManager {
public:
	const BoundaryShieldSettings& Get() const { return settings_; }

	// UI が編集した値を一括で受け取り、許容範囲へ丸めて即時反映する。
	void Set(BoundaryShieldSettings settings, Player* player);
	void Reset(Player* player);
	void Apply(Player* player);
	bool Load(const std::string& filePath, Player* player);
	bool Save(const std::string& filePath) const;

private:
	BoundaryShieldSettings settings_;
};
