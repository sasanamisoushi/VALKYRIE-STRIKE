#pragma once

#include "3D/Object3d.h"
#include "3D/Trail.h"
#include "engine/math/MyMath.h"

#include <array>
#include <cstddef>
#include <memory>

class Camera;
class Player;

// 歌（SONG）発動中だけの機体移動履歴リボン（胴体＋両翼端の3本）の更新・描画を管理する。
class SongFlightTrailRenderer {
public:
	static constexpr size_t kTrailCount = 3;

	void Initialize();
	void Reset();
	void Update(const Player& player);
	void Draw(Camera* renderCamera, const Player& player, int songFrame);

private:
	std::array<std::unique_ptr<Trail>, kTrailCount> trails_;
	std::array<std::unique_ptr<Object3d>, kTrailCount> objects_;
	std::array<Vector3, kTrailCount> heads_{};
	bool isInitialized_ = false;
};
