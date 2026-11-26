#include "BossTurretController.h"

#include "Boss.h"
#include "Game/enemy/EnemyBulletManager.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr Vector3 kBossScale = { 1.85f, 1.55f, 1.85f };

Vector3 AddVector(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 SubtractVector(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vector3 ScaleVector(const Vector3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }
float DotVector(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vector3 NormalizeVector(const Vector3& value) {
	const float length = std::sqrt(DotVector(value, value));
	return length > 0.0001f ? ScaleVector(value, 1.0f / length) : Vector3{ 0.0f, 0.0f, 1.0f };
}

Quaternion MakeEulerQuaternion(const Vector3& euler) {
	const Quaternion qx = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, euler.x);
	const Quaternion qy = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, euler.y);
	const Quaternion qz = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, euler.z);
	return MyMath::Normalize(MyMath::Multiply(MyMath::Multiply(qx, qy), qz));
}

// 行ベクトル規約 (v * R(a * b) = v * R(b) * R(a)) に合わせ、
// 砲身ローカルのピッチ回転を先に適用し、その後にヨー回転を適用する。
Quaternion MakePitchYawQuaternion(float pitch, float yaw) {
	const Quaternion qx = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
	const Quaternion qy = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
	return MyMath::Normalize(MyMath::Multiply(qy, qx));
}

bool IsMuzzleFacingAwayFromPivot(const Vector3& pivotPosition, const Vector3& muzzlePosition,
	const Vector3& barrelDirection) {
	const Vector3 outward = NormalizeVector(SubtractVector(muzzlePosition, pivotPosition));
	return DotVector(barrelDirection, outward) > 0.10f;
}

float WrapAngle(float angle) {
	constexpr float kPi = 3.14159265359f;
	while (angle > kPi) angle -= kPi * 2.0f;
	while (angle < -kPi) angle += kPi * 2.0f;
	return angle;
}

Vector3 RotatePitchYaw(const Vector3& value, float pitch, float yaw) {
	const float pitchCos = std::cos(pitch);
	const float pitchSin = std::sin(pitch);
	const Vector3 pitched = {
		value.x,
		value.y * pitchCos - value.z * pitchSin,
		value.y * pitchSin + value.z * pitchCos,
	};
	const float yawCos = std::cos(yaw);
	const float yawSin = std::sin(yaw);
	return {
		pitched.x * yawCos + pitched.z * yawSin,
		pitched.y,
		-pitched.x * yawSin + pitched.z * yawCos,
	};
}

struct WeaponMount {
	Vector3 pivot;
	std::array<Vector3, 2> muzzles;
	std::array<Vector3, 2> barrelAxes = { Vector3{ 0.0f, 0.0f, 1.0f }, Vector3{ 0.0f, 0.0f, 1.0f } };
	float modelPitchOffset = 0.0f;
};

constexpr std::array<WeaponMount, 4> kMainWeaponMounts = {
	WeaponMount{ Vector3{ -1.73f, 3.85f, -2.62f }, std::array<Vector3, 2>{ Vector3{ -1.41f, 4.30f, -0.48f }, Vector3{ -2.10f, 4.30f, -0.48f } } },
	WeaponMount{ Vector3{  1.83f, 3.85f, -2.62f }, std::array<Vector3, 2>{ Vector3{  1.47f, 4.30f, -0.48f }, Vector3{  2.15f, 4.30f, -0.48f } } },
	WeaponMount{ Vector3{  0.04f, 2.38f,  2.26f }, std::array<Vector3, 2>{ Vector3{ -0.38f, 2.07f,  4.60f }, Vector3{  0.31f, 2.07f,  4.60f } } },
	WeaponMount{ Vector3{  0.02f, 4.45f, -0.76f },
		std::array<Vector3, 2>{ Vector3{ -0.89f, 4.89f, -0.78f }, Vector3{  0.93f, 4.89f, -0.78f } },
		std::array<Vector3, 2>{ Vector3{ -0.634f, 0.773f, 0.0f }, Vector3{ 0.634f, 0.773f, 0.0f } } },
};
constexpr std::array<Vector3, 2> kMiniTurretBarrelPivots = {
	Vector3{ -0.72f, 4.34f, -0.78f }, Vector3{ 0.70f, 4.34f, -0.78f },
};
constexpr std::array<Vector3, 2> kMiniTurretBasePivots = {
	Vector3{ -0.59f, 4.30f, -0.76f }, Vector3{ 0.58f, 4.30f, -0.76f },
};
constexpr std::array<float, 3> kMainTurretBaseYaws = { 0.0f, 0.0f, 0.0f };
constexpr std::array<float, 3> kMainTurretYawMinimums = {
	-1.57079632679f, -1.57079632679f, -1.57079632679f,
};
constexpr std::array<float, 3> kMainTurretYawMaximums = {
	1.57079632679f, 1.57079632679f, 1.57079632679f,
};
constexpr std::array<float, 2> kMiniTurretBaseYaws = { 0.0f, 0.0f };
constexpr WeaponMount kMachineGunMount = WeaponMount{
	Vector3{ 0.01f, 4.70f, 0.90f }, std::array<Vector3, 2>{ Vector3{ 0.29f, 5.22f, 1.78f }, Vector3{ -0.28f, 5.22f, 1.78f } }
};

