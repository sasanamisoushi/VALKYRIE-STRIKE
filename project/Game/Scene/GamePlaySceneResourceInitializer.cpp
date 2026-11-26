#include "GamePlaySceneResourceInitializer.h"

#include "3D/Model.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "Game/Player/Player.h"
#include "Game/Scene/GameCameraManager.h"
#include "Game/bullet/MissileManager.h"
#include "Game/editor/EditorReceiver.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"
#include "engine/Particle/ExplosionManager.h"
#include "engine/Particle/ParticleManager.h"

void GamePlaySceneResourceInitializer::InitializeBaseModelsAndPrimitives(const GamePlaySceneDebugPrimitives& targets) {
	ModelManager* modelManager = ModelManager::GetInstance();
	modelManager->LoadModel("plane.obj");
	modelManager->LoadModel("multiMesh.obj");
	modelManager->CreateSphereModel("Sphere", 16);
	modelManager->CreateSphereModel("EnemyBulletSphere", 6);
	// 地上敵の射撃用。球を進行方向へ細長く伸ばすことで、箱型ではない丸いビーム芯になる。
	modelManager->CreateSphereModel("GroundBeamCore", 12);
	if (Model* model = modelManager->FindModel("GroundBeamCore")) {
		model->SetColor({ 0.10f, 0.90f, 1.0f, 1.0f });
		model->SetEnableLighting(false);
	}
	// ボス主砲は地上敵のビームと同じ丸い芯・残光を共有しつつ、射撃時だけ大きく伸ばす。
	modelManager->CreateSphereModel("BossGroundBeam", 12);
	if (Model* model = modelManager->FindModel("BossGroundBeam")) {
		model->SetColor({ 0.28f, 0.96f, 1.0f, 1.0f });
		model->SetEnableLighting(false);
	}

	if (targets.sphere) {
		*targets.sphere = std::make_unique<Primitive>();
		(*targets.sphere)->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Sphere);
		(*targets.sphere)->SetTranslate({ 2.0f, 0.0f, 0.0f });
		if ((*targets.sphere)->GetModel()) {
			(*targets.sphere)->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
		}
	}

	if (targets.box) {
		*targets.box = std::make_unique<Primitive>();
		(*targets.box)->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Box);
		(*targets.box)->SetTranslate({ -2.0f, 0.0f, 0.0f });
	}

	if (targets.animatedCube && targets.animationData && targets.skeleton) {
		*targets.animatedCube = std::make_unique<Object3d>();
		(*targets.animatedCube)->Initialize(Object3dCommon::GetInstance());
		modelManager->LoadModel("AnimatedCube/AnimatedCube.gltf");
		(*targets.animatedCube)->SetModel("AnimatedCube/AnimatedCube.gltf");

		*targets.animationData = LoadAnimationFile("resources/AnimatedCube", "AnimatedCube.gltf");
		Node rootNode = Model::LoadNodeHierarchy("resources/AnimatedCube", "AnimatedCube.gltf");
		*targets.skeleton = CreateSkeleton(rootNode);
		if (!targets.skeleton->joints.empty()) {
			targets.skeleton->joints[targets.skeleton->root].transform.translate = { 0.0f, 0.0f, 0.0f };
		}
		(*targets.animatedCube)->skinCluster = (*targets.animatedCube)->GetModel()->CreateSkinCluster(*targets.skeleton);
	}

	modelManager->CreateLineModel("SkeletonLines");
	if (targets.skeletonLinesObject) {
		*targets.skeletonLinesObject = std::make_unique<Object3d>();
		(*targets.skeletonLinesObject)->Initialize(Object3dCommon::GetInstance());
		(*targets.skeletonLinesObject)->SetModel("SkeletonLines");
	}

	modelManager->CreateTrailModel("SmokeTrail");
	if (targets.missileTrail) {
		*targets.missileTrail = std::make_unique<Trail>();
		(*targets.missileTrail)->Initialize(60);
	}
	if (targets.trailObject) {
		*targets.trailObject = std::make_unique<Object3d>();
		(*targets.trailObject)->Initialize(Object3dCommon::GetInstance());
		(*targets.trailObject)->SetModel("SmokeTrail");
	}

	modelManager->CreateBoxModel("PlayerBox");
	modelManager->CreateBoxModel("EnemyBox");
	modelManager->CreateBoxModel("BossHull");
	modelManager->CreateBoxModel("BossCannon");
	modelManager->CreateBoxModel("BossBeam");
	if (Model* model = modelManager->FindModel("BossHull")) model->SetColor({ 0.18f, 0.22f, 0.35f, 1.0f });
	if (Model* model = modelManager->FindModel("BossCannon")) model->SetColor({ 1.0f, 0.35f, 0.05f, 1.0f });
	if (Model* model = modelManager->FindModel("BossBeam")) model->SetColor({ 0.15f, 0.8f, 1.0f, 1.0f });
	modelManager->CreateBoxModel("ObstacleBox");
}

