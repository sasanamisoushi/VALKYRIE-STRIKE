#include "TutorialGoalMarker.h"

#include "3D/Model.h"
#include "3D/Object3dCommon.h"
#include "Game/Player/Player.h"

#include <cmath>

void TutorialGoalMarker::Reset() {
	active_ = false;
	ring_.reset();
}

bool TutorialGoalMarker::Activate(Player* player, const std::list<std::unique_ptr<Obstacle>>& obstacles) {
	if (active_ || !player) {
		return false;
	}

	Vector3 stageCenter{};
	for (const auto& obstacle : obstacles) {
		if (obstacle && obstacle->IsStageBounds()) {
			stageCenter = obstacle->GetPosition();
			break;
		}
	}
	position_ = { stageCenter.x, player->GetPosition().y, stageCenter.z };
	active_ = true;
	if (!ring_) {
		ring_ = std::make_unique<Primitive>();
		ring_->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Ring);
	}
	if (Model* model = ring_->GetModel()) {
		model->SetColor({ 0.12f, 0.90f, 1.0f, 1.0f });
		model->SetEnableLighting(false);
	}
	return true;
}

void TutorialGoalMarker::Draw(int phaseFrames) {
	if (!active_ || !ring_) {
		return;
	}
	const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(phaseFrames) * 0.12f);
	ring_->SetTranslate(position_);
	ring_->SetRotate({ 0.0f, 0.0f, static_cast<float>(phaseFrames) * 0.018f });
	const float ringScale = 4.6f + pulse * 0.9f;
	ring_->SetScale({ ringScale, ringScale, 1.0f });
	ring_->Update();
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	ring_->Draw();
	Object3dCommon::GetInstance()->SetCommonDrawSettings();
}
