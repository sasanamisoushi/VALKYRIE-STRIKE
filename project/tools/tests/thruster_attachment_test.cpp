#include "3D/Trail.h"
#include "Game/Player/BoosterEffect.h"
#include <externals/json.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
Vector3 Add(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
void RequireNear(Vector3 a, Vector3 b) {
    const float error = std::sqrt((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y) + (a.z-b.z)*(a.z-b.z));
    if (error > 0.0002f) throw std::runtime_error("Plume root separated from transformed nozzle");
}
void RequireFloat(float actual, float expected) {
    if (std::abs(actual - expected) > 0.00001f) throw std::runtime_error("Unexpected persisted effect setting");
}
void CheckSettings() {
    const std::string path = "generated/tests/thruster/settings_roundtrip.json";
    std::filesystem::create_directories("generated/tests/thruster");
    {
        std::ofstream output(path);
        output << nlohmann::json{{"futureField", 17}, {"effect", {{"futureEffectField", 42}}}};
    }
    BoosterEffect source;
    auto fighterPlacement = source.GetPlacementSettings(0);
    fighterPlacement.positionOffset = {0.3f, -0.29f, 0.07f};
    source.SetPlacementSettings(0, fighterPlacement);
    auto gerwalkPlacement = source.GetPlacementSettings(1);
    gerwalkPlacement.positionOffset = {-0.18f, -0.42f, -0.12f};
    gerwalkPlacement.scale = {0.85f, 1.10f, 0.90f};
    gerwalkPlacement.rotationDegrees = {12.0f, -18.0f, 4.0f};
    source.SetPlacementSettings(1, gerwalkPlacement);
    source.AddExtraThruster(2);
    auto extraThruster = source.GetExtraThrusters(2).front();
    extraThruster = { {0.12f, -0.58f, -0.31f}, {0.75f, 1.15f, 0.80f}, {8.0f, -15.0f, 3.0f} };
    source.SetExtraThrusterSettings(2, 0, extraThruster);
    auto effect = source.GetEffectSettings();
    effect.widthScale = 3.0f;
    effect.lengthScale = 1.4f;
    effect.brightness = 1.6f;
    effect.glowScale = 2.2f;
    effect.coreLengthRatio = 0.36f;
    effect.boostWidthMultiplier = 1.55f;
    effect.boostLengthMultiplier = 1.1f;
    source.SetEffectSettings(effect);
    if (!source.SaveSettings(path)) throw std::runtime_error("Settings save failed");
    BoosterEffect loaded;
    if (!loaded.LoadSettings(path)) throw std::runtime_error("Settings load failed");
    RequireNear(loaded.GetPlacementAdjustment(), source.GetPlacementAdjustment());
    const auto& restoredGerwalkPlacement = loaded.GetPlacementSettings(1);
    RequireNear(restoredGerwalkPlacement.positionOffset, gerwalkPlacement.positionOffset);
    RequireNear(restoredGerwalkPlacement.scale, gerwalkPlacement.scale);
    RequireNear(restoredGerwalkPlacement.rotationDegrees, gerwalkPlacement.rotationDegrees);
    const auto& restoredExtraThrusters = loaded.GetExtraThrusters(2);
    if (restoredExtraThrusters.size() != 1) throw std::runtime_error("Extra thruster count was not persisted");
    RequireNear(restoredExtraThrusters[0].position, extraThruster.position);
    RequireNear(restoredExtraThrusters[0].scale, extraThruster.scale);
    RequireNear(restoredExtraThrusters[0].rotationDegrees, extraThruster.rotationDegrees);
    const auto& restored = loaded.GetEffectSettings();
    RequireFloat(restored.widthScale, 3.0f);
    RequireFloat(restored.lengthScale, 1.4f);
    RequireFloat(restored.brightness, 1.6f);
    RequireFloat(restored.glowScale, 2.2f);
    RequireFloat(restored.coreLengthRatio, 0.36f);
    RequireFloat(restored.boostWidthMultiplier, 1.55f);
    RequireFloat(restored.boostLengthMultiplier, 1.1f);
    {
        nlohmann::json saved;
        std::ifstream input(path);
        input >> saved;
        if (saved.at("futureField") != 17 || saved.at("effect").at("futureEffectField") != 42)
            throw std::runtime_error("Unknown settings fields were overwritten");
    }
    const auto writeCase = [&](const nlohmann::json& json) { std::ofstream output(path); output << json; };
    writeCase({{"positionOffset", {0.0f, -0.29f, 0.0f}}});
    if (!loaded.LoadSettings(path)) throw std::runtime_error("Legacy position-only settings rejected");
    RequireFloat(loaded.GetEffectSettings().widthScale, 1.0f);
    RequireFloat(loaded.GetPlacementAdjustment().y, -0.29f);
    auto editedPlacement = loaded.GetPlacementSettings(0);
    editedPlacement.positionOffset.x = 0.7f;
    loaded.SetPlacementSettings(0, editedPlacement);
    auto editedEffect = loaded.GetEffectSettings();
    editedEffect.widthScale = 2.0f;
    loaded.SetEffectSettings(editedEffect);
    writeCase({{"positionOffset", {0,0,0}}, {"effect", {{"widthScale", "invalid"}}}});
    if (loaded.LoadSettings(path)) throw std::runtime_error("Invalid settings accepted");
    RequireFloat(loaded.GetPlacementAdjustment().x, 0.7f);
    RequireFloat(loaded.GetEffectSettings().widthScale, 2.0f);
    writeCase({{"positionOffset", {9,-9,0}}, {"effect", {{"widthScale", 999}, {"lengthScale", -99},
        {"brightness", 999}, {"glowScale", -3}, {"coreLengthRatio", 0},
        {"boostWidthMultiplier", 0.5}, {"boostLengthMultiplier", 99}}}});
    if (!loaded.LoadSettings(path)) throw std::runtime_error("Numeric settings rejected");
    RequireFloat(loaded.GetEffectSettings().widthScale, 6.0f);
    RequireFloat(loaded.GetEffectSettings().lengthScale, 0.25f);
    RequireFloat(loaded.GetEffectSettings().brightness, 3.0f);
    RequireFloat(loaded.GetEffectSettings().glowScale, 0.0f);
    RequireFloat(loaded.GetEffectSettings().coreLengthRatio, 0.2f);
    RequireFloat(loaded.GetEffectSettings().boostWidthMultiplier, 1.0f);
    RequireFloat(loaded.GetEffectSettings().boostLengthMultiplier, 2.0f);
    RequireNear(loaded.GetPlacementAdjustment(), {3,-3,0});

    Trail shortWideFlame;
    shortWideFlame.SetOrigin({0,0,0});
    const auto geometry = shortWideFlame.GenerateThrusterVertices(nullptr, 0.5f, {0,0,-1}, 0.25f,
        0.0f, 1.0f, 0.2f, 0.0f);
    float axialLength = 0.0f;
    for (const auto& v : geometry) axialLength = (std::max)(axialLength, -v.position.z);
    RequireFloat(axialLength, 0.25f * 1.04f);
    std::cout << "PASS: appearance round-trip, legacy loading, validation, and independent width/length\n";
}
}

