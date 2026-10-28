#include "Missile.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/Enemy.h"
#include "3D/ModelManager.h"
#include "engine/Particle/ParticleManager.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
	constexpr bool kEnableMissileTrails = true;
	constexpr int kHomingForwardLaunchFrames = 10; // 30から10に短縮して、すぐ誘導開始する

	float LengthSq(const Vector3 &value) {
		return value.x * value.x + value.y * value.y + value.z * value.z;
	}

	float Length(const Vector3 &value) {
		return std::sqrt(LengthSq(value));
	}

	Vector3 Add(const Vector3 &a, const Vector3 &b) {
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	Vector3 Subtract(const Vector3 &a, const Vector3 &b) {
		return { a.x - b.x, a.y - b.y, a.z - b.z };
	}

	Vector3 Scale(const Vector3 &value, float scalar) {
		return { value.x * scalar, value.y * scalar, value.z * scalar };
	}

	Vector3 NormalizeOr(const Vector3 &value, const Vector3 &fallback) {
		const float length = Length(value);
		if (length <= 0.0001f) {
			return fallback;
		}
		return Scale(value, 1.0f / length);
	}

	Vector3 BlendDirection(const Vector3 &from, const Vector3 &to, float blend) {
		return NormalizeOr(Add(Scale(from, 1.0f - blend), Scale(to, blend)), to);
	}

}

void Missile::Initialize(const Vector3 &position, const Vector3 &velocity, MissileType type, const MissileTuning &tuning, ParticleManager* pManager, Enemy* target) {
	type_ = type;
	tuning_ = tuning;
	particleManager_ = pManager;
	target_ = target;

	object_ = std::make_unique<Object3d>();
	object_->Initialize(Object3dCommon::GetInstance());
	object_->SetModel("Sphere");

	if (type_ == MissileType::Normal) {
		if (object_->GetModel()) object_->GetModel()->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f });
	} else {
		if (object_->GetModel()) object_->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
	}

	const float visualScale = (std::max)(0.01f, tuning_.scale);
	object_->SetScale({ visualScale, visualScale, visualScale });
	collisionRadius_ = (std::max)(0.01f, tuning_.collisionRadius);

	position_ = position;
	velocity_ = velocity;

	if (type_ == MissileType::MissileWithTrail) {
		Vector3 forward = NormalizeOr(velocity_, { 0.0f, 0.0f, 1.0f });
		if (target_ && !target_->IsDead()) {
			Vector3 toTarget = Subtract(target_->GetPosition(), position_);
			forward = NormalizeOr(toTarget, forward);
			const float speed = Length(velocity_);
			velocity_ = Scale(forward, speed > 0.001f ? speed : (std::max)(0.01f, tuning_.speed));
		}
		Vector3 right = NormalizeOr(MyMath::Cross(forward, { 0.0f, 1.0f, 0.0f }), { 1.0f, 0.0f, 0.0f });
		Vector3 up = NormalizeOr(MyMath::Cross(right, forward), { 0.0f, 1.0f, 0.0f });
		float spreadX = ((float)(rand() % 100) / 100.0f - 0.5f) * 2.0f;
		float spreadY = ((float)(rand() % 100) / 100.0f - 0.5f) * 2.0f;
		velocity_ = Add(velocity_, Scale(right, spreadX * 0.2f));
		velocity_ = Add(velocity_, Scale(up, spreadY * 0.2f));
	}

	lifeTimer_ = (std::max)(1, tuning_.lifeTime);
	elapsedFrames_ = 0;
	isDead_ = false;

	phaseOffset_ = (float)(rand() % 360) * 0.0174532925f;
	waveSign_ = (rand() % 2 == 0) ? 1.0f : -1.0f;
	spiralSpeed_ = 0.12f + (float)(rand() % 8) * 0.01f;

	// トレイルの初期化処理は削除しました（パーティクルを使用）
}

