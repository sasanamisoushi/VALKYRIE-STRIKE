#include "SceneDebugEditorController.h"

#include "Game/Player/Player.h"
#include "Game/enemy/Boss.h"
#include "Game/enemy/Enemy.h"
#include "Game/obstacle/Obstacle.h"
#include "GamePlayUIManager.h"
#include "SimulationManager.h"
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/Debug/ImGuiManager.h"

#ifdef ENABLE_IMGUI
#include <externals/imgui/imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <string>

namespace {
Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

Vector3 NormalizeOrVector3(const Vector3& value, const Vector3& fallback) {
	const float length = MyMath::Length(value);
	if (length <= 0.0001f) {
		return fallback;
	}
	return { value.x / length, value.y / length, value.z / length };
}

Quaternion MakeLookQuaternion(const Vector3& forward) {
	const Vector3 normalizedForward = NormalizeOrVector3(forward, { 0.0f, 0.0f, 1.0f });
	const float clampedY = std::clamp(normalizedForward.y, -1.0f, 1.0f);
	const float pitch = -std::asin(clampedY);
	const float yaw = std::atan2(normalizedForward.x, normalizedForward.z);

	const Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
	const Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
	return MyMath::Normalize(MyMath::Multiply(qYaw, qPitch));
}
}

void SceneDebugEditorController::ResetSelectionState() {
	isBoxSelecting_ = false;
	boxSelectStartPos_ = { 0.0f, 0.0f };
	boxSelectEndPos_ = { 0.0f, 0.0f };
	clickedOnBone_ = false;
	isDraggingBone_ = false;
	mouseDownPos_ = { 0.0f, 0.0f };
}

void SceneDebugEditorController::ResetCameraToPlayer(
	FlyCamera* debugCamera,
	const Camera* fallbackCamera,
	const Player* player) const {
	if (!debugCamera) {
		return;
	}

	Vector3 target = fallbackCamera ? fallbackCamera->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
	Vector3 forward = { 0.0f, 0.0f, 1.0f };
	if (player) {
		target = player->GetPosition();
		forward = NormalizeOrVector3(player->GetForwardVector(), forward);
	}

	const Vector3 position = {
		target.x - forward.x * 20.0f,
		target.y - forward.y * 20.0f + 7.0f,
		target.z - forward.z * 20.0f,
	};
	debugCamera->SetTranslate(position);
	debugCamera->SetQuaternion(MakeLookQuaternion(SubtractVector3(target, position)));
	debugCamera->Camera::Update();
}

void SceneDebugEditorController::ResetCameraToStageOverview(
	FlyCamera* debugCamera,
	const Camera* fallbackCamera,
	const Player* player,
	const std::list<std::unique_ptr<Obstacle>>& obstacles) const {
	if (!debugCamera) {
		return;
	}

	// StageBounds はステージ全体を囲むため、ここを基準にすると
	// ステージの大きさが変わってもフリーカメラの初期位置が追従する。
	const Obstacle* stageBounds = nullptr;
	for (const auto& obstacle : obstacles) {
		if (obstacle && obstacle->IsStageBounds()) {
			stageBounds = obstacle.get();
			break;
		}
	}

	if (!stageBounds) {
		ResetCameraToPlayer(debugCamera, fallbackCamera, player);
		return;
	}

	const Vector3 target = stageBounds->GetPosition();
	const Vector3 halfExtents = stageBounds->GetWorldHalfExtents();
	const float stageRadius = (std::max)(halfExtents.x, halfExtents.z);
	const float distance = (std::max)(stageRadius * 2.2f, 80.0f);
	const Vector3 position = {
		target.x,
		target.y + distance * 0.85f,
		target.z - distance * 1.10f,
	};

	debugCamera->SetFovY(0.90f);
	debugCamera->SetFarClip((std::max)(distance * 4.0f, 3000.0f));
	debugCamera->SetTranslate(position);
	debugCamera->SetQuaternion(MakeLookQuaternion(SubtractVector3(target, position)));
	debugCamera->Camera::Update();
}

