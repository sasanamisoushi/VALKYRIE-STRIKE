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

}


void BoosterEffect::SetupBurnersForMode(int playerMode) {
    burners_.clear();
    lastMode_ = playerMode;

    const float scale = kPlayerModelScale;

    const auto makeBurner = [](const Vector3& offset, const Vector4& color,
        int trailLength, const std::string& nozzleModel, const std::string& trailModel, float modelSide,
        bool isExtraThruster = false, size_t extraThrusterIndex = 0) {
        Burner burner;
        burner.trail = std::make_unique<Trail>();
        burner.trail->Initialize(trailLength);
        burner.trailObject = std::make_unique<Object3d>();
        burner.trailObject->Initialize(Object3dCommon::GetInstance());
        burner.trailObject->SetModel(trailModel);

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
        burners_.push_back(makeBurner({ -fighterNozzleOffset.x * scale, fighterNozzleOffset.y * scale, fighterNozzleOffset.z * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 15, "vf-15c/thruster_left.obj", "PlayerThrusterTrailLeft", -1.0f));
        burners_.push_back(makeBurner({  fighterNozzleOffset.x * scale, fighterNozzleOffset.y * scale, fighterNozzleOffset.z * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 15, "vf-15c/thruster_right.obj", "PlayerThrusterTrailRight", 1.0f));
    } else if (playerMode == 1) { // Gerwalk: 脚部のふくらはぎ後方・左右ノズル
        burners_.push_back(makeBurner({ -2.1f * scale, -5.3f * scale, -1.6f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 12, "vf-15c/thruster_left.obj", "PlayerThrusterTrailLeft", -1.0f));
        burners_.push_back(makeBurner({  2.1f * scale, -5.3f * scale, -1.6f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 12, "vf-15c/thruster_right.obj", "PlayerThrusterTrailRight", 1.0f));
    } else { // Battroid: 脚部のふくらはぎ後方・左右ノズル
        burners_.push_back(makeBurner({ -1.85f * scale, -6.2f * scale, -1.45f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 10, "vf-15c/thruster_left.obj", "PlayerThrusterTrailLeft", -1.0f));
        burners_.push_back(makeBurner({  1.85f * scale, -6.2f * scale, -1.45f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 10, "vf-15c/thruster_right.obj", "PlayerThrusterTrailRight", 1.0f));
    }

    // 追加ノズルは各形態ごとに独立して持つ。炎の頂点を共有しないよう、
    // 1基ごとに専用トレイルモデルを用意する。
    const size_t modeIndex = playerMode <= 0 ? 0u : (playerMode >= 3 ? 2u : static_cast<size_t>(playerMode));
    ModelManager* modelManager = ModelManager::GetInstance();
    const auto& extraThrusters = extraThrustersByMode_[modeIndex];
    for (size_t index = 0; index < extraThrusters.size(); ++index) {
        const bool isRight = index % 2 != 0;
        const std::string trailModel = "PlayerThrusterTrailExtra_" + std::to_string(modeIndex) + "_" + std::to_string(index);
        modelManager->CreateTrailModel(trailModel);
        burners_.push_back(makeBurner({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.08f, 0.72f, 0.9f }, 12,
            isRight ? "vf-15c/thruster_right.obj" : "vf-15c/thruster_left.obj", trailModel,
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
        if (burner.trail && burner.trailObject) {
            // 炎は機体ローカルで生成し、取付回転を排気方向へ反映する。
            burner.adjustedExhaustDirection = MyMath::RotateVector(burner.exhaustDirection, placementRotation);
            burner.placementScale = burnerPlacement.scale;
            const Vector3 localExit = {
                adjustedOffset.x + burner.adjustedExhaustDirection.x * kNozzleRearOffset,
                adjustedOffset.y + burner.adjustedExhaustDirection.y * kNozzleRearOffset,
                adjustedOffset.z + burner.adjustedExhaustDirection.z * kNozzleRearOffset
            };
            burner.trail->SetOrigin(localExit);
            burner.trailObject->SetTranslate(position);
            burner.trailObject->SetScale({ 1.0f, 1.0f, 1.0f });
            burner.trailObject->SetQuaternionRotate(rotation);
        }
    }
}

bool BoosterEffect::AddExtraThruster(int playerMode) {
    constexpr size_t kMaxExtraThrustersPerMode = 8;
    std::vector<ExtraThrusterSettings>& extraThrusters = GetExtraThrusters(playerMode);
    if (extraThrusters.size() >= kMaxExtraThrustersPerMode) {
        return false;
    }
    // 左右に追加しやすいよう、追加するたびに初期X座標を交互にする。
    ExtraThrusterSettings extra;
    extra.position = { extraThrusters.size() % 2 == 0 ? -0.20f : 0.20f, -0.25f, -0.15f };
    extraThrusters.push_back(extra);
    lastMode_ = -1;
    return true;
}

bool BoosterEffect::RemoveExtraThruster(int playerMode, size_t index) {
    std::vector<ExtraThrusterSettings>& extraThrusters = GetExtraThrusters(playerMode);
    if (index >= extraThrusters.size()) {
        return false;
    }
    extraThrusters.erase(extraThrusters.begin() + static_cast<std::ptrdiff_t>(index));
    lastMode_ = -1;
    return true;
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

    for (auto& burner : burners_) {
        // 長い白い棒に見えないよう、短い噴射を基準に幅で加速を表す。
        burner.plumeLength = burner.plumeLength > 0.0f
            ? burner.plumeLength + (targetLength - burner.plumeLength) * 0.25f : targetLength;
        burner.plumeRadius = burner.plumeRadius > 0.0f
            ? burner.plumeRadius + (targetRadius - burner.plumeRadius) * 0.25f : targetRadius;
        burner.color.w += (targetAlpha - burner.color.w) * 0.25f;
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
    object3dCommon->SetEffectDrawSettings();
}
