#include "TutorialRequirementsManager.h"

#include "externals/json.hpp"

#include <algorithm>
#include <exception>
#include <fstream>

using json = nlohmann::json;

void TutorialRequirementsManager::Set(TutorialRequirements requirements) {
	requirements_ = requirements;
	ApplyLimits();
}

void TutorialRequirementsManager::Reset() {
	requirements_ = TutorialRequirements{};
}

void TutorialRequirementsManager::ApplyLimits() {
	requirements_.forward = std::clamp(requirements_.forward, 1, 20);
	requirements_.altitude = std::clamp(requirements_.altitude, 1, 20);
	requirements_.turn = std::clamp(requirements_.turn, 1, 20);
	requirements_.gerwalkHover = std::clamp(requirements_.gerwalkHover, 1, 20);
	requirements_.gerwalkStrafe = std::clamp(requirements_.gerwalkStrafe, 1, 20);
	requirements_.lockOn = std::clamp(requirements_.lockOn, 1, 20);
	requirements_.normalShots = std::clamp(requirements_.normalShots, 1, 20);
	requirements_.fighterTransform = std::clamp(requirements_.fighterTransform, 1, 20);
	requirements_.homingLaunches = std::clamp(requirements_.homingLaunches, 1, 20);
}

bool TutorialRequirementsManager::Load(const std::string& filePath) {
	std::ifstream input(filePath);
	if (!input.is_open()) {
		return false;
	}

	try {
		json root;
		input >> root;
		const json& source = root.contains("tutorial_requirements") ? root["tutorial_requirements"] : root;
		TutorialRequirements loaded = requirements_;
		auto readInt = [&source](const char* name, int& value) {
			if (source.contains(name) && source[name].is_number_integer()) {
				value = source[name].get<int>();
			}
		};
		readInt("forward", loaded.forward);
		readInt("altitude", loaded.altitude);
		readInt("turn", loaded.turn);
		readInt("gerwalk_hover", loaded.gerwalkHover);
		readInt("gerwalk_strafe", loaded.gerwalkStrafe);
		readInt("lock_on", loaded.lockOn);
		readInt("normal_shots", loaded.normalShots);
		readInt("fighter_transform", loaded.fighterTransform);
		readInt("homing_launches", loaded.homingLaunches);
		Set(loaded);
		return true;
	} catch (const std::exception&) {
		return false;
	}
}

bool TutorialRequirementsManager::Save(const std::string& filePath) const {
	std::ofstream output(filePath, std::ios::trunc);
	if (!output.is_open()) {
		return false;
	}

	const json root = { { "tutorial_requirements", {
		{ "forward", requirements_.forward },
		{ "altitude", requirements_.altitude },
		{ "turn", requirements_.turn },
		{ "gerwalk_hover", requirements_.gerwalkHover },
		{ "gerwalk_strafe", requirements_.gerwalkStrafe },
		{ "lock_on", requirements_.lockOn },
		{ "normal_shots", requirements_.normalShots },
		{ "fighter_transform", requirements_.fighterTransform },
		{ "homing_launches", requirements_.homingLaunches },
	} } };
	output << root.dump(2) << '\n';
	return output.good();
}
