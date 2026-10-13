#include "BoosterEffect.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
constexpr float kPlayerModelScale = 0.08f;
constexpr float kNozzleRearOffset = kPlayerModelScale * 1.02f;
// Model::LoadGltfFile centers the VF-15C mesh around its bounding-box center.
// The thruster OBJ still uses the original Blender scene coordinates, so use
// the same center offset when converting its attachment point to player-local.
constexpr Vector3 kVf15cModelCenterOffset = { 0.0f, 5.356f, 0.393f };
constexpr const char* kThrusterPlacementSettingsPath = "resources/vf-15c/thruster_position.json";

Quaternion MakePlacementRotation(const Vector3& degrees) {
    constexpr float kDegreesToRadians = 3.14159265359f / 180.0f;
    const Quaternion pitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, degrees.x * kDegreesToRadians);
    const Quaternion yaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, degrees.y * kDegreesToRadians);
    const Quaternion roll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, degrees.z * kDegreesToRadians);
    return MyMath::Normalize(MyMath::Multiply(MyMath::Multiply(pitch, yaw), roll));
}

}

void BoosterEffect::Initialize() {
    lastMode_ = -1;
    time_ = 0.0f;
    accelerationBlend_ = 0.0f;
    LoadSettings(kThrusterPlacementSettingsPath);

    // フォルダ内のOBJを機体スケール付きで使う。OBJ頂点にはBlender上の
    // 左右ノズル位置が含まれているが、機体モデルの読み込み時の中心補正分を考慮する。
    ModelManager* modelManager = ModelManager::GetInstance();
    modelManager->LoadModel("vf-15c/thruster_left.obj");
    modelManager->LoadModel("vf-15c/thruster_right.obj");
    for (const char* name : { "vf-15c/thruster_left.obj", "vf-15c/thruster_right.obj" }) {
        Model* nozzle = modelManager->FindModel(name);
        if (!nozzle) continue;
        nozzle->SetTextureFilePath("resources/white1x1.png");
        nozzle->SetEnableLighting(true);
        nozzle->SetAlphaReference(0.0f);
        nozzle->SetColor({ 0.16f, 0.18f, 0.22f, 1.0f });
    }

    modelManager->CreateSphereModel("PlayerThrusterPlacementMarker", 12);
    if (Model* marker = modelManager->FindModel("PlayerThrusterPlacementMarker")) {
        marker->SetTextureFilePath("resources/white1x1.png");
        marker->SetEnableLighting(false);
        marker->SetAlphaReference(0.0f);
        marker->SetColor({ 1.0f, 0.32f, 0.02f, 1.0f });
    }
    modelManager->CreateTrailModel("PlayerThrusterTrailLeft");
    modelManager->CreateTrailModel("PlayerThrusterTrailRight");
    modelManager->CreateTrailModel("PlayerThrusterRibbonLeft");
    modelManager->CreateTrailModel("PlayerThrusterRibbonRight");

}