void Missile::Update(Camera *camera) {
	if (isDead_) return;

	if (type_ == MissileType::Normal && target_) {
		if (target_->IsDead()) {
			target_ = nullptr;
		} else {
			Vector3 targetPos = target_->GetPosition();
			targetPos.y += target_->GetCollisionRadius() * 0.3f;
			const Vector3 toTarget = Subtract(targetPos, position_);
			const float distanceToTarget = Length(toTarget);
			const Vector3 desiredDirection = NormalizeOr(toTarget, NormalizeOr(velocity_, { 0.0f, 0.0f, 1.0f }));
			const Vector3 currentDirection = NormalizeOr(velocity_, desiredDirection);

			// 通常弾は直進らしさを残しつつ、接近時だけ曲がりを少し強めて
			// ロックオン中の移動目標を取り逃がさないようにする。
			float homingStrength = std::clamp(tuning_.homingStrength, 0.0f, 1.0f);
			if (distanceToTarget < 25.0f) {
				homingStrength = (std::max)(homingStrength, 0.55f);
			}
			const Vector3 homingDirection = BlendDirection(currentDirection, desiredDirection, homingStrength);
			const float speed = (std::max)(0.01f, Length(velocity_));
			velocity_ = Scale(homingDirection, speed);
		}
	}

	if (type_ == MissileType::MissileWithTrail && elapsedFrames_ >= kHomingForwardLaunchFrames) {
		Vector3 targetPos = position_;
		bool hasTarget = false;

		if (target_) {
			if (target_->IsDead()) {
				target_ = nullptr;
			} else {
				targetPos = target_->GetPosition();
				hasTarget = true;
			}
		}

		if (hasTarget) {
			Vector3 toTarget = Subtract(targetPos, position_);
			float distToTarget = Length(toTarget);
			Vector3 desiredDirection = NormalizeOr(toTarget, { 0.0f, 0.0f, 1.0f });
			Vector3 currentDirection = NormalizeOr(velocity_, desiredDirection);
			
			float actualHomingStrength = tuning_.homingStrength;
			if (distToTarget < 30.0f) {
				actualHomingStrength = (std::max)(actualHomingStrength, 1.0f - (distToTarget / 30.0f));
			}

			Vector3 homingDirection = BlendDirection(currentDirection, desiredDirection, std::clamp(actualHomingStrength, 0.0f, 1.0f));

			Vector3 rightVec = NormalizeOr(MyMath::Cross(homingDirection, { 0.0f, 1.0f, 0.0f }), { 1.0f, 0.0f, 0.0f });
			if (LengthSq(rightVec) < 0.001f) {
				rightVec = { 1.0f, 0.0f, 0.0f };
			}
			Vector3 upVec = NormalizeOr(MyMath::Cross(rightVec, homingDirection), { 0.0f, 1.0f, 0.0f });

			float time = (float)(tuning_.lifeTime - lifeTimer_);
			float theta = time * spiralSpeed_ + phaseOffset_;
			
			float fade = std::clamp(distToTarget / 40.0f, 0.0f, 1.0f);
			if (lifeTimer_ < 60) {
				fade *= (lifeTimer_ / 60.0f);
			}

			// 振幅が強すぎるとうねりが蓄積して一定の場所をループしてしまうため弱める
			float amplitude = 0.2f * fade; 
			Vector3 wave = Add(Scale(rightVec, std::cos(theta) * amplitude), Scale(upVec, std::sin(theta) * waveSign_ * amplitude));

			// 前進ベクトルを強めに合成して軌道の破綻を防ぐ
			Vector3 finalDirection = NormalizeOr(Add(Scale(homingDirection, 2.0f), wave), homingDirection);
			float homingSpeed = (std::max)(0.01f, tuning_.speed * 1.5f);
			velocity_ = Scale(finalDirection, homingSpeed);
		} else {
			Vector3 currentDirection = NormalizeOr(velocity_, { 0.0f, 0.0f, 1.0f });
			Vector3 rightVec = NormalizeOr(MyMath::Cross(currentDirection, { 0.0f, 1.0f, 0.0f }), { 1.0f, 0.0f, 0.0f });
			if (LengthSq(rightVec) < 0.001f) rightVec = { 1.0f, 0.0f, 0.0f };
			Vector3 upVec = NormalizeOr(MyMath::Cross(rightVec, currentDirection), { 0.0f, 1.0f, 0.0f });

			float time = (float)(tuning_.lifeTime - lifeTimer_);
			float theta = time * spiralSpeed_ + phaseOffset_;
			
			float amplitude = 0.1f;
			Vector3 wave = Add(Scale(rightVec, std::cos(theta) * amplitude), Scale(upVec, std::sin(theta) * waveSign_ * amplitude));
			
			Vector3 finalDirection = NormalizeOr(Add(Scale(currentDirection, 2.0f), wave), currentDirection);
			velocity_ = Scale(finalDirection, (std::max)(0.01f, tuning_.speed * 1.5f));
		}
	}
	position_.x += velocity_.x;
	position_.y += velocity_.y;
	position_.z += velocity_.z;
	elapsedFrames_++;

	lifeTimer_--;
	if (lifeTimer_ <= 0) {
		isDead_ = true;
	}

	UpdateModel();

	// トレイルの代わりに煙パーティクルを放出する
	if (kEnableMissileTrails && type_ == MissileType::MissileWithTrail && particleManager_) {
		// 煙っぽく見せるため、初めは少しオレンジで、すぐにグレーになるようなパーティクルなどを想定。
		// 加算ブレンドの影響を抑えるため、色は暗めのグレーにし、サイズを大きくして放出数を増やします。
		particleManager_->Emit(
			"smoke",            // ParticleGroup の名前
			position_,          // 発生座標
			3,                  // count (1から3に増やして煙を濃くする)
			{ 0.2f, 0.2f, 0.2f, 0.5f }, // 色（暗いグレーにしてキラキラ感を抑える）
			0.01f,              // speed (その場に留まらせるため遅く)
			0.02f,              // speedVariance
			1.2f,               // scale (初期サイズを大きくしてモクモク感を出す)
			0.3f,               // scaleVariance
			0.4f,               // lifeTimeMin
			0.7f,               // lifeTimeMax
			0.2f                // posVariance
		);
	}
}

void Missile::UpdateModel(Camera *camera) {
	if (object_) {
		object_->SetTranslate(position_);
		object_->Update();
	}
}

void Missile::Draw() {
	if (isDead_) return;

	if (object_) {
		object_->Draw();
	}
}

void Missile::OnCollision() {
	isDead_ = true;
}

void Missile::ResolveGroundContact(float terrainY, float clearance) {
	if (isDead_) {
		return;
	}

	const float minimumY = terrainY + (std::max)(clearance, collisionRadius_ + 0.05f);
	if (position_.y < minimumY) {
		position_.y = minimumY;
	}

	// 誘導が再び地面へ向いても次フレームで潜らないよう、速度を保ったまま
	// 下降成分を小さな上昇成分へ置き換える。
	if (velocity_.y < 0.05f) {
		const float speed = (std::max)(0.01f, Length(velocity_));
		velocity_.y = 0.05f;
		velocity_ = Scale(NormalizeOr(velocity_, { 0.0f, 1.0f, 0.0f }), speed);
	}

	UpdateModel(nullptr);
}
