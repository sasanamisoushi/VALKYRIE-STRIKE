#include "GameplayWorldRenderer.h"

#include "3D/Object3d.h"
#include "3D/Object3dCommon.h"
#include "BoundaryShieldRenderer.h"
#include "BoundaryShieldSettingsManager.h"
#include "DebugColliderRenderer.h"
#include "EnemyDefeatEffectController.h"
#include "EnvironmentRenderer.h"
#include "Game/Player/Player.h"
#include "Game/bullet/MissileManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"
#include "SongFlightTrailRenderer.h"
#include "TutorialGoalMarker.h"
#include "WarpExitPresentationState.h"
#include "engine/Camera/Camera.h"
#include "engine/Particle/ExplosionManager.h"
#include "engine/math/MyMath.h"

void GameplayWorldRenderer::Draw(const GameplayWorldDrawContext& context) {
	if (!context.renderCamera) {
		return;
	}
	Object3dCommon::GetInstance()->SetDefaultCamera(context.renderCamera);

	// 深度を書き込まない炎やパーティクルを、背景で上書きしない。
	if (context.environmentRenderer) {
		context.environmentRenderer->DrawBackground();
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	const bool shouldDrawPlayer = context.player && context.player->GetPresentationAlpha() > 0.001f;
	if (shouldDrawPlayer) {
		// 機体・ノズルは不透明パス。炎は地形の深度が完成してから描く。
		if (context.isEditorPreviewPlaying && context.player->GetBoosterEffect()) {
			// 編集UIを閉じて再生した際に位置確認用の黄色い球を残さない。
			context.player->GetBoosterEffect()->SetPlacementMarkersVisible(false);
		}
		context.player->Draw(context.renderCamera, false);
	}

	if (context.missileManager) {
		context.missileManager->Draw();
	}

	if (context.enemyBulletManager && !context.hideEnemyBulletsForIntro) {
		context.enemyBulletManager->Draw(context.renderCamera);
	}

	Vector4 frustumPlanes[6];
	MyMath::ExtractFrustumPlanes(context.renderCamera->GetViewProjectionMatrix(), frustumPlanes);

	if (context.enemies) {
		for (const auto& enemy : *context.enemies) {
			Sphere enemySphere;
			enemySphere.center = enemy->GetPosition();
			enemySphere.radius = enemy->GetCollisionRadius();
			if (MyMath::IsInFrustum(enemySphere, frustumPlanes)) {
				enemy->Draw();
				Object3dCommon::GetInstance()->SetCommonDrawSettings();
			}
		}
	}

	if (context.enemyDefeatEffectController) {
		context.enemyDefeatEffectController->Draw(frustumPlanes);
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	if (context.obstacles) {
		for (const auto& obstacle : *context.obstacles) {
			Sphere obsSphere;
			obsSphere.center = obstacle->GetPosition();
			obsSphere.radius = MyMath::Length(obstacle->GetWorldHalfExtents());
			if (MyMath::IsInFrustum(obsSphere, frustumPlanes)) {
				obstacle->Draw();
				Object3dCommon::GetInstance()->SetCommonDrawSettings();
			}
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	if (context.showPlane && context.objects) {
		for (Object3d* object3d : *context.objects) {
			object3d->Draw();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	if (context.showModel && context.myModelObject) {
		context.myModelObject->Draw();
	}

	// ゴールリングは地面・障害物などの不透明モデルが深度を書き終えた後に描く。
	if (context.tutorialGoalMarker) {
		context.tutorialGoalMarker->Draw(context.tutorialPhaseFrames);
	}

	// 地形・障害物を含む不透明物の深度を書き終えてから、敵の半透明部位を描く。
	if (context.enemies) {
		for (const auto& enemy : *context.enemies) {
			Sphere enemySphere;
			enemySphere.center = enemy->GetPosition();
			enemySphere.radius = enemy->GetCollisionRadius();
			if (MyMath::IsInFrustum(enemySphere, frustumPlanes)) {
				enemy->DrawTransparent();
			}
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	// 訓練中は敵も危険地帯も存在しないため、境界シールドを出さない。
	if (context.boundaryShieldRenderer && context.boundaryShieldSettings) {
		context.boundaryShieldRenderer->Draw(!context.isTutorialEnabled, context.player, *context.boundaryShieldSettings);
	}

	if (context.showBones) {
		Object3dCommon::GetInstance()->SetCommonDrawSettings();
		if (context.skeletonLinesObject && context.skeletonLinesObject->GetModel()) {
			context.skeletonLinesObject->Draw();
		}
	}

	if (context.debugColliderRenderer) {
		context.debugColliderRenderer->Draw(context.showDebugColliders);
	}

	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (context.environmentRenderer) {
		context.environmentRenderer->Draw();
	}
	if (context.warpExitPresentation) {
		context.warpExitPresentation->DrawGate();
	}

	// 歌の発動中だけ、ノズルではなく胴体・翼端が通った軌跡をリボンとして描く。
	if (context.isSongActive && context.player && context.songFlightTrailRenderer) {
		context.songFlightTrailRenderer->Draw(context.renderCamera, *context.player, context.songFrame);
	}

	// すべての不透明オブジェクトの後に、実際の噴射を加算する。
	if (shouldDrawPlayer) {
		context.player->DrawEffects(context.renderCamera);
	}

	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (context.explosionManager) {
		context.explosionManager->Draw();
	}
}
