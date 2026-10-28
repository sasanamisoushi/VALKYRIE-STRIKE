#pragma once
#include "engine/Camera/Camera.h"
#include "2D/Sprite.h"
#include "Game/base/BaseScene.h"
#include <memory>

class ClearScene : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<Sprite> resultBackgroundSprite_;
	std::unique_ptr<Sprite> resultFlowingWorldPrimarySprite_;
	std::unique_ptr<Sprite> resultFlowingWorldSecondarySprite_;
	std::unique_ptr<Sprite> resultStaticAircraftSprite_;
	std::unique_ptr<Sprite> resultCloudFlowNearSprite_;
	std::unique_ptr<Sprite> resultCloudFlowFarSprite_;
	std::unique_ptr<Sprite> resultHudRingTopRightSprite_;
	std::unique_ptr<Sprite> resultHudRingBottomLeftSprite_;
	std::unique_ptr<Sprite> transitionFadeSprite_;
	int resultAnimationFrame_ = 0;
	float transitionFadeAlpha_ = 1.0f;
	bool isReturningToTitle_ = false;
	int returnToTitleFrame_ = 0;
	static constexpr int kShutdownDurationFrames = 38;
};


