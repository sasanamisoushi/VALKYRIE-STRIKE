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

Vector3 ClampVector3(const Vector3& value, float minimum, float maximum) {
    return {
        std::clamp(value.x, minimum, maximum),
        std::clamp(value.y, minimum, maximum),
        std::clamp(value.z, minimum, maximum),
    };
}
}

size_t BoosterEffect::ToModeIndex(int playerMode) {
    return playerMode <= 0 ? 0u : (playerMode >= 3 ? 2u : static_cast<size_t>(playerMode));
}

const BoosterEffect::PlacementSettings& BoosterEffect::GetPlacementSettings(int playerMode) const {
    return placementSettings_[ToModeIndex(playerMode)];
}

void BoosterEffect::SetPlacementSettings(int playerMode, const PlacementSettings& settings) {
    PlacementSettings validated = settings;
    validated.positionOffset = ClampVector3(validated.positionOffset, -3.0f, 3.0f);
    validated.scale = ClampVector3(validated.scale, 0.2f, 3.0f);
    validated.rotationDegrees = ClampVector3(validated.rotationDegrees, -180.0f, 180.0f);
    placementSettings_[ToModeIndex(playerMode)] = validated;
}

Vector3 BoosterEffect::GetPlacementAdjustment(int playerMode) const {
    return GetPlacementSettings(playerMode).positionOffset;
}

void BoosterEffect::ResetPlacementAdjustment(int playerMode) {
    placementSettings_[ToModeIndex(playerMode)] = PlacementSettings{};
}

const std::vector<BoosterEffect::ExtraThrusterSettings>& BoosterEffect::GetExtraThrusters(int playerMode) const {
    return extraThrustersByMode_[ToModeIndex(playerMode)];
}

bool BoosterEffect::SetExtraThrusterSettings(int playerMode, size_t index, const ExtraThrusterSettings& settings) {
    std::vector<ExtraThrusterSettings>& extraThrusters = extraThrustersByMode_[ToModeIndex(playerMode)];
    if (index >= extraThrusters.size()) return false;

    ExtraThrusterSettings validated = settings;
    validated.position = ClampVector3(validated.position, -3.0f, 3.0f);
    validated.scale = ClampVector3(validated.scale, 0.2f, 3.0f);
    validated.rotationDegrees = ClampVector3(validated.rotationDegrees, -180.0f, 180.0f);
    extraThrusters[index] = validated;
    return true;
}

bool BoosterEffect::AddExtraThruster(int playerMode) {
    constexpr size_t kMaxExtraThrustersPerMode = 8;
    std::vector<ExtraThrusterSettings>& extraThrusters = extraThrustersByMode_[ToModeIndex(playerMode)];
    if (extraThrusters.size() >= kMaxExtraThrustersPerMode) return false;

    ExtraThrusterSettings extra;
    extra.position = { extraThrusters.size() % 2 == 0 ? -0.20f : 0.20f, -0.25f, -0.15f };
    extraThrusters.push_back(extra);
    lastMode_ = -1;
    return true;
}

bool BoosterEffect::RemoveExtraThruster(int playerMode, size_t index) {
    std::vector<ExtraThrusterSettings>& extraThrusters = extraThrustersByMode_[ToModeIndex(playerMode)];
    if (index >= extraThrusters.size()) return false;

    extraThrusters.erase(extraThrusters.begin() + static_cast<std::ptrdiff_t>(index));
    lastMode_ = -1;
    return true;
}

void BoosterEffect::SetEffectSettings(const EffectSettings& settings) {
    EffectSettings validated = settings;
    validated.widthScale = std::clamp(validated.widthScale, 0.25f, 6.0f);
    validated.lengthScale = std::clamp(validated.lengthScale, 0.25f, 4.0f);
    validated.brightness = std::clamp(validated.brightness, 0.1f, 3.0f);
    validated.glowScale = std::clamp(validated.glowScale, 0.0f, 3.0f);
    validated.coreLengthRatio = std::clamp(validated.coreLengthRatio, 0.2f, 0.8f);
    validated.boostWidthMultiplier = std::clamp(validated.boostWidthMultiplier, 1.0f, 2.0f);
    validated.boostLengthMultiplier = std::clamp(validated.boostLengthMultiplier, 1.0f, 2.0f);
    effectSettings_ = validated;
}

void BoosterEffect::ResetEffectSettings() {
    SetEffectSettings(EffectSettings{});
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