void BoosterEffect::SetupBurnersForMode(int playerMode) {
    burners_.clear();
    lastMode_ = playerMode;

    const float scale = kPlayerModelScale;

    const auto makeBurner = [](const Vector3& offset, const Vector4& color,
        int trailLength, const std::string& nozzleModel, const std::string& trailModel,
        const std::string& ribbonModel, float modelSide,
        bool isExtraThruster = false, size_t extraThrusterIndex = 0) {
        Burner burner;
        burner.trail = std::make_unique<Trail>();
        burner.trail->Initialize(trailLength);
        burner.trailObject = std::make_unique<Object3d>();
        burner.trailObject->Initialize(Object3dCommon::GetInstance());
        burner.trailObject->SetModel(trailModel);

        burner.ribbonTrail = std::make_unique<Trail>();
        burner.ribbonTrail->Initialize(28);
        burner.ribbonObject = std::make_unique<Object3d>();
        burner.ribbonObject->Initialize(Object3dCommon::GetInstance());
        burner.ribbonObject->SetModel(ribbonModel);

        burner.nozzleObject = std::make_unique<Object3d>();
        burner.nozzleObject->Initialize(Object3dCommon::GetInstance());
        burner.nozzleObject->SetModel(nozzleModel);
        burner.positionMarkerObject = std::make_unique<Object3d>();
        burner.positionMarkerObject->Initialize(Object3dCommon::GetInstance());
        burner.positionMarkerObject->SetModel("PlayerThrusterPlacementMarker");

        burner.offset = offset;
        burner.nozzleOriginOffset = { modelSide * 0.771f * kPlayerModelScale,
            7.998f * kPlayerModelScale, -10.772f * kPlayerModelScale };
        // ゲーム内のプレイヤー前方はローカル +Z。三人称カメラはその反対側
        // （-Z、機体後方）に置かれるため、噴射も -Z 側へ伸ばす。
        burner.exhaustDirection = { 0.0f, 0.0f, -1.0f };
        burner.adjustedExhaustDirection = burner.exhaustDirection;
        burner.color = color;
        burner.isExtraThruster = isExtraThruster;
        burner.extraThrusterIndex = extraThrusterIndex;
        return burner;
    };

    if (playerMode == 0) { // Fighter: Blender参照画像の胴体後部・左右ノズル
        // OBJはBlenderシーン座標、機体GLBは中心補正済み。モデル中心分を
        // 差し引いてから縮尺を掛け、ノズルが機体上面へ浮くのを防ぐ。
        const Vector3 fighterNozzleOffset = {
            0.771f, 7.998f - kVf15cModelCenterOffset.y,
            -10.772f - kVf15cModelCenterOffset.z
        };
        burners_.push_back(makeBurner({ -fighterNozzleOffset.x * scale, fighterNozzleOffset.y * scale, fighterNozzleOffset.z * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 15, "vf-15c/thruster_left.obj", "PlayerThrusterTrailLeft", "PlayerThrusterRibbonLeft", -1.0f));
        burners_.push_back(makeBurner({  fighterNozzleOffset.x * scale, fighterNozzleOffset.y * scale, fighterNozzleOffset.z * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 15, "vf-15c/thruster_right.obj", "PlayerThrusterTrailRight", "PlayerThrusterRibbonRight", 1.0f));
    } else if (playerMode == 1) { // Gerwalk: 脚部のふくらはぎ後方・左右ノズル
        burners_.push_back(makeBurner({ -2.1f * scale, -5.3f * scale, -1.6f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 12, "vf-15c/thruster_left.obj", "PlayerThrusterTrailLeft", "PlayerThrusterRibbonLeft", -1.0f));
        burners_.push_back(makeBurner({  2.1f * scale, -5.3f * scale, -1.6f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 12, "vf-15c/thruster_right.obj", "PlayerThrusterTrailRight", "PlayerThrusterRibbonRight", 1.0f));
    } else { // Battroid: 脚部のふくらはぎ後方・左右ノズル
        burners_.push_back(makeBurner({ -1.85f * scale, -6.2f * scale, -1.45f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 10, "vf-15c/thruster_left.obj", "PlayerThrusterTrailLeft", "PlayerThrusterRibbonLeft", -1.0f));
        burners_.push_back(makeBurner({  1.85f * scale, -6.2f * scale, -1.45f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 10, "vf-15c/thruster_right.obj", "PlayerThrusterTrailRight", "PlayerThrusterRibbonRight", 1.0f));
    }

    // 追加ノズルは各形態ごとに独立して持つ。炎の頂点を共有しないよう、
    // 1基ごとに専用トレイルモデルを用意する。
    const size_t modeIndex = playerMode <= 0 ? 0u : (playerMode >= 3 ? 2u : static_cast<size_t>(playerMode));
    ModelManager* modelManager = ModelManager::GetInstance();
    const auto& extraThrusters = extraThrustersByMode_[modeIndex];
    for (size_t index = 0; index < extraThrusters.size(); ++index) {
        const bool isRight = index % 2 != 0;
        const std::string trailModel = "PlayerThrusterTrailExtra_" + std::to_string(modeIndex) + "_" + std::to_string(index);
        const std::string ribbonModel = "PlayerThrusterRibbonExtra_" + std::to_string(modeIndex) + "_" + std::to_string(index);
        modelManager->CreateTrailModel(trailModel);
        modelManager->CreateTrailModel(ribbonModel);
        burners_.push_back(makeBurner({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.08f, 0.72f, 0.9f }, 12,
            isRight ? "vf-15c/thruster_right.obj" : "vf-15c/thruster_left.obj", trailModel, ribbonModel,
            isRight ? 1.0f : -1.0f, true, index));
    }
}

void BoosterEffect::RefreshPlacement(const Vector3& position, const Quaternion& rotation, int playerMode) {
    if (playerMode != lastMode_) {
        SetupBurnersForMode(playerMode);
    }

    const Matrix4x4 rotationMatrix = MyMath::MakeRotateMatrix(rotation);
    const PlacementSettings& placement = GetPlacementSettings(playerMode);
    const size_t modeIndex = playerMode <= 0 ? 0u : (playerMode >= 3 ? 2u : static_cast<size_t>(playerMode));
    for (auto& burner : burners_) {
        PlacementSettings burnerPlacement = placement;
        Vector3 baseOffset = burner.offset;
        if (burner.isExtraThruster && burner.extraThrusterIndex < extraThrustersByMode_[modeIndex].size()) {
            const ExtraThrusterSettings& extra = extraThrustersByMode_[modeIndex][burner.extraThrusterIndex];
            // 追加ノズルは既定ノズル全体の補正とは分け、各基を個別に配置できる。
            burnerPlacement.positionOffset = extra.position;
            burnerPlacement.scale = extra.scale;
            burnerPlacement.rotationDegrees = extra.rotationDegrees;
            baseOffset = { 0.0f, 0.0f, 0.0f };
        }
        const Quaternion placementRotation = MakePlacementRotation(burnerPlacement.rotationDegrees);
        const Quaternion nozzleRotation = MyMath::Normalize(MyMath::Multiply(placementRotation, rotation));
        const Vector3 adjustedOffset = {
            baseOffset.x + burnerPlacement.positionOffset.x,
            baseOffset.y + burnerPlacement.positionOffset.y,
            baseOffset.z + burnerPlacement.positionOffset.z
        };
        const Vector3 worldOffset = MyMath::Transform(adjustedOffset, rotationMatrix);
        const Vector3 modelTranslation = MyMath::Transform(
            { adjustedOffset.x - burner.nozzleOriginOffset.x,
                adjustedOffset.y - burner.nozzleOriginOffset.y,
                adjustedOffset.z - burner.nozzleOriginOffset.z }, rotationMatrix);

        if (burner.nozzleObject) {
            burner.nozzleObject->SetTranslate({ position.x + modelTranslation.x,
                position.y + modelTranslation.y, position.z + modelTranslation.z });
            burner.nozzleObject->SetScale({ kPlayerModelScale * burnerPlacement.scale.x,
                kPlayerModelScale * burnerPlacement.scale.y, kPlayerModelScale * burnerPlacement.scale.z });
            burner.nozzleObject->SetQuaternionRotate(nozzleRotation);
        }
        if (burner.positionMarkerObject) {
            burner.positionMarkerObject->SetTranslate({ position.x + worldOffset.x,
                position.y + worldOffset.y, position.z + worldOffset.z });
            burner.positionMarkerObject->SetScale({ 0.045f, 0.045f, 0.045f });
        }
        // 炎とリボンは同じノズル開口を始点にする。
        burner.adjustedExhaustDirection = MyMath::RotateVector(burner.exhaustDirection, placementRotation);
        burner.placementScale = burnerPlacement.scale;
        const Vector3 localExit = {
            adjustedOffset.x + burner.adjustedExhaustDirection.x * kNozzleRearOffset,
            adjustedOffset.y + burner.adjustedExhaustDirection.y * kNozzleRearOffset,
            adjustedOffset.z + burner.adjustedExhaustDirection.z * kNozzleRearOffset
        };
        if (burner.trail && burner.trailObject) {
            burner.trail->SetOrigin(localExit);
            burner.trailObject->SetTranslate(position);
            burner.trailObject->SetScale({ 1.0f, 1.0f, 1.0f });
            burner.trailObject->SetQuaternionRotate(rotation);
        }
        if (burner.ribbonTrail && burner.ribbonObject) {
            const Vector3 worldExitOffset = MyMath::Transform(localExit, rotationMatrix);
            const Vector3 worldExit = {
                position.x + worldExitOffset.x,
                position.y + worldExitOffset.y,
                position.z + worldExitOffset.z
            };
            const Vector3 delta = {
                worldExit.x - burner.lastRibbonWorldExit.x,
                worldExit.y - burner.lastRibbonWorldExit.y,
                worldExit.z - burner.lastRibbonWorldExit.z
            };
            const float distanceSquared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            if (burner.ribbonIntensity <= 0.02f) {
                burner.ribbonTrail->SetOrigin(worldExit);
                burner.hasRibbonOrigin = false;
            } else if (!burner.hasRibbonOrigin) {
                burner.ribbonTrail->SetOrigin(worldExit);
                burner.hasRibbonOrigin = true;
            } else if (distanceSquared > 0.000001f) {
                burner.ribbonTrail->Update(worldExit);
            }
            burner.lastRibbonWorldExit = worldExit;
            burner.ribbonObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
            burner.ribbonObject->SetScale({ 1.0f, 1.0f, 1.0f });
            burner.ribbonObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
        }
    }
}

void BoosterEffect::Update(const Vector3& position, const Quaternion& rotation, int playerMode, float speedRatio, bool isAccelerating) {
    if (playerMode != lastMode_) {
        SetupBurnersForMode(playerMode);
    }

    time_ += 0.4f;

    const float cappedSpeedRatio = std::clamp(speedRatio, 0.0f, 1.2f);
    // 加速・解除に約0.2秒で追従し、幅と光量が自然に変化する。
    accelerationBlend_ += ((isAccelerating ? 1.0f : 0.0f) - accelerationBlend_) * 0.18f;
    const float flicker = 1.0f + std::sin(time_) * 0.02f;
    const float targetLength = kPlayerModelScale * (9.5f + cappedSpeedRatio * 3.2f)
        * (1.0f + accelerationBlend_ * (effectSettings_.boostLengthMultiplier - 1.0f)) * flicker;
    const float targetRadius = kPlayerModelScale * (0.56f + cappedSpeedRatio * 0.16f)
        * (1.0f + accelerationBlend_ * (effectSettings_.boostWidthMultiplier - 1.0f));
    const float targetAlpha = std::clamp(0.40f + cappedSpeedRatio * 0.40f
        + accelerationBlend_ * 0.18f, 0.0f, 0.92f);
    const float targetRibbonIntensity = (!ribbonTrailEnabled_ || cappedSpeedRatio <= 0.06f) ? 0.0f : std::clamp(
        0.18f + cappedSpeedRatio * 0.72f + accelerationBlend_ * 0.20f, 0.0f, 1.0f);

    for (auto& burner : burners_) {
        // 長い白い棒に見えないよう、短い噴射を基準に幅で加速を表す。
        burner.plumeLength = burner.plumeLength > 0.0f
            ? burner.plumeLength + (targetLength - burner.plumeLength) * 0.25f : targetLength;
        burner.plumeRadius = burner.plumeRadius > 0.0f
            ? burner.plumeRadius + (targetRadius - burner.plumeRadius) * 0.25f : targetRadius;
        burner.color.w += (targetAlpha - burner.color.w) * 0.25f;
        burner.ribbonIntensity = ribbonTrailEnabled_
            ? burner.ribbonIntensity + (targetRibbonIntensity - burner.ribbonIntensity) * 0.20f
            : 0.0f;
        if (!ribbonTrailEnabled_) {
            burner.hasRibbonOrigin = false;
        }
    }
    RefreshPlacement(position, rotation, playerMode);
}

void BoosterEffect::UpdateEditorPreview(const Vector3& position, const Quaternion& rotation, int playerMode) {
    // 停止中でも一定出力で更新し、向きや機体の角度を変えて見た目を確認できる。
    Update(position, rotation, playerMode, 0.65f, editorPreviewAccelerating_);
}

void BoosterEffect::DrawNozzles(Camera* camera) {
    // ノズル本体は機体と同じ奥行き・通常描画で表示する。
    Object3dCommon* object3dCommon = Object3dCommon::GetInstance();
    object3dCommon->SetCommonDrawSettings();
    for (auto& burner : burners_) {
        if (burner.nozzleObject) {
            burner.nozzleObject->Update(camera);
            burner.nozzleObject->Draw();
        }
    }
}

void BoosterEffect::Draw(Camera* camera) {
    if (!camera) return;
    Object3dCommon* object3dCommon = Object3dCommon::GetInstance();

    if (showPlacementMarkers_) {
        object3dCommon->SetOverlayEffectDrawSettings();
        for (auto& burner : burners_) {
            if (burner.positionMarkerObject) {
                burner.positionMarkerObject->Update(camera);
                burner.positionMarkerObject->Draw();
            }
        }
        object3dCommon->SetCommonDrawSettings();
    }

    // 噴射炎は加算合成しつつ、機体・地形との前後関係を守る。
    if (thrusterPlumeEnabled_) {
        object3dCommon->SetThrusterDrawSettings();
        for (auto& burner : burners_) {
            if (burner.trail && burner.trailObject && burner.trailObject->GetModel()) {
                const float plumeScale = std::clamp(
                    (burner.placementScale.x + burner.placementScale.y + burner.placementScale.z) / 3.0f,
                    0.2f, 3.0f);
                std::vector<VertexData> trailVertices = burner.trail->GenerateThrusterVertices(
                    camera, burner.plumeRadius * effectSettings_.widthScale * plumeScale,
                    burner.adjustedExhaustDirection,
                    burner.plumeLength * effectSettings_.lengthScale * plumeScale, time_, burner.color.w,
                    effectSettings_.coreLengthRatio, effectSettings_.glowScale);

                // 頂点の発光色を保ち、材質には明るさの倍率だけを掛ける。
                const float brightness = effectSettings_.brightness;
                burner.trailObject->GetModel()->SetColor({ brightness, brightness, brightness, 1.0f });
                
                burner.trailObject->GetModel()->UpdateTrailVertices(trailVertices, false);
                burner.trailObject->Update(camera);
                
                burner.trailObject->Draw();
            }
        }
    }

    // 立体の炎の後ろへ、実際のノズル移動履歴から作った光の速度線を重ねる。
    // カメラ正面を向く帯なので、急旋回時でも細いラインとして視認できる。
    if (ribbonTrailEnabled_) {
        object3dCommon->SetEffectDrawSettings();
        for (auto& burner : burners_) {
            if (!burner.ribbonTrail || !burner.ribbonObject || !burner.ribbonObject->GetModel() ||
                burner.ribbonIntensity <= 0.02f) {
                continue;
            }
            const float plumeScale = std::clamp(
                (burner.placementScale.x + burner.placementScale.y + burner.placementScale.z) / 3.0f,
                0.2f, 3.0f);
            const float ribbonWidth = (0.030f + 0.075f * burner.ribbonIntensity) *
                effectSettings_.widthScale * plumeScale;
            std::vector<VertexData> ribbonVertices = burner.ribbonTrail->GenerateRibbonVertices(
                camera, ribbonWidth, { 0.42f, 0.82f, 1.35f, 0.82f }, burner.ribbonIntensity);
            if (ribbonVertices.empty()) {
                continue;
            }
            burner.ribbonObject->GetModel()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            burner.ribbonObject->GetModel()->UpdateTrailVertices(ribbonVertices, true);
            burner.ribbonObject->Update(camera);
            burner.ribbonObject->Draw();
        }
    }
    object3dCommon->SetEffectDrawSettings();
}
