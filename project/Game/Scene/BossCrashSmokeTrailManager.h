#pragma once

#include "3D/Trail.h"
#include "3D/Object3d.h"

#include <array>
#include <memory>

// ボス墜落演出に使う煙の軌跡リソースを所有する。
class BossCrashSmokeTrailManager {
public:
	static constexpr size_t kTrailCount = 3;

	void Initialize();
	void Reset();
	void SetOrigin(size_t index, const Vector3& position);

private:
	std::array<std::unique_ptr<Trail>, kTrailCount> trails_;
	std::array<std::unique_ptr<Object3d>, kTrailCount> objects_;
};