void SceneDebugEditorController::TryPlaceEnemyAtGameViewMouse(
	FlyCamera* debugCamera,
	GamePlayUIManager* uiManager,
	SimulationManager* simulationManager) const {
#ifdef ENABLE_IMGUI
	if (!uiManager || !uiManager->IsEnemyMousePlacementEnabled() || !debugCamera ||
		!simulationManager || !ImGuiManager::IsVisible() || ImGui::GetCurrentContext() == nullptr ||
		!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return;
	}

	Vector2 localMousePosition;
	if (!FlyCamera::GetGameViewMousePos(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y, localMousePosition)) {
		return;
	}

	float gameViewWidth = 0.0f;
	float gameViewHeight = 0.0f;
	FlyCamera::GetGameViewSize(gameViewWidth, gameViewHeight);
	if (gameViewWidth <= 0.0f || gameViewHeight <= 0.0f) {
		return;
	}

	const Ray ray = MyMath::ScreenToRay(
		localMousePosition,
		gameViewWidth,
		gameViewHeight,
		MyMath::Inverse(debugCamera->GetViewProjectionMatrix()));
	if (std::abs(ray.direction.y) <= 0.0001f) {
		uiManager->SetSimulationSaveMessage("配置面と視線が平行のため、敵を配置できませんでした");
		return;
	}

	const float placementHeight = uiManager->GetEnemyMousePlacementHeight();
	const float distance = (placementHeight - ray.origin.y) / ray.direction.y;
	if (distance <= 0.0f) {
		uiManager->SetSimulationSaveMessage("クリック位置がカメラの後方にあるため、敵を配置できませんでした");
		return;
	}

	const Vector3 position = {
		ray.origin.x + ray.direction.x * distance,
		placementHeight,
		ray.origin.z + ray.direction.z * distance,
	};
	const float degreesToRadians = 3.141592654f / 180.0f;
	const float* rotationDegrees = uiManager->GetBlenderEnemyRotationDegrees();
	const Vector3 rotation = {
		rotationDegrees[0] * degreesToRadians,
		rotationDegrees[1] * degreesToRadians,
		rotationDegrees[2] * degreesToRadians,
	};
	static constexpr const char* enemyTypes[] = { "VF1", "VF1-1", "VF3", "Jammer", "Boss" };
	const int typeIndex = std::clamp(uiManager->GetBlenderEnemyTypeIndex(), 0, static_cast<int>(std::size(enemyTypes)) - 1);
	simulationManager->AddEnemySpawnToSceneJson(
		"resources/scene.json", uiManager->GetBlenderEnemyName(), enemyTypes[typeIndex], position, rotation);
#else
	(void)debugCamera;
	(void)uiManager;
	(void)simulationManager;
#endif
}