int main() {
    try {
        CheckSettings();
        constexpr float scale = 0.08f;
        constexpr float rear = scale * 1.02f;
        const Vector3 axis{0.0f, 0.0f, -1.0f};
        const Vector3 anchors[] = {
            {-0.771f * scale, (7.998f - 5.356f) * scale, (-10.772f - 0.393f) * scale},
            { 0.771f * scale, (7.998f - 5.356f) * scale, (-10.772f - 0.393f) * scale},
            {-1.5f * scale, -3.5f * scale, -1.5f * scale},
            { 1.5f * scale, -3.5f * scale, -1.5f * scale},
            {-0.8f * scale,  1.5f * scale, -2.0f * scale},
            { 0.8f * scale,  1.5f * scale, -2.0f * scale}
        };
        size_t rootChecks = 0;
        Trail trail;
        trail.Initialize(15);
        for (int frame = 0; frame < 120; ++frame) {
            // Sudden position jumps, reversing yaw/pitch, and a complete dodge roll.
            const float angle = static_cast<float>(frame) * 0.27f;
            const Quaternion rotation = MyMath::Normalize(MyMath::Multiply(
                MyMath::Multiply(MyMath::MakeAxisAngle({0,1,0}, angle),
                    MyMath::MakeAxisAngle({1,0,0}, std::sin(angle) * 1.3f)),
                MyMath::MakeAxisAngle({0,0,1}, angle * 2.0f)));
            const Vector3 position{std::sin(angle) * 200.0f, std::cos(angle) * 80.0f, frame * 3.0f};
            const Matrix4x4 parent = MyMath::MakeAffineMatrix({1,1,1}, rotation, position);
            for (size_t burner = 0; burner < 6; ++burner) {
                const Vector3 center = Add(anchors[burner], {0.13f, -0.29f, 0.07f});
                const Vector3 exit = Add(center, {0,0,-rear});
                // An old world-space history must never become the plume's root.
                trail.Update({999, -777, 555});
                trail.SetOrigin(exit);
                const auto vertices = trail.GenerateThrusterVertices(nullptr, 0.05f, axis, 2.0f, angle, 1.0f);
                if (vertices.empty()) throw std::runtime_error("No plume generated");

                // Reconstruct the separately drawn OBJ nozzle's opening.
                const float side = burner % 2 == 0 ? -1.0f : 1.0f;
                const Vector3 authoredCenter{side * 0.771f * scale, 7.998f * scale, -10.772f * scale};
                const Vector3 modelShift{center.x-authoredCenter.x, center.y-authoredCenter.y, center.z-authoredCenter.z};
                const Vector3 opening = Add(Add(position, MyMath::RotateVector(modelShift, rotation)),
                    MyMath::RotateVector(Add(authoredCenter, {0,0,-rear}), rotation));

                for (const auto& v : vertices) {
                    const Vector3 local{v.position.x, v.position.y, v.position.z};
                    if (!std::isfinite(local.x) || !std::isfinite(local.y) || !std::isfinite(local.z))
                        throw std::runtime_error("Non-finite plume geometry");
                    if (local.z > exit.z + 0.00001f) throw std::runtime_error("Plume points into aircraft");
                    if (v.texcoord.y == 0.0f) {
                        RequireNear(local, exit);
                        RequireNear(MyMath::Transform(local, parent), opening);
                        ++rootChecks;
                    }
                }
            }
        }
        std::cout << "PASS: " << rootChecks << " nozzle-root checks over 120 poses and 6 burners\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
