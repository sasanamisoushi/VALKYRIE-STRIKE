#pragma once

#include <string>

// チュートリアルで各操作に必要な回数。進行ロジックはこの値を読み取り専用で使う。
struct TutorialRequirements {
	int forward = 3;
	int altitude = 3;
	int turn = 3;
	int gerwalkHover = 3;
	int gerwalkStrafe = 3;
	int lockOn = 3;
	int normalShots = 3;
	int fighterTransform = 1;
	int homingLaunches = 1;
};

// チュートリアル条件の値検証と JSON 永続化を管理する。
class TutorialRequirementsManager {
public:
	const TutorialRequirements& Get() const { return requirements_; }
	void Set(TutorialRequirements requirements);
	void Reset();
	bool Load(const std::string& filePath);
	bool Save(const std::string& filePath) const;

private:
	void ApplyLimits();

	TutorialRequirements requirements_;
};
