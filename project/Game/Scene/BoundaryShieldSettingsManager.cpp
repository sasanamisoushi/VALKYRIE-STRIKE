#include "BoundaryShieldSettingsManager.h"

#include "Game/Player/Player.h"
#include "externals/json.hpp"

#include <algorithm>
#include <exception>
#include <fstream>

using json = nlohmann::json;

void BoundaryShieldSettingsManager::Set(BoundaryShieldSettings settings, Player* player) {
	settings_ = settings;
	Apply(player);
}

void BoundaryShieldSettingsManager::Reset(Player* player) {
	settings_ = BoundaryShieldSettings{};
	Apply(player);
}

void BoundaryShieldSettingsManager::Apply(Player* player) {
	settings_.warningDistance = std::clamp(settings_.warningDistance, 5.0f, 250.0f);
	settings_.wallWidth = std::clamp(settings_.wallWidth, 1.0f, 100.0f);
	settings_.wallHeight = std::clamp(settings_.wallHeight, 1.0f, 100.0f);
	settings_.ceilingWidth = std::clamp(settings_.ceilingWidth, 1.0f, 100.0f);
	settings_.ceilingHeight = std::clamp(settings_.ceilingHeight, 1.0f, 100.0f);
	settings_.maxOpacity = std::clamp(settings_.maxOpacity, 0.05f, 1.0f);
	settings_.pulseSpeed = std::clamp(settings_.pulseSpeed, 0.0f, 0.50f);
	settings_.pulseStrength = std::clamp(settings_.pulseStrength, 0.0f, 0.50f);
	settings_.surfaceOffset = std::clamp(settings_.surfaceOffset, 0.0f, 5.0f);
	settings_.color.x = std::clamp(settings_.color.x, 0.0f, 1.0f);
	settings_.color.y = std::clamp(settings_.color.y, 0.0f, 1.0f);
	settings_.color.z = std::clamp(settings_.color.z, 0.0f, 1.0f);
	if (player) {
		player->SetBoundaryWarningThreshold(settings_.warningDistance);
	}
}

bool BoundaryShieldSettingsManager::Load(const std::string& filePath, Player* player) {
	std::ifstream input(filePath);
	if (!input.is_open()) {
		return false;
	}

	try {
		json document;
		input >> document;
		const json& source = document.contains("boundary_shield") ? document["boundary_shield"] : document;
		BoundaryShieldSettings loaded = settings_;
		auto readFloat = [&source](const char* name, float& destination) {
			if (source.contains(name) && source[name].is_number()) {
				destination = source[name].get<float>();
			}
		};
		readFloat("warning_distance", loaded.warningDistance);
		readFloat("wall_width", loaded.wallWidth);
		readFloat("wall_height", loaded.wallHeight);
		readFloat("ceiling_width", loaded.ceilingWidth);
		readFloat("ceiling_height", loaded.ceilingHeight);
		readFloat("max_opacity", loaded.maxOpacity);
		readFloat("pulse_speed", loaded.pulseSpeed);
		readFloat("pulse_strength", loaded.pulseStrength);
		readFloat("surface_offset", loaded.surfaceOffset);
		if (source.contains("color") && source["color"].is_array() && source["color"].size() == 3) {
			if (source["color"][0].is_number()) loaded.color.x = source["color"][0].get<float>();
			if (source["color"][1].is_number()) loaded.color.y = source["color"][1].get<float>();
			if (source["color"][2].is_number()) loaded.color.z = source["color"][2].get<float>();
		}
		Set(loaded, player);
		return true;
	} catch (const std::exception&) {
		return false;
	}
}

bool BoundaryShieldSettingsManager::Save(const std::string& filePath) const {
	std::ofstream output(filePath, std::ios::trunc);
	if (!output.is_open()) {
		return false;
	}

	const json document = {
		{ "boundary_shield", {
			{ "warning_distance", settings_.warningDistance },
			{ "wall_width", settings_.wallWidth },
			{ "wall_height", settings_.wallHeight },
			{ "ceiling_width", settings_.ceilingWidth },
			{ "ceiling_height", settings_.ceilingHeight },
			{ "max_opacity", settings_.maxOpacity },
			{ "pulse_speed", settings_.pulseSpeed },
			{ "pulse_strength", settings_.pulseStrength },
			{ "surface_offset", settings_.surfaceOffset },
			{ "color", { settings_.color.x, settings_.color.y, settings_.color.z } },
		} }
	};
	output << document.dump(2) << '\n';
	return output.good();
}
