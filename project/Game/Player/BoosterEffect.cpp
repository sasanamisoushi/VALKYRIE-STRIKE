#include "BoosterEffect.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
constexpr float kDefaultPlayerModelScale = 0.08f;
constexpr float kNozzleRearOffsetMultiplier = 1.02f;
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
    modelManager->CreateTrailModel("PlayerThrusterSpeedTrailLeft");
    modelManager->CreateTrailModel("PlayerThrusterSpeedTrailRight");
    modelManager->CreateTrailModel("PlayerThrusterFlareLeft");
    modelManager->CreateTrailModel("PlayerThrusterFlareRight");

}

void BoosterEffect::SetPlayerModelScale(float scale) {
    // 演出用にプレイヤーを拡大しても、プレイ中と同じ相対位置に
    // ノズルと炎を保つ。極小値へのクランプでゼロ除算も防ぐ。
    const float safeScale = (std::max)(scale, 0.0001f);
    if (std::abs(playerModelScale_ - safeScale) < 0.0001f) {
        return;
    }
    playerModelScale_ = safeScale;
    if (lastMode_ >= 0) {
        SetupBurnersForMode(lastMode_);
    }
}

void BoosterEffect::SetupBurnersForMode(int playerMode) {
    burners_.clear();
    lastMode_ = playerMode;

    const size_t modeIndex = playerMode <= 0 ? 0u : (playerMode >= 3 ? 2u : static_cast<size_t>(playerMode));
    // 既定2基と追加ノズルを先に確保し、Burner（所有ポインタを含む）の
    // push_back 中に再配置が起きないようにする。
    burners_.reserve(2 + extraThrustersByMode_[modeIndex].size());

    const float scale = playerModelScale_;

    const auto makeBurner = [this](const Vector3& offset, const Vector4& color,
        int trailLength, const std::string& nozzleModel, const std::string& trailModel,
        const std::string& speedTrailModel, const std::string& flareModel, float modelSide,
        bool isExtraThruster = false, size_t extraThrusterIndex = 0) {
        Burner burner;
        burner.trail = std::make_unique<Trail>();
        burner.trail->Initialize(trailLength);
        burner.trailObject = std::make_unique<Object3d>();
        burner.trailObject->Initialize(Object3dCommon::GetInstance());
        burner.trailObject->SetModel(trailModel);

        burner.speedTrail = std::make_unique<Trail>();
        // 距離固定サンプリングは高速時に1フレームへ複数点を入れるため、
        // 最長0.76秒ぶんを切らずに保持できる余裕を持たせる。
        burner.speedTrail->Initialize(128);
        burner.speedTrailObject = std::make_unique<Object3d>();
        burner.speedTrailObject->Initialize(Object3dCommon::GetInstance());
        burner.speedTrailObject->SetModel(speedTrailModel);
        burner.speedTrailFlareObject = std::make_unique<Object3d>();
        burner.speedTrailFlareObject->Initialize(Object3dCommon::GetInstance());
        burner.speedTrailFlareObject->SetModel(flareModel);

        burner.nozzleObject = std::make_unique<Object3d>();
        burner.nozzleObject->Initialize(Object3dCommon::GetInstance());
        burner.nozzleObject->SetModel(nozzleModel);
        burner.positionMarkerObject = std::make_unique<Object3d>();
        burner.positionMarkerObject->Initialize(Object3dCommon::GetInstance());
        burner.positionMarkerObject->SetModel("PlayerThrusterPlacementMarker");

        burner.offset = offset;
        burner.nozzleOriginOffset = { modelSide * 0.771f * playerModelScale_,
            7.998f * playerModelScale_, -10.772f * playerModelScale_ };
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
        burners_.push_back(makeBurner({ -fighterNozzleOffset.x * scale, fighterNozzleOffset.y * scale, fighterNozzleOffset.z * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 15, "vf-15c/thruster_left.obj", "PlayerThrusterTrailLeft", "PlayerThrusterSpeedTrailLeft", "PlayerThrusterFlareLeft", -1.0f));
        burners_.push_back(makeBurner({  fighterNozzleOffset.x * scale, fighterNozzleOffset.y * scale, fighterNozzleOffset.z * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 15, "vf-15c/thruster_right.obj", "PlayerThrusterTrailRight", "PlayerThrusterSpeedTrailRight", "PlayerThrusterFlareRight", 1.0f));
    } else if (playerMode == 1) { // Gerwalk: 脚部のふくらはぎ後方・左右ノズル
        burners_.push_back(makeBurner({ -2.1f * scale, -5.3f * scale, -1.6f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 12, "vf-15c/thruster_left.obj", "PlayerThrusterTrailLeft", "PlayerThrusterSpeedTrailLeft", "PlayerThrusterFlareLeft", -1.0f));
        burners_.push_back(makeBurner({  2.1f * scale, -5.3f * scale, -1.6f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 12, "vf-15c/thruster_right.obj", "PlayerThrusterTrailRight", "PlayerThrusterSpeedTrailRight", "PlayerThrusterFlareRight", 1.0f));
    } else { // Battroid: 脚部のふくらはぎ後方・左右ノズル
        burners_.push_back(makeBurner({ -1.85f * scale, -6.2f * scale, -1.45f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 10, "vf-15c/thruster_left.obj", "PlayerThrusterTrailLeft", "PlayerThrusterSpeedTrailLeft", "PlayerThrusterFlareLeft", -1.0f));
        burners_.push_back(makeBurner({  1.85f * scale, -6.2f * scale, -1.45f * scale }, { 1.0f, 0.08f, 0.72f, 0.9f }, 10, "vf-15c/thruster_right.obj", "PlayerThrusterTrailRight", "PlayerThrusterSpeedTrailRight", "PlayerThrusterFlareRight", 1.0f));
    }

    // 追加ノズルは各形態ごとに独立して持つ。炎の頂点を共有しないよう、
    // 1基ごとに専用トレイルモデルを用意する。
    ModelManager* modelManager = ModelManager::GetInstance();
    const auto& extraThrusters = extraThrustersByMode_[modeIndex];
    for (size_t index = 0; index < extraThrusters.size(); ++index) {
        const bool isRight = index % 2 != 0;
        const std::string trailModel = "PlayerThrusterTrailExtra_" + std::to_string(modeIndex) + "_" + std::to_string(index);
        const std::string speedTrailModel = "PlayerThrusterSpeedTrailExtra_" + std::to_string(modeIndex) + "_" + std::to_string(index);
        const std::string flareModel = "PlayerThrusterFlareExtra_" + std::to_string(modeIndex) + "_" + std::to_string(index);
        modelManager->CreateTrailModel(trailModel);
        modelManager->CreateTrailModel(speedTrailModel);
        modelManager->CreateTrailModel(flareModel);
        burners_.push_back(makeBurner({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.08f, 0.72f, 0.9f }, 12,
            isRight ? "vf-15c/thruster_right.obj" : "vf-15c/thruster_left.obj", trailModel, speedTrailModel, flareModel,
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
    const float placementScaleFactor = playerModelScale_ / kDefaultPlayerModelScale;
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
            baseOffset.x + burnerPlacement.positionOffset.x * placementScaleFactor,
            baseOffset.y + burnerPlacement.positionOffset.y * placementScaleFactor,
            baseOffset.z + burnerPlacement.positionOffset.z * placementScaleFactor
        };
        const Vector3 worldOffset = MyMath::Transform(adjustedOffset, rotationMatrix);
        const Vector3 modelTranslation = MyMath::Transform(
            { adjustedOffset.x - burner.nozzleOriginOffset.x,
                adjustedOffset.y - burner.nozzleOriginOffset.y,
                adjustedOffset.z - burner.nozzleOriginOffset.z }, rotationMatrix);

        if (burner.nozzleObject) {
            burner.nozzleObject->SetTranslate({ position.x + modelTranslation.x,
                position.y + modelTranslation.y, position.z + modelTranslation.z });
            burner.nozzleObject->SetScale({ playerModelScale_ * burnerPlacement.scale.x,
                playerModelScale_ * burnerPlacement.scale.y, playerModelScale_ * burnerPlacement.scale.z });
            burner.nozzleObject->SetQuaternionRotate(nozzleRotation);
        }
        if (burner.positionMarkerObject) {
            burner.positionMarkerObject->SetTranslate({ position.x + worldOffset.x,
                position.y + worldOffset.y, position.z + worldOffset.z });
            burner.positionMarkerObject->SetScale({ 0.045f * placementScaleFactor,
                0.045f * placementScaleFactor, 0.045f * placementScaleFactor });
        }
        // 炎はノズル開口から始め、機体の向きに合わせて排気方向へ伸ばす。
        burner.adjustedExhaustDirection = MyMath::RotateVector(burner.exhaustDirection, placementRotation);
        burner.worldExhaustDirection = MyMath::RotateVector(burner.adjustedExhaustDirection, rotation);
        burner.placementScale = burnerPlacement.scale;
        const Vector3 localExit = {
            adjustedOffset.x + burner.adjustedExhaustDirection.x * playerModelScale_ * kNozzleRearOffsetMultiplier,
            adjustedOffset.y + burner.adjustedExhaustDirection.y * playerModelScale_ * kNozzleRearOffsetMultiplier,
            adjustedOffset.z + burner.adjustedExhaustDirection.z * playerModelScale_ * kNozzleRearOffsetMultiplier
        };
        if (burner.trail && burner.trailObject) {
            burner.trail->SetOrigin(localExit);
            burner.trailObject->SetTranslate(position);
            burner.trailObject->SetScale({ 1.0f, 1.0f, 1.0f });
            burner.trailObject->SetQuaternionRotate(rotation);
        }
        if (burner.speedTrail && burner.speedTrailObject) {
            const Vector3 worldExitOffset = MyMath::Transform(localExit, rotationMatrix);
            const Vector3 worldExit = { position.x + worldExitOffset.x,
                position.y + worldExitOffset.y, position.z + worldExitOffset.z };
            const float placementScale = std::clamp(
                (burner.placementScale.x + burner.placementScale.y + burner.placementScale.z) / 3.0f,
                0.2f, 3.0f);
            const float flameLength = burner.plumeLength * effectSettings_.lengthScale * placementScale;
            // 光跡の現在点・履歴点はともに実際のノズル口を使う。固定炎の途中や
            // 先端を履歴へ足すと、急旋回時に人工的な直線区間が残って直角に見える。
            burner.speedTrailStart = worldExit;
            burner.speedTrailFlareCenter = {
                worldExit.x + burner.worldExhaustDirection.x * flameLength * 0.12f,
                worldExit.y + burner.worldExhaustDirection.y * flameLength * 0.12f,
                worldExit.z + burner.worldExhaustDirection.z * flameLength * 0.12f
            };
            if (!burner.hasSpeedTrailOrigin || burner.speedTrailIntensity <= 0.035f) {
                burner.speedTrail->SetOrigin(worldExit);
                burner.hasSpeedTrailOrigin = true;
            } else {
                burner.speedTrail->Update(worldExit, 1.0f / 60.0f, burner.speedTrailLifetime);
            }
            burner.speedTrailObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
            burner.speedTrailObject->SetScale({ 1.0f, 1.0f, 1.0f });
            burner.speedTrailObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
            if (burner.speedTrailFlareObject) {
                burner.speedTrailFlareObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
                burner.speedTrailFlareObject->SetScale({ 1.0f, 1.0f, 1.0f });
                burner.speedTrailFlareObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
            }
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
    const float targetLength = playerModelScale_ * (9.5f + cappedSpeedRatio * 3.2f)
        * (1.0f + accelerationBlend_ * (effectSettings_.boostLengthMultiplier - 1.0f)) * flicker;
    const float targetRadius = playerModelScale_ * (0.56f + cappedSpeedRatio * 0.16f)
        * (1.0f + accelerationBlend_ * (effectSettings_.boostWidthMultiplier - 1.0f));
    const float targetAlpha = std::clamp(0.40f + cappedSpeedRatio * 0.40f
        + accelerationBlend_ * 0.18f, 0.0f, 0.92f);
    // 参考映像と同じく低速では固定炎だけにし、推進中だけ短い帯を残す。
    const float targetSpeedTrailIntensity = cappedSpeedRatio <= 0.10f ? 0.0f : std::clamp(
        0.16f + cappedSpeedRatio * 0.46f + accelerationBlend_ * 0.30f, 0.0f, 0.86f);
    // 加速時ほど、履歴を空間に長く残す。距離ではなく各点の経過時間で消える。
    const float targetSpeedTrailLifetime = cappedSpeedRatio <= 0.10f ? 0.18f : std::clamp(
        0.30f + cappedSpeedRatio * 0.26f + accelerationBlend_ * 0.20f, 0.18f, 0.76f);
    for (auto& burner : burners_) {
        // 長い白い棒に見えないよう、短い噴射を基準に幅で加速を表す。
        burner.plumeLength = burner.plumeLength > 0.0f
            ? burner.plumeLength + (targetLength - burner.plumeLength) * 0.25f : targetLength;
        burner.plumeRadius = burner.plumeRadius > 0.0f
            ? burner.plumeRadius + (targetRadius - burner.plumeRadius) * 0.25f : targetRadius;
        burner.color.w += (targetAlpha - burner.color.w) * 0.25f;
        burner.speedTrailIntensity += (targetSpeedTrailIntensity - burner.speedTrailIntensity) * 0.20f;
        burner.speedTrailLifetime += (targetSpeedTrailLifetime - burner.speedTrailLifetime) * 0.18f;
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

    // 参考映像の光跡は、寿命付きの外側リボン・内側コア・根元フレアの三層。
    // 固定炎の中から出る順番で先に描き、その上へ立体的な白熱芯を重ねる。
    if (thrusterPlumeEnabled_) {
        object3dCommon->SetSpeedTrailDrawSettings();
        for (auto& burner : burners_) {
            if (!burner.speedTrail || !burner.speedTrailObject || !burner.speedTrailObject->GetModel()
                || burner.speedTrailIntensity <= 0.04f) {
                continue;
            }
            const float plumeScale = std::clamp(
                (burner.placementScale.x + burner.placementScale.y + burner.placementScale.z) / 3.0f,
                0.2f, 3.0f);
            const float trailRadius = burner.plumeRadius * effectSettings_.widthScale * plumeScale;
            if (burner.speedTrailFlareObject && burner.speedTrailFlareObject->GetModel()) {
                const std::vector<VertexData> flareVertices = burner.speedTrail->GenerateThrusterFlareVertices(
                    camera, burner.speedTrailFlareCenter, trailRadius * 1.90f, burner.speedTrailIntensity * 0.82f);
                if (!flareVertices.empty()) {
                    burner.speedTrailFlareObject->GetModel()->SetColor({
                        effectSettings_.brightness, effectSettings_.brightness,
                        effectSettings_.brightness, 1.0f
                    });
                    burner.speedTrailFlareObject->GetModel()->UpdateTrailVertices(flareVertices, true);
                    burner.speedTrailFlareObject->Update(camera);
                    burner.speedTrailFlareObject->Draw();
                }
            }
            const std::vector<VertexData> speedTrailVertices = burner.speedTrail->GenerateSpeedTrailVertices(
                camera, trailRadius,
                burner.speedTrailStart, burner.worldExhaustDirection,
                burner.speedTrailIntensity, burner.speedTrailLifetime);
            if (speedTrailVertices.empty()) continue;
            burner.speedTrailObject->GetModel()->SetColor({
                effectSettings_.brightness, effectSettings_.brightness,
                effectSettings_.brightness, 1.0f
            });
            // V座標をスクロールさせ、空間に残った帯の内部だけを後方へ流す。
            const Vector3 uvScale = { 1.0f, 5.5f, 1.0f };
            const Vector3 uvTranslate = { 0.0f, -time_ * 0.0125f, 0.0f };
            burner.speedTrailObject->GetModel()->SetUvTransform(
                MyMath::MakeAffineMatrix(uvScale, Vector3{ 0.0f, 0.0f, 0.0f }, uvTranslate));
            burner.speedTrailObject->GetModel()->UpdateTrailVertices(speedTrailVertices, true);
            burner.speedTrailObject->Update(camera);
            burner.speedTrailObject->Draw();
        }
    }

    // 白熱芯と外炎も光跡と同じ履歴曲線に沿わせる。これで固定炎だけが直線のまま
    // 残らず、ノズル口から光跡まで一本の曲がる噴射になる。
    if (thrusterPlumeEnabled_) {
        object3dCommon->SetThrusterDrawSettings();
        for (auto& burner : burners_) {
            if (!burner.trail || !burner.trailObject || !burner.trailObject->GetModel()) {
                continue;
            }
            const float plumeScale = std::clamp(
                (burner.placementScale.x + burner.placementScale.y + burner.placementScale.z) / 3.0f,
                0.2f, 3.0f);
            const float coreLength = burner.plumeLength * effectSettings_.lengthScale * plumeScale;
            std::vector<VertexData> coreVertices;
            if (burner.speedTrail) {
                coreVertices = burner.speedTrail->GenerateCurvedThrusterVertices(
                    camera, burner.plumeRadius * effectSettings_.widthScale * plumeScale,
                    burner.speedTrailStart, burner.worldExhaustDirection,
                    coreLength, time_, burner.color.w,
                    effectSettings_.coreLengthRatio, effectSettings_.glowScale);
            }
            if (!coreVertices.empty()) {
                // 曲線炎の頂点はワールド空間で生成済みなので、Object3d 側の変換は
                // 恒等にする。光跡と同じ位置・同じ曲率で重なる。
                burner.trailObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
                burner.trailObject->SetScale({ 1.0f, 1.0f, 1.0f });
                burner.trailObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
            } else {
                // 履歴がまだ2点たまっていない発生直後だけ、従来の短い直線炎を使う。
                coreVertices = burner.trail->GenerateThrusterVertices(
                    camera, burner.plumeRadius * effectSettings_.widthScale * plumeScale,
                    burner.adjustedExhaustDirection,
                    coreLength, time_, burner.color.w,
                    effectSettings_.coreLengthRatio, effectSettings_.glowScale);
                burner.trailObject->SetTranslate(burner.trailObject->GetTranslate());
            }
            burner.trailObject->GetModel()->SetColor({
                effectSettings_.brightness, effectSettings_.brightness,
                effectSettings_.brightness, 1.0f
            });
            burner.trailObject->GetModel()->UpdateTrailVertices(coreVertices, false);
            burner.trailObject->Update(camera);
            burner.trailObject->Draw();
        }
    }

    object3dCommon->SetEffectDrawSettings();
}