struct ForwardWeaponMount {
	Vector3 pivot;
	Vector3 muzzle;
	float hitRadius;
};

constexpr std::array<ForwardWeaponMount, 3> kForwardWeaponMounts = {
	ForwardWeaponMount{ Vector3{ 0.02f, 4.42f, 3.23f }, Vector3{ 0.02f, 4.55f, 4.20f }, 1.00f },
	ForwardWeaponMount{ Vector3{ -1.31f, 3.28f, 3.57f }, Vector3{ -1.31f, 3.28f, 4.92f }, 0.82f },
	ForwardWeaponMount{ Vector3{ 1.33f, 3.28f, 3.57f }, Vector3{ 1.33f, 3.28f, 4.92f }, 0.82f },
};

constexpr size_t kForwardTurretFirst = 0;
constexpr size_t kMainTurretFirst = kForwardTurretFirst + kForwardWeaponMounts.size();
constexpr size_t kMiniTurretFirst = kMainTurretFirst + kMainWeaponMounts.size() - 1;
constexpr size_t kMachineGunTurretIndex = kMiniTurretFirst + kMiniTurretBarrelPivots.size();
}

void BossTurretController::FireMachineGun(const Boss& boss, const Vector3& playerPos, EnemyBulletManager* bulletManager) const {
	if (!bulletManager || !boss.IsTurretAlive(kMachineGunTurretIndex)) return;
	(void)playerPos;
	const float pitch = kMachineGunMount.modelPitchOffset;
	const Vector3 pivotPosition = boss.LocalPointToWorld(kMachineGunMount.pivot);
	for (const Vector3& localMuzzle : kMachineGunMount.muzzles) {
		const Vector3 rotatedMuzzle = AddVector(kMachineGunMount.pivot,
			RotatePitchYaw(SubtractVector(localMuzzle, kMachineGunMount.pivot), pitch, boss.turretYaw_));
		const Vector3 muzzlePosition = AddVector(boss.position_, RotatePitchYaw({
			rotatedMuzzle.x * kBossScale.x, rotatedMuzzle.y * kBossScale.y, rotatedMuzzle.z * kBossScale.z
		}, 0.0f, boss.rotation_.y));
		const Vector3 localDirection = RotatePitchYaw(kMachineGunMount.barrelAxes[0], pitch, boss.turretYaw_);
		const Vector3 direction = RotatePitchYaw(localDirection, 0.0f, boss.rotation_.y);
		if (!IsMuzzleFacingAwayFromPivot(pivotPosition, muzzlePosition, direction)) continue;
		bulletManager->Shoot(muzzlePosition, ScaleVector(direction, 0.70f));
	}
}

