#include "BossCrashSmokeTrailManager.h"

#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"

#include <string>

void BossCrashSmokeTrailManager::Initialize() {
	for (size_t index = 0; index < trails_.size(); ++index) {
		const std::string modelName = "BossCrashSmokeTrail" + std::to_string(index);
		ModelManager::GetInstance()->CreateTrailModel(modelName);
		trails_[index] = std::make_unique<Trail>();
		trails_[index]->Initialize(96);
		objects_[index] = std::make_unique<Object3d>();
		objects_[index]->Initialize(Object3dCommon::GetInstance());
		objects_[index]->SetModel(modelName);
		if (objects_[index]->GetModel()) {
			objects_[index]->GetModel()->SetTextureFilePath("resources/circle.png");
		}
	}
}

void BossCrashSmokeTrailManager::Reset() {
	for (auto& trail : trails_) {
		if (trail) {
			trail->Initialize(96);
		}
	}
}

void BossCrashSmokeTrailManager::SetOrigin(size_t index, const Vector3& position) {
	if (index < trails_.size() && trails_[index]) {
		trails_[index]->SetOrigin(position);
	}
}
