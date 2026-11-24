#include "BossClearCrashState.h"
void BossClearCrashState::Reset() { initialRotation_ = {}; velocity_ = {}; }
void BossClearCrashState::Start(const Vector3& initialRotation, const Vector3& velocity) { initialRotation_ = initialRotation; velocity_ = velocity; }