void BossTurretController::FireMainBeam(const Boss& boss, const Vector3& playerPos, EnemyBulletManager* bulletManager, size_t turretIndex) const {
	if (!bulletManager || turretIndex >= kMainWeaponMounts.size()) return;
	if (turretIndex < boss.mainTurrets_.size() && !boss.IsTurretAlive(kMainTurretFirst + turretIndex)) return;
	(void)playerPos;
	const WeaponMount& mount = kMainWeaponMounts[turretIndex];
	// 主砲はヨーだけ。上部ミニ連装砲は台座ヨー + 砲身ピッチの二段旋回。
	for (size_t muzzleIndex = 0; muzzleIndex < mount.muzzles.size(); ++muzzleIndex) {
		if (turretIndex == boss.mainTurrets_.size() && !boss.IsTurretAlive(kMiniTurretFirst + muzzleIndex)) continue;
		const Vector3& localMuzzle = mount.muzzles[muzzleIndex];
		Vector3 rotatedMuzzle{};
		Vector3 pivotPosition{};
		float pitch = mount.modelPitchOffset;
		float yaw = turretIndex < boss.mainTurretYaws_.size()
			? kMainTurretBaseYaws[turretIndex] + boss.mainTurretYaws_[turretIndex]
			: boss.turretYaw_;
		if (turretIndex == 3) {
			pitch = boss.miniTurretPitches_[muzzleIndex];
			yaw = kMiniTurretBaseYaws[muzzleIndex] + boss.miniTurretYaws_[muzzleIndex];
			const Vector3& barrelPivot = kMiniTurretBarrelPivots[muzzleIndex];
			const Vector3& basePivot = kMiniTurretBasePivots[muzzleIndex];
			// 台座は横旋回のみ、砲身は自身のヒンジを中心に上下旋回する。
			const Vector3 rotatedBarrelPivot = AddVector(basePivot,
				RotatePitchYaw(SubtractVector(barrelPivot, basePivot), 0.0f, yaw));
			rotatedMuzzle = AddVector(rotatedBarrelPivot,
				RotatePitchYaw(SubtractVector(localMuzzle, barrelPivot), pitch, yaw));
			pivotPosition = AddVector(boss.position_, RotatePitchYaw({
				rotatedBarrelPivot.x * kBossScale.x, rotatedBarrelPivot.y * kBossScale.y,
				rotatedBarrelPivot.z * kBossScale.z
			}, 0.0f, boss.rotation_.y));
		} else {
			rotatedMuzzle = AddVector(mount.pivot,
				RotatePitchYaw(SubtractVector(localMuzzle, mount.pivot), pitch, yaw));
			pivotPosition = boss.LocalPointToWorld(mount.pivot);
		}
		const Vector3 muzzlePosition = AddVector(boss.position_, RotatePitchYaw({
			rotatedMuzzle.x * kBossScale.x, rotatedMuzzle.y * kBossScale.y, rotatedMuzzle.z * kBossScale.z
		}, 0.0f, boss.rotation_.y));
		const Vector3 localDirection = RotatePitchYaw(mount.barrelAxes[muzzleIndex], pitch, yaw);
		const Vector3 direction = RotatePitchYaw(localDirection, 0.0f, boss.rotation_.y);
		if (!IsMuzzleFacingAwayFromPivot(pivotPosition, muzzlePosition, direction)) continue;
		if (turretIndex == 3) {
			bulletManager->Shoot(muzzlePosition, ScaleVector(direction, 0.88f));
		} else {
			// 巨大なビーム用の板ポリゴンはスラスターと見分けが付きにくく、
			// 停止中に艦体から離れたように見えるため、主砲も通常弾として描画する。
			bulletManager->Shoot(muzzlePosition, ScaleVector(direction, 0.92f));
		}
	}
}

void BossTurretController::FireForwardTurret(const Boss& boss, const Vector3& playerPos, EnemyBulletManager* bulletManager, size_t turretIndex) const {
	if (!bulletManager || turretIndex >= kForwardWeaponMounts.size() ||
		!boss.IsTurretAlive(kForwardTurretFirst + turretIndex)) return;
	(void)playerPos;

	const ForwardWeaponMount& mount = kForwardWeaponMounts[turretIndex];
	// 左右砲台（1, 2）は艦体に対する横向きを固定し、上下だけを回転する。
	// 中央砲台（0）は横回転だけで追尾する。
	const float yaw = turretIndex > 0 ? 0.0f : boss.turretYaw_;
	const float pitch = turretIndex > 0 ? boss.turretPitch_ : 0.0f;
	const Vector3 rotatedMuzzle = AddVector(mount.pivot,
		RotatePitchYaw(SubtractVector(mount.muzzle, mount.pivot), pitch, yaw));
	const Vector3 muzzlePosition = boss.LocalPointToWorld(rotatedMuzzle);
	const Vector3 pivotPosition = boss.LocalPointToWorld(mount.pivot);
	const Vector3 localDirection = RotatePitchYaw({ 0.0f, 0.0f, 1.0f }, pitch, yaw);
	const Vector3 direction = RotatePitchYaw(localDirection, 0.0f, boss.rotation_.y);
	if (!IsMuzzleFacingAwayFromPivot(pivotPosition, muzzlePosition, direction)) return;
	bulletManager->Shoot(muzzlePosition, ScaleVector(direction, 0.70f));
}

Vector3 BossTurretController::GetTurretPivot(size_t turretIndex) const {
	if (turretIndex < kForwardWeaponMounts.size()) return kForwardWeaponMounts[turretIndex].pivot;
	if (turretIndex < kMiniTurretFirst) return kMainWeaponMounts[turretIndex - kMainTurretFirst].pivot;
	if (turretIndex < kMachineGunTurretIndex) return kMiniTurretBasePivots[turretIndex - kMiniTurretFirst];
	if (turretIndex == kMachineGunTurretIndex) return kMachineGunMount.pivot;
	return { 0.0f, 0.0f, 0.0f };
}

