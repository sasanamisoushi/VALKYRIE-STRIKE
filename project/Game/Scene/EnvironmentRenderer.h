#pragma once

#include "3D/Model.h"
#include "3D/primitive.h"
#include "3D/Skybox.h"
#include "3D/Trail.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/Particle/ParticleEmitter.h"
#include <memory>

class EnvironmentRenderer {
public:
	friend class GamePlayUIManager;

	EnvironmentRenderer();
	~EnvironmentRenderer() = default;

	void Initialize();
	// advanceEffects を false にすると描画用のカメラ行列のみ更新し、環境演出は停止する。
	void Update(Camera* camera, bool advanceEffects = true);
	void Draw();
	// 不透明オブジェクトと半透明エフェクトを描く前に呼ぶ。
	void DrawBackground();

	Skybox* GetSkybox() const { return skybox_.get(); }
	bool GetShowSkybox() const { return showSkybox_; }
	bool GetShowParticles() const { return showParticles_; }

	ParticleManager* GetParticleManager() const { return particleManager_.get(); }
	ParticleEmitter* GetParticleEmitter() const { return particleEmitter_.get(); }

	Trail* GetMissileTrail() const { return missileTrail_.get(); }

private:
	std::unique_ptr<Skybox> skybox_;

	std::unique_ptr<ParticleManager> particleManager_;
	std::unique_ptr<ParticleEmitter> particleEmitter_;

	std::unique_ptr<Primitive> boundaryAlertPlane_;
	std::unique_ptr<Primitive> myPlane_;
	std::unique_ptr<Primitive> myShere_;
	std::unique_ptr<Primitive> myBox_;
	std::unique_ptr<Primitive> myRing_;
	std::unique_ptr<Primitive> myPartialRing_;
	std::unique_ptr<Primitive> myCylinder_;

	bool showNormalRing_ = false;
	bool showPartialRing_ = false;
	bool showCylinder_ = false;
	bool showSkybox_ = true;
	bool showParticles_ = true;

	// Partial Ring用パラメータ
	int prSubdivision_ = 64;
	float prOuterRadius_ = 1.2f;
	float prInnerRadius_ = 0.4f;
	bool prIsUvHorizontal_ = false;
	float prInnerColor_[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
	float prOuterColor_[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
	float prStartAngle_ = 0.0f;
	float prEndAngle_ = 180.0f;
	float prFadeAngle_ = 30.0f;

	// Cylinder用パラメータ
	float cylinderPos_[3] = { 0.0f, 0.0f, 0.0f };
	float cylinderScale_[3] = { 1.0f, 1.0f, 1.0f };
	float cylinderUVOffset_[2] = { 0.0f, 0.0f };
	float cylinderUVScrollSpeed_[2] = { 0.01f, 0.0f };
	float cylinderAlphaReference_ = 0.0f;

	int cylinderSubdivision_ = 32;
	int cylinderVerticalSubdivision_ = 1;
	float cylinderTopRadiusX_ = 1.0f;
	float cylinderTopRadiusZ_ = 1.0f;
	float cylinderBottomRadiusX_ = 1.0f;
	float cylinderBottomRadiusZ_ = 1.0f;
	float cylinderHeight_ = 3.0f;
	float cylinderTopColor_[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float cylinderBottomColor_[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float cylinderStartAngle_ = 0.0f;
	float cylinderEndAngle_ = 360.0f;
	bool cylinderIsUvFlipped_ = false;

	std::unique_ptr<Trail> missileTrail_;
	std::unique_ptr<Object3d> trailObject_;
};
