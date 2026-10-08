#include "BoosterEffect.h"
#include <externals/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace {
constexpr size_t kPlacementModeCount = 3;
constexpr size_t kMaxExtraThrustersPerMode = 8;

nlohmann::json ToJson(const Vector3& value) {
    return { value.x, value.y, value.z };
}
}

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
    // positionOffset は旧設定との互換用に残し、新しい形態別設定を正として保存する。
    settings["positionOffset"] = ToJson(placementSettings_[0].positionOffset);
    settings["placementByMode"] = nlohmann::json::array();
    for (const PlacementSettings& placement : placementSettings_) {
        settings["placementByMode"].push_back({
            { "positionOffset", ToJson(placement.positionOffset) },
            { "scale", ToJson(placement.scale) },
            { "rotationDegrees", ToJson(placement.rotationDegrees) },
        });
    }
    settings["extraThrustersByMode"] = nlohmann::json::array();
    for (const auto& extraThrusters : extraThrustersByMode_) {
        nlohmann::json modeThrusters = nlohmann::json::array();
        for (const ExtraThrusterSettings& extra : extraThrusters) {
            modeThrusters.push_back({
                { "position", ToJson(extra.position) },
                { "scale", ToJson(extra.scale) },
                { "rotationDegrees", ToJson(extra.rotationDegrees) },
            });
        }
        settings["extraThrustersByMode"].push_back(std::move(modeThrusters));
    }
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
        const auto boundedFloat = [](const nlohmann::json& value, float low, float high) {
            if (!value.is_number()) throw std::runtime_error("Invalid thruster setting");
            const float number = value.get<float>();
            if (!std::isfinite(number)) throw std::runtime_error("Non-finite thruster setting");
            return std::clamp(number, low, high);
        };
        const auto readVector3 = [&](const nlohmann::json& value, float low, float high) {
            if (!value.is_array() || value.size() != 3) {
                throw std::runtime_error("Invalid thruster vector setting");
            }
            return Vector3{
                boundedFloat(value.at(0), low, high),
                boundedFloat(value.at(1), low, high),
                boundedFloat(value.at(2), low, high),
            };
        };
        std::array<PlacementSettings, kPlacementModeCount> placements{};
        std::array<std::vector<ExtraThrusterSettings>, kPlacementModeCount> extraThrustersByMode{};
        if (settings.contains("placementByMode")) {
            const auto& savedPlacements = settings.at("placementByMode");
            if (!savedPlacements.is_array() || savedPlacements.size() != kPlacementModeCount) {
                return false;
            }
            for (size_t mode = 0; mode < kPlacementModeCount; ++mode) {
                const auto& savedPlacement = savedPlacements.at(mode);
                if (!savedPlacement.is_object()) return false;
                placements[mode].positionOffset = readVector3(savedPlacement.at("positionOffset"), -3.0f, 3.0f);
                placements[mode].scale = readVector3(savedPlacement.at("scale"), 0.2f, 3.0f);
                placements[mode].rotationDegrees = readVector3(savedPlacement.at("rotationDegrees"), -180.0f, 180.0f);
            }
        } else {
            // 既存の位置だけの設定は、従来どおり全形態へ適用する。
            const Vector3 legacyOffset = readVector3(settings.at("positionOffset"), -3.0f, 3.0f);
            for (PlacementSettings& placement : placements) {
                placement.positionOffset = legacyOffset;
            }
        }
        if (settings.contains("extraThrustersByMode")) {
            const auto& savedExtraThrusters = settings.at("extraThrustersByMode");
            if (!savedExtraThrusters.is_array() || savedExtraThrusters.size() != kPlacementModeCount) {
                return false;
            }
            for (size_t mode = 0; mode < kPlacementModeCount; ++mode) {
                const auto& savedModeThrusters = savedExtraThrusters.at(mode);
                if (!savedModeThrusters.is_array() || savedModeThrusters.size() > kMaxExtraThrustersPerMode) {
                    return false;
                }
                for (const auto& savedThruster : savedModeThrusters) {
                    if (!savedThruster.is_object()) return false;
                    ExtraThrusterSettings extra;
                    extra.position = readVector3(savedThruster.at("position"), -3.0f, 3.0f);
                    extra.scale = readVector3(savedThruster.at("scale"), 0.2f, 3.0f);
                    extra.rotationDegrees = readVector3(savedThruster.at("rotationDegrees"), -180.0f, 180.0f);
                    extraThrustersByMode[mode].push_back(extra);
                }
            }
        }
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
        placementSettings_ = placements;
        extraThrustersByMode_ = std::move(extraThrustersByMode);
        effectSettings_ = appearance;
        // 読み込みで追加ノズル数が変わるため、次回の配置更新でノズル群を作り直す。
        lastMode_ = -1;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