float BossTurretController::GetTurretHitRadius(size_t turretIndex) const {
	if (turretIndex < kForwardWeaponMounts.size()) return kForwardWeaponMounts[turretIndex].hitRadius;
	if (turretIndex < kMiniTurretFirst) return 0.95f;
	if (turretIndex < kMachineGunTurretIndex) return 0.58f;
	if (turretIndex == kMachineGunTurretIndex) return 0.72f;
	return 0.0f;
}

void BossTurretController::UpdateAim(Boss& boss, const Vector3& playerPos) const {
	const Vector3 direction = boss.DirectionTo(playerPos);
	const float horizontalLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	if (horizontalLength <= 0.0001f) return;

	const float requestedYaw = boss.tuning_.mainTurretYawTracking
		? WrapAngle(std::atan2(direction.x, direction.z) - boss.rotation_.y) : 0.0f;
	const float requestedPitch = boss.tuning_.mainTurretPitchTracking
		? std::clamp(-std::atan2(direction.y, horizontalLength), -boss.tuning_.mainTurretMaxPitch, boss.tuning_.mainTurretMaxPitch) : 0.0f;
	constexpr float kQuarterPi = 0.78539816339f;
	// ミニ連装砲は初期姿勢が上向き。+90度を基準にすることで砲身を前方へ倒し、
	// プレイヤーが下にいる場合もさらに下向きまで追尾できる。
	constexpr float kIgelForwardPitch = 1.57079632679f;
	constexpr float kIgelPitchMin = 0.45f;
	constexpr float kIgelPitchMax = 2.45f;
	// 左右の大型砲は担当する舷側と正面境界で反応する。
	// 正面では左右・中央の3基が揃って発射し、右側では右砲だけ、左側では左砲だけが追尾する。
	for (size_t index = 0; index < boss.mainTurretYaws_.size(); ++index) {
		const bool handlesTarget = index == 0 ? requestedYaw <= 0.08f
			: index == 1 ? requestedYaw >= -0.08f
			: std::abs(requestedYaw) <= 0.62f;
		boss.mainTurretEngaged_[index] = handlesTarget;
		const float desiredRelativeYaw = handlesTarget
			? std::clamp(WrapAngle(requestedYaw - kMainTurretBaseYaws[index]),
				kMainTurretYawMinimums[index], kMainTurretYawMaximums[index])
			: 0.0f;
		boss.mainTurretYaws_[index] += WrapAngle(desiredRelativeYaw - boss.mainTurretYaws_[index]) *
			boss.tuning_.mainTurretYawFollow;
	}
	// 既存の中央機銃・艦首中央砲は前方半円に限定する。
	const float desiredYaw = std::clamp(requestedYaw, -1.57079632679f, 1.57079632679f);
	boss.turretYaw_ += WrapAngle(desiredYaw - boss.turretYaw_) * boss.tuning_.mainTurretYawFollow;
	boss.turretPitch_ += (requestedPitch - boss.turretPitch_) * boss.tuning_.mainTurretPitchFollow;

	// 上部ミニ連装砲は戦艦の機関砲と同じく、台座が横旋回・砲身が上下旋回する。
	const float igelYawFollow = std::clamp(boss.tuning_.mainTurretYawFollow * 3.0f, 0.16f, 0.48f);
	const float igelPitchFollow = std::clamp(boss.tuning_.mainTurretPitchFollow * 3.0f, 0.16f, 0.48f);
	for (size_t index = 0; index < boss.miniTurretYaws_.size(); ++index) {
		// 各台座自身からプレイヤーを測る。ボス中心を共用しないため、近距離でも左右が同期しない。
		const Vector3 miniPosition = boss.LocalPointToWorld(kMiniTurretBasePivots[index]);
		const Vector3 toPlayer = SubtractVector(playerPos, miniPosition);
		const float miniHorizontalLength = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
		if (miniHorizontalLength <= 0.0001f) continue;
		const float miniRequestedYaw = WrapAngle(std::atan2(toPlayer.x, toPlayer.z) - boss.rotation_.y);
		const float miniRequestedPitch = std::clamp(-std::atan2(toPlayer.y, miniHorizontalLength),
			-boss.tuning_.mainTurretMaxPitch, boss.tuning_.mainTurretMaxPitch);
		// 左右の副砲を±90度へ配置し、それぞれ45度ずつの扇形を受け持たせる。
		const float desiredRelativeYaw = std::clamp(
			WrapAngle(miniRequestedYaw - kMiniTurretBaseYaws[index]), -kQuarterPi, kQuarterPi);
		boss.miniTurretYaws_[index] += WrapAngle(desiredRelativeYaw - boss.miniTurretYaws_[index]) * igelYawFollow;
		const float desiredPitch = std::clamp(kIgelForwardPitch + miniRequestedPitch,
			kIgelPitchMin, kIgelPitchMax);
		boss.miniTurretPitches_[index] += (desiredPitch - boss.miniTurretPitches_[index]) * igelPitchFollow;
	}
}

