#include "BossClearFocusState.h"

#include <algorithm>

void BossClearFocusState::Reset() {
	position_ = {};
	radius_ = 1.0f;
}

void BossClearFocusState::Set(const Vector3& position, float radius) {
	position_ = position;
	radius_ = (std::max)(1.0f, radius);
}