void SceneDebugEditorController::UpdateAnimationEditorBoneSelection(
	FlyCamera* debugCamera,
	Player* player,
	SimulationManager* simulationManager) {
#ifdef ENABLE_IMGUI
	if (!debugCamera || !simulationManager || !ImGui::GetCurrentContext()) {
		return;
	}

	simulationManager->UpdateShortcuts();
	ImGuiIO& io = ImGui::GetIO();
	Vector2 localMousePos;
	if (!FlyCamera::GetGameViewMousePos(io.MousePos.x, io.MousePos.y, localMousePos)) {
		return;
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		float gw = 0.0f;
		float gh = 0.0f;
		FlyCamera::GetGameViewSize(gw, gh);
		Ray ray = MyMath::ScreenToRay(localMousePos, gw, gh, MyMath::Inverse(debugCamera->GetViewProjectionMatrix()));

		float closestDist = 999999.0f;
		std::string closestBone;

		if (player) {
			const Skeleton& playerSkeleton = player->GetSkeleton();
			for (const auto& joint : playerSkeleton.joints) {
				Vector3 pos = {
					joint.skeletonSpaceMatrix.m[3][0],
					joint.skeletonSpaceMatrix.m[3][1],
					joint.skeletonSpaceMatrix.m[3][2]
				};

				Vector3 currentScale = { 1.0f, 1.0f, 1.0f };
				if (player->GetObject3d()) {
					currentScale = player->GetObject3d()->GetScale();
					Matrix4x4 worldMat = MyMath::MakeAffineMatrix(currentScale, player->GetQuaternion(), player->GetPosition());
					pos = MyMath::Transform(pos, worldMat);
				}

				Sphere s;
				s.center = pos;
				s.radius = 0.5f * currentScale.x;

				float dist = 0.0f;
				if (MyMath::IntersectRaySphere(ray, s, &dist)) {
					if (dist < closestDist) {
						closestDist = dist;
						closestBone = joint.name;
					}
				}
			}
		}

		clickedOnBone_ = !closestBone.empty();
		mouseDownPos_ = { io.MousePos.x, io.MousePos.y };

		if (clickedOnBone_) {
			if (io.KeyShift) {
				if (simulationManager->IsBoneSelected(closestBone)) {
					simulationManager->RemoveSelectedBoneName(closestBone);
				} else {
					simulationManager->AddSelectedBoneName(closestBone);
				}
			} else {
				if (!simulationManager->IsBoneSelected(closestBone)) {
					simulationManager->ClearSelectedBones();
					simulationManager->AddSelectedBoneName(closestBone);
				}
			}
			isBoxSelecting_ = false;
			isDraggingBone_ = true;
		} else {
			if (io.KeyShift || simulationManager->GetSelectedBoneNames().empty()) {
				isBoxSelecting_ = true;
				isDraggingBone_ = false;
				boxSelectStartPos_ = localMousePos;
				boxSelectEndPos_ = localMousePos;
			} else {
				isBoxSelecting_ = false;
				isDraggingBone_ = true;
			}
		}
	}

	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		const auto& selBones = simulationManager->GetSelectedBoneNames();
		const float dx = io.MouseDelta.x;
		const float dy = io.MouseDelta.y;

		if (isBoxSelecting_) {
			Vector2 dragMousePos;
			if (FlyCamera::GetGameViewMousePos(io.MousePos.x, io.MousePos.y, dragMousePos)) {
				boxSelectEndPos_ = dragMousePos;
			}
		} else if (isDraggingBone_ && !selBones.empty()) {
			if (dx != 0.0f || dy != 0.0f) {
				const bool doTranslate = io.KeyCtrl;
				for (const auto& boneName : selBones) {
					if (doTranslate) {
						simulationManager->AddBoneTranslationFromDrag(boneName, dx, dy);
					} else {
						simulationManager->AddBoneRotationFromDrag(boneName, dx, dy);
					}
				}
			}
		}
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		if (isBoxSelecting_) {
			isBoxSelecting_ = false;

			const float minX = (std::min)(boxSelectStartPos_.x, boxSelectEndPos_.x);
			const float maxX = (std::max)(boxSelectStartPos_.x, boxSelectEndPos_.x);
			const float minY = (std::min)(boxSelectStartPos_.y, boxSelectEndPos_.y);
			const float maxY = (std::max)(boxSelectStartPos_.y, boxSelectEndPos_.y);

			if (maxX - minX > 2.0f && maxY - minY > 2.0f && player) {
				float gw = 0.0f;
				float gh = 0.0f;
				FlyCamera::GetGameViewSize(gw, gh);
				const Matrix4x4 vpMat = debugCamera->GetViewProjectionMatrix();
				const Skeleton& playerSkeleton = player->GetSkeleton();

				for (const auto& joint : playerSkeleton.joints) {
					Vector3 pos = {
						joint.skeletonSpaceMatrix.m[3][0],
						joint.skeletonSpaceMatrix.m[3][1],
						joint.skeletonSpaceMatrix.m[3][2]
					};

					if (player->GetObject3d()) {
						const Vector3 currentScale = player->GetObject3d()->GetScale();
						const Matrix4x4 worldMat = MyMath::MakeAffineMatrix(currentScale, player->GetQuaternion(), player->GetPosition());
						pos = MyMath::Transform(pos, worldMat);
					}

					const Vector3 screenPos = MyMath::WorldToScreen(pos, vpMat, gw, gh);
					if (screenPos.z > 0.0f && screenPos.z < 1.0f) {
						if (screenPos.x >= minX && screenPos.x <= maxX &&
							screenPos.y >= minY && screenPos.y <= maxY) {
							simulationManager->AddSelectedBoneName(joint.name);
						}
					}
				}
			}
		} else {
			if (!clickedOnBone_ && !io.KeyShift) {
				const float deltaX = io.MousePos.x - mouseDownPos_.x;
				const float deltaY = io.MousePos.y - mouseDownPos_.y;
				if (std::abs(deltaX) < 2.0f && std::abs(deltaY) < 2.0f) {
					simulationManager->ClearSelectedBones();
				}
			}
		}
		isDraggingBone_ = false;
	}

	if (isBoxSelecting_) {
		float minViewX = 0.0f;
		float minViewY = 0.0f;
		float maxViewX = 0.0f;
		float maxViewY = 0.0f;
		if (FlyCamera::GetGameViewBounds(minViewX, minViewY, maxViewX, maxViewY)) {
			const ImVec2 start(minViewX + boxSelectStartPos_.x, minViewY + boxSelectStartPos_.y);
			const ImVec2 end(minViewX + boxSelectEndPos_.x, minViewY + boxSelectEndPos_.y);
			const ImU32 colFill = IM_COL32(100, 150, 250, 80);
			const ImU32 colBorder = IM_COL32(150, 200, 255, 200);
			ImGui::GetForegroundDrawList()->AddRectFilled(start, end, colFill);
			ImGui::GetForegroundDrawList()->AddRect(start, end, colBorder, 0.0f, 1.5f, 0);
		}
	}