void GamePlaySceneResourceInitializer::InitializeParticleGroups(ParticleManager* particleManager,
	const char* songParticleGroup, const char* speedRushParticleGroup) {
	if (!particleManager) {
		return;
	}
	particleManager->CreateParticleGroup("test", "resources/circle.png");
	particleManager->CreateParticleGroup("smoke", "resources/circle.png");
	particleManager->CreateParticleGroup("blackSmoke", "resources/circle.png");
	particleManager->SetParticleGroupAlphaBlend("blackSmoke", true);
	// 歌の最中だけプレイヤーの後方へ流す、黄色いスラスター状の残光。
	particleManager->CreateParticleGroup(songParticleGroup, "resources/circle.png");
	// 低空高速時にカメラ近傍を横切る、細い大気・ダストの流れ。
	particleManager->CreateParticleGroup(speedRushParticleGroup, "resources/warp_speedline_particle.png");
	// 地面すれすれの飛行時にだけ、機体真下から舞い上がる土埃（ジェット風圧・巨大砂煙トレイル・砂礫デブリ）。
	particleManager->CreateParticleGroup("lowAltitudeDownwash", "resources/low_altitude_dust_puff.png");
	particleManager->SetParticleGroupAlphaBlend("lowAltitudeDownwash", true);
	particleManager->CreateParticleGroup("lowAltitudeDustTrail", "resources/low_altitude_dust_puff.png");
	particleManager->SetParticleGroupAlphaBlend("lowAltitudeDustTrail", true);
	particleManager->CreateParticleGroup("lowAltitudeDustDebris", "resources/white1x1.png");
	particleManager->SetParticleGroupAlphaBlend("lowAltitudeDustDebris", true);
}

void GamePlaySceneResourceInitializer::InitializeDebugCamera(
	std::unique_ptr<FlyCamera>& debugFlyCamera, bool& isDebugCameraActive) {
	debugFlyCamera = std::make_unique<FlyCamera>();
	debugFlyCamera->SetTranslate({ 0.0f, 5.0f, -20.0f });
	isDebugCameraActive = false;
}

void GamePlaySceneResourceInitializer::InitializeCombatManagers(
	const GamePlaySceneCombatManagers& targets, const char* playerModelName, ParticleManager* particleManager) {
	if (targets.player) {
		*targets.player = std::make_unique<Player>();
		(*targets.player)->Initialize(playerModelName);
	}
	if (targets.missileManager) {
		*targets.missileManager = std::make_unique<MissileManager>();
		(*targets.missileManager)->Initialize(particleManager);
	}
	if (targets.explosionManager) {
		*targets.explosionManager = std::make_unique<ExplosionManager>();
		(*targets.explosionManager)->Initialize(particleManager);
	}
	if (targets.enemyBulletManager) {
		*targets.enemyBulletManager = std::make_unique<EnemyBulletManager>();
		(*targets.enemyBulletManager)->Initialize();
	}
	if (targets.cameraManager) {
		*targets.cameraManager = std::make_unique<GameCameraManager>();
	}
}

void GamePlaySceneResourceInitializer::PrimeStageStart(
	Player* player, Camera* camera, std::list<std::unique_ptr<Obstacle>>& obstacles) {
	// 障害物・地形のコライダーを全件有効化して更新（PDF 不具合A-1, A-2対策）
	for (auto& obstacle : obstacles) {
		if (obstacle) {
			obstacle->SetCollisionEnabled(true);
			obstacle->Update();
		}
	}

	// プレイヤーの移動制御フラグと推力・コライダー復帰（PDF 不具合B-1, A-3対策）
	if (player) {
		player->SetSpecialAttackActive(false);
		player->ChangeMode(PlayerMode::Fighter);

		// ファイター形態の前進基本推力を直ちに付与し、前進停止を防止
		const Vector3 forward = player->GetForwardVector();
		const float baseSpeed = player->GetModeParams(PlayerMode::Fighter).maxMoveSpeed * 0.8f;
		player->SetVelocity({ forward.x * baseSpeed, forward.y * baseSpeed, forward.z * baseSpeed });

		// カメラを自機位置に即座に同期（PDF 不具合B-3対策）
		if (camera) {
			player->UpdateCamera(camera, nullptr, false);
			camera->Update();
		}
	}

	EditorReceiver::GetInstance()->Initialize();
}