void BossTurretController::UpdateModels(Boss& boss) const {
	if (boss.cinematicDebrisActive_) {
		return;
	}
	const Quaternion hullQuat = MakeEulerQuaternion(boss.rotation_);
	auto updateTurret = [&boss, &hullQuat](Object3d* turret, const Vector3& pivot, float pitch, float yaw,
		float modelPitchOffset = 0.0f) {
		if (!turret) return;
		turret->SetTranslate(boss.LocalPointToWorld(pivot));
		const Quaternion localQuat = MakePitchYawQuaternion(pitch + modelPitchOffset, yaw);
		// 行ベクトル規約では右辺が先に適用されるため、砲台ローカル回転の後に船体回転を掛ける。
		turret->SetQuaternionRotate(MyMath::Normalize(MyMath::Multiply(hullQuat, localQuat)));
		turret->SetScale(kBossScale);
		turret->Update();
	};

	for (size_t index = 0; index < boss.mainTurrets_.size(); ++index) {
		if (boss.IsTurretAlive(kMainTurretFirst + index)) {
			updateTurret(boss.mainTurrets_[index].get(), kMainWeaponMounts[index].pivot,
				0.0f, kMainTurretBaseYaws[index] + boss.mainTurretYaws_[index],
				kMainWeaponMounts[index].modelPitchOffset);
		}
	}
	// ミニ砲台: 箱形の台座だけを横旋回する。
	for (size_t index = 0; index < boss.miniTurretBases_.size(); ++index) {
		if (boss.IsTurretAlive(kMiniTurretFirst + index)) {
			updateTurret(boss.miniTurretBases_[index].get(), kMiniTurretBasePivots[index], 0.0f,
				kMiniTurretBaseYaws[index] + boss.miniTurretYaws_[index]);
		}
	}
	// ミニ砲身: 台座の横旋回に追従しつつ、各自のヒンジで上下へ動く。
	for (size_t index = 0; index < boss.miniTurretBarrels_.size(); ++index) {
		if (!boss.IsTurretAlive(kMiniTurretFirst + index)) continue;
		Object3d* barrel = boss.miniTurretBarrels_[index].get();
		if (!barrel) continue;
		const Vector3& barrelPivot = kMiniTurretBarrelPivots[index];
		const Vector3& basePivot = kMiniTurretBasePivots[index];
		const float yaw = kMiniTurretBaseYaws[index] + boss.miniTurretYaws_[index];
		const Vector3 rotatedBarrelPivot = AddVector(basePivot,
			RotatePitchYaw(SubtractVector(barrelPivot, basePivot), 0.0f, yaw));
		barrel->SetTranslate(boss.LocalPointToWorld(rotatedBarrelPivot));
		const Quaternion localQuat = MakePitchYawQuaternion(boss.miniTurretPitches_[index], yaw);
		barrel->SetQuaternionRotate(MyMath::Normalize(MyMath::Multiply(hullQuat, localQuat)));
		barrel->SetScale(kBossScale);
		barrel->Update();
	}
	// 通常の機銃も横回転だけ。上部ミニ連装砲とは別の砲台として扱う。
	if (boss.IsTurretAlive(kMachineGunTurretIndex)) {
		updateTurret(boss.machineGunTurret_.get(), kMachineGunMount.pivot, 0.0f, boss.turretYaw_,
			kMachineGunMount.modelPitchOffset);
	}
	for (size_t index = 0; index < boss.forwardTurrets_.size(); ++index) {
		if (boss.IsTurretAlive(kForwardTurretFirst + index)) {
			// 左右砲台だけはヨーを固定し、ピッチ回転だけにする。
			const float yaw = index > 0 ? 0.0f : boss.turretYaw_;
			const float pitch = index > 0 ? boss.turretPitch_ : 0.0f;
			updateTurret(boss.forwardTurrets_[index].get(), kForwardWeaponMounts[index].pivot, pitch, yaw);
		}
	}
}
