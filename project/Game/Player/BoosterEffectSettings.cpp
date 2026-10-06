#include "BoosterEffect.h"
#include <externals/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>

bool BoosterEffect::SaveSettings(const std::string& filePath) const {
    nlohmann::json settings = nlohmann::json::object();
    // Keep unrelated/forward-compatible fields in an existing settings file.
    std::ifstream existing(filePath, std::ios::binary);
    if (existing.is_open()) {
        try {
            existing >> settings;
            if (!settings.is_object()) settings = nlohmann::json::object();
        } catch (const nlohmann::json::exception&) {
            settings = nlohmann::json::object();
        }
        existing.close();
    }
    settings["positionOffset"] = { placementAdjustment_.x, placementAdjustment_.y, placementAdjustment_.z };
    if (!settings.contains("effect") || !settings["effect"].is_object()) {
        settings["effect"] = nlohmann::json::object();
    }
    auto& effect = settings["effect"];
    effect["widthScale"] = effectSettings_.widthScale;
    effect["lengthScale"] = effectSettings_.lengthScale;
    effect["brightness"] = effectSettings_.brightness;
    effect["glowScale"] = effectSettings_.glowScale;
    effect["coreLengthRatio"] = effectSettings_.coreLengthRatio;
    effect["boostWidthMultiplier"] = effectSettings_.boostWidthMultiplier;
    effect["boostLengthMultiplier"] = effectSettings_.boostLengthMultiplier;
    std::ofstream output(filePath, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << std::setw(2) << settings << '\n';
    return output.good();
}

bool BoosterEffect::LoadSettings(const std::string& filePath) {
    std::ifstream input(filePath, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    try {
        nlohmann::json settings;
        input >> settings;
        const auto& offset = settings.at("positionOffset");
        if (!offset.is_array() || offset.size() != 3) {
            return false;
        }
        const auto boundedFloat = [](const nlohmann::json& value, float low, float high) {
            if (!value.is_number()) throw std::runtime_error("Invalid thruster setting");
            const float number = value.get<float>();
            if (!std::isfinite(number)) throw std::runtime_error("Non-finite thruster setting");
            return std::clamp(number, low, high);
        };
        const Vector3 positionOffset = {
            boundedFloat(offset.at(0), -3.0f, 3.0f),
            boundedFloat(offset.at(1), -3.0f, 3.0f),
            boundedFloat(offset.at(2), -3.0f, 3.0f)
        };
        EffectSettings appearance;
        if (settings.contains("effect")) {
            const auto& effect = settings.at("effect");
            if (!effect.is_object()) return false;
            const auto readSetting = [&](const char* key, float fallback, float low, float high) {
                const auto found = effect.find(key);
                return found == effect.end() ? fallback : boundedFloat(*found, low, high);
            };
            appearance.widthScale = readSetting("widthScale", appearance.widthScale, 0.25f, 6.0f);
            appearance.lengthScale = readSetting("lengthScale", appearance.lengthScale, 0.25f, 4.0f);
            appearance.brightness = readSetting("brightness", appearance.brightness, 0.1f, 3.0f);
            appearance.glowScale = readSetting("glowScale", appearance.glowScale, 0.0f, 3.0f);
            appearance.coreLengthRatio = readSetting("coreLengthRatio", appearance.coreLengthRatio, 0.2f, 0.8f);
            appearance.boostWidthMultiplier = readSetting("boostWidthMultiplier", appearance.boostWidthMultiplier, 1.0f, 2.0f);
            appearance.boostLengthMultiplier = readSetting("boostLengthMultiplier", appearance.boostLengthMultiplier, 1.0f, 2.0f);
        }
        // Apply only after the entire document has been validated. Legacy position-only files remain valid.
        placementAdjustment_ = positionOffset;
        effectSettings_ = appearance;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