#else
	(void)debugCamera;
	(void)player;
	(void)simulationManager;
#endif
}

void SceneDebugEditorController::UpdateBossEditorPartSelection(
	Camera* editorCamera,
	bool isEditorPreviewPlaying,
	GamePlayUIManager* uiManager,
	const std::list<std::unique_ptr<Enemy>>& enemies) const {
#ifdef ENABLE_IMGUI
	// 停止中のボス編集では、Game View 上で本体と後部スラスターを直接選択する。
	const bool isBossEditor = uiManager &&
		uiManager->IsBossEditorTargetSelected() && !isEditorPreviewPlaying &&
		ImGuiManager::IsVisible() && ImGui::GetCurrentContext() != nullptr;
	if (!isBossEditor) {
		return;
	}

	Vector2 localMousePosition;
	ImGuiIO& io = ImGui::GetIO();
	if (editorCamera && FlyCamera::GetGameViewMousePos(io.MousePos.x, io.MousePos.y, localMousePosition)) {
		Boss* activeBoss = nullptr;
		for (const auto& enemy : enemies) {
			if (enemy && enemy->IsBoss() && !enemy->IsDead()) {
				activeBoss = dynamic_cast<Boss*>(enemy.get());
				if (activeBoss) break;
			}
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			uiManager->SetSelectedBossEditorPart(-1);
			uiManager->SetBossEditorSelectionDragging(false);
			float gameViewWidth = 0.0f;
			float gameViewHeight = 0.0f;
			FlyCamera::GetGameViewSize(gameViewWidth, gameViewHeight);
			if (activeBoss && gameViewWidth > 0.0f && gameViewHeight > 0.0f) {
				const Ray ray = MyMath::ScreenToRay(localMousePosition, gameViewWidth, gameViewHeight,
					MyMath::Inverse(editorCamera->GetViewProjectionMatrix()));
				float closestDistance = (std::numeric_limits<float>::max)();
				const Boss::Tuning& tuning = activeBoss->GetTuning();
				for (size_t index = 0; index < activeBoss->GetRearThrusterCount(); ++index) {
					Sphere thrusterSphere{};
					thrusterSphere.center = activeBoss->GetRearThrusterWorldPosition(index);
					thrusterSphere.radius = (std::max)(0.55f, tuning.rearThrusterRadius * 3.0f);
					float distance = 0.0f;
					if (MyMath::IntersectRaySphere(ray, thrusterSphere, &distance) && distance < closestDistance) {
						closestDistance = distance;
						uiManager->SetSelectedBossEditorPart(static_cast<int>(index) + 1);
					}
				}
				Sphere bossSphere{};
				bossSphere.center = activeBoss->GetPosition();
				bossSphere.radius = activeBoss->GetCollisionRadius();
				float bodyDistance = 0.0f;
				if (MyMath::IntersectRaySphere(ray, bossSphere, &bodyDistance) && bodyDistance < closestDistance) {
					uiManager->SetSelectedBossEditorPart(0);
				}
				uiManager->SetBossEditorSelectionDragging(uiManager->GetSelectedBossEditorPart() >= 0);
			}
		}

		if (activeBoss && uiManager->IsBossEditorSelectionDragging() && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
			(io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
			// カメラの右・上方向へ動かすため、現在の見た目に沿ってドラッグできる。
			const Matrix4x4 inverseView = MyMath::Inverse(editorCamera->GetViewMatrix());
			const Vector3 cameraOrigin = MyMath::Transform({ 0.0f, 0.0f, 0.0f }, inverseView);
			const Vector3 rightPoint = MyMath::Transform({ 1.0f, 0.0f, 0.0f }, inverseView);
			const Vector3 upPoint = MyMath::Transform({ 0.0f, 1.0f, 0.0f }, inverseView);
			const Vector3 dragDelta = {
				((rightPoint.x - cameraOrigin.x) * io.MouseDelta.x - (upPoint.x - cameraOrigin.x) * io.MouseDelta.y) * 0.015f,
				((rightPoint.y - cameraOrigin.y) * io.MouseDelta.x - (upPoint.y - cameraOrigin.y) * io.MouseDelta.y) * 0.015f,
				((rightPoint.z - cameraOrigin.z) * io.MouseDelta.x - (upPoint.z - cameraOrigin.z) * io.MouseDelta.y) * 0.015f,
			};
			if (uiManager->GetSelectedBossEditorPart() == 0) {
				const Vector3 position = activeBoss->GetPosition();
				const Vector3 newPosition = { position.x + dragDelta.x, position.y + dragDelta.y, position.z + dragDelta.z };
				activeBoss->SetPosition(newPosition);
				activeBoss->UpdateModel();
				uiManager->SetBossBattlePosition(newPosition);
			} else {
				const size_t thrusterIndex = static_cast<size_t>(uiManager->GetSelectedBossEditorPart() - 1);
				if (thrusterIndex < activeBoss->GetRearThrusterCount()) {
					Boss::Tuning tuning = activeBoss->GetTuning();
					const Vector3 rotation = activeBoss->GetRotation();
					const float inverseYawCos = std::cos(-rotation.y);
					const float inverseYawSin = std::sin(-rotation.y);
					const Vector3 bossSpaceDelta = {
						dragDelta.x * inverseYawCos + dragDelta.z * inverseYawSin,
						dragDelta.y,
						-dragDelta.x * inverseYawSin + dragDelta.z * inverseYawCos,
					};
					const Vector3 scale = activeBoss->GetScale();
					Vector3& offset = tuning.rearThrusterIndividualOffsets[thrusterIndex];
					offset.x += bossSpaceDelta.x / (std::max)(scale.x, 0.0001f);
					offset.y += bossSpaceDelta.y / (std::max)(scale.y, 0.0001f);
					offset.z += bossSpaceDelta.z / (std::max)(scale.z, 0.0001f);
					activeBoss->SetTuning(tuning);
					activeBoss->UpdateModel();
					uiManager->SetBossTuning(tuning);
				}
			}
		}
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		uiManager->SetBossEditorSelectionDragging(false);
	}
#else
	(void)editorCamera;
	(void)isEditorPreviewPlaying;
	(void)uiManager;
	(void)enemies;
#endif
}
