#include "EnvironmentRenderer.h"
#include "engine/Graphics/DirectXCommon.h"
#include "3D/Object3dCommon.h"
#include "engine/math/MyMath.h"
#include <cmath>

EnvironmentRenderer::EnvironmentRenderer() {
}

void EnvironmentRenderer::Initialize() {
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize("resources/SkyBox.dds");

	particleManager_ = std::make_unique<ParticleManager>();
	particleManager_->Initialize(DirectXCommon::GetInstance());

	particleEmitter_ = std::make_unique<ParticleEmitter>("emitter1", Vector3{0.0f, 0.0f, 0.0f}, particleManager_.get());

	missileTrail_ = std::make_unique<Trail>();
	missileTrail_->Initialize(60);
	
	trailObject_ = std::make_unique<Object3d>();
	trailObject_->Initialize(Object3dCommon::GetInstance());

	myRing_ = std::make_unique<Primitive>();
	myRing_->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Ring);
	myRing_->SetTranslate({ 0.0f, 0.0f, 0.0f });

	myPartialRing_ = std::make_unique<Primitive>();
	myPartialRing_->Initialize(Object3dCommon::GetInstance(), PrimitiveType::PartialRing);
	myPartialRing_->SetTranslate({ 0.0f, 0.0f, 0.0f });

	myCylinder_ = std::make_unique<Primitive>();
	myCylinder_->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Cylinder);
	myCylinder_->SetTranslate({ 0.0f, 0.0f, 0.0f });
	myCylinder_->SetScale({ 2.0f, 2.0f, 2.0f });

	boundaryAlertPlane_ = std::make_unique<Primitive>();
	boundaryAlertPlane_->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Plane);
}

void EnvironmentRenderer::Update(Camera* camera, bool advanceEffects) {
if (skybox_ && showSkybox_) {
		skybox_->Update(camera);
	}

	if (myRing_ && showNormalRing_) {
		static float ringTime = 0.0f;
		if (advanceEffects) {
			ringTime += 0.05f;
		}

		myRing_->SetRotate({ 0.0f, 0.0f, 0.0f });
		myRing_->SetScale({ 2.0f, 2.0f, 1.0f });

		Model* ringModel = myRing_->GetModel();
		if (ringModel) {
			Vector3 uvScale = { 10.0f, 1.0f, 1.0f }; 
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			Vector3 uvTranslate = { ringTime * 0.1f, 0.0f, 0.0f }; 
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			ringModel->SetUvTransform(uvTransform);
		}
		myRing_->Update();
	}

	if (myPartialRing_ && showPartialRing_) {
		static float pRingTime = 0.0f;
		if (advanceEffects) {
			pRingTime += 0.05f;
		}

		myPartialRing_->SetRotate({ 0.0f, 0.0f, pRingTime * -0.5f });
		myPartialRing_->SetScale({ 2.0f, 2.0f, 1.0f });

		Model* pRingModel = myPartialRing_->GetModel();
		if (pRingModel) {
			Vector3 uvScale = { 1.0f, 10.0f, 1.0f };
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			Vector3 uvTranslate = { 0.0f, pRingTime * 0.1f, 0.0f }; 
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			pRingModel->SetUvTransform(uvTransform);
		}
		myPartialRing_->Update();
	}

	if (myCylinder_ && showCylinder_) {
		if (advanceEffects) {
			cylinderUVOffset_[0] += cylinderUVScrollSpeed_[0];
			cylinderUVOffset_[1] += cylinderUVScrollSpeed_[1];
		}

		Model* cModel = myCylinder_->GetModel();
		if (cModel) {
			Vector3 uvScale = { 1.0f, 1.0f, 1.0f };
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			Vector3 uvTranslate = { cylinderUVOffset_[0], cylinderUVOffset_[1], 0.0f };
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			cModel->SetUvTransform(uvTransform);
			cModel->SetAlphaReference(cylinderAlphaReference_);
		}
		
		myCylinder_->SetTranslate({ cylinderPos_[0], cylinderPos_[1], cylinderPos_[2] });
		myCylinder_->SetScale({ cylinderScale_[0], cylinderScale_[1], cylinderScale_[2] });

		myCylinder_->Update();
	}

	if (advanceEffects) {
		particleManager_->Update(camera);
		particleEmitter_->Update();
	}
}

void EnvironmentRenderer::DrawBackground() {
if (skybox_ && showSkybox_) {
		skybox_->Draw();
	}
}

void EnvironmentRenderer::Draw() {
	
	if (missileTrail_ && trailObject_) {
		// trailObject draws the trail
	}

	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (myRing_ && showNormalRing_) myRing_->Draw();
	if (myPartialRing_ && showPartialRing_) myPartialRing_->Draw();
	if (myCylinder_ && showCylinder_) myCylinder_->Draw();
	// Object3dCommon::GetInstance()->SetCommonDrawSettings();
if (particleManager_ && showParticles_) {
		particleManager_->Draw();
	}
}
