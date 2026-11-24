#include "BossClearPlayerExitState.h"
void BossClearPlayerExitState::Reset() { position_ = {}; direction_ = {0.0f, 0.0f, 1.0f}; speed_ = 0.0f; roll_ = 0.0f; }
void BossClearPlayerExitState::Set(const Vector3& position, const Vector3& direction, float speed, float roll) { position_ = position; direction_ = direction; speed_ = speed; roll_ = roll; }
