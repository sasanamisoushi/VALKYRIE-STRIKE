#pragma once

#include "3D/Primitive.h"
#include "Game/obstacle/Obstacle.h"

#include <list>
#include <memory>

class Player;

// チュートリアル終了地点の位置決定とリング表示を管理する。
class TutorialGoalMarker {
public:
	void Reset();
	void Deactivate() { active_ = false; }
	bool Activate(Player* player, const std::list<std::unique_ptr<Obstacle>>& obstacles);
	void Draw(int phaseFrames);

	bool IsActive() const { return active_; }
	const Vector3& GetPosition() const { return position_; }

private:
	bool active_ = false;
	Vector3 position_{};
	std::unique_ptr<Primitive> ring_;
};
