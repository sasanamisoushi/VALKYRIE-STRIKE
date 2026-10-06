#pragma once
#include "3D/Object3d.h"
#include "3D/Trail.h"
#include "engine/math/MyMath.h"
#include "engine/Camera/Camera.h"
#include <memory>
#include <vector>
#include <string>

class BoosterEffect {
public:
    struct EffectSettings {
        float widthScale = 1.0f;
        float lengthScale = 1.0f;
        float brightness = 1.0f;
        float glowScale = 1.0f;
        float coreLengthRatio = 0.52f;
        float boostWidthMultiplier = 1.25f;
        float boostLengthMultiplier = 1.18f;
    };
    void Initialize();
    void Update(const Vector3& position, const Quaternion& rotation, int playerMode, float speedRatio, bool isAccelerating);
    void UpdateEditorPreview(const Vector3& position, const Quaternion& rotation, int playerMode);
    void DrawNozzles(Camera* camera = nullptr);
    void Draw(Camera* camera);
    Vector3& GetPlacementAdjustment() { return placementAdjustment_; }
    void ResetPlacementAdjustment() { placementAdjustment_ = { 0.0f, 0.0f, 0.0f }; }
    void RefreshPlacement(const Vector3& position, const Quaternion& rotation, int playerMode);
    void SetPlacementMarkersVisible(bool visible) { showPlacementMarkers_ = visible; }
    bool GetEditorPreviewAcceleration() const { return editorPreviewAccelerating_; }
    void SetEditorPreviewAcceleration(bool accelerating) { editorPreviewAccelerating_ = accelerating; }
    EffectSettings& GetEffectSettings() { return effectSettings_; }
    void ResetEffectSettings() { effectSettings_ = EffectSettings{}; }
    bool SaveSettings(const std::string& filePath) const;
    bool LoadSettings(const std::string& filePath);

private:
    struct Burner {
        std::unique_ptr<Trail> trail;
        std::unique_ptr<Object3d> trailObject;
        // 左右別の実モデルを機体へ固定し、その後端から噴射する。
        std::unique_ptr<Object3d> nozzleObject;
        std::unique_ptr<Object3d> positionMarkerObject;
        Vector3 offset;
        Vector3 nozzleOriginOffset;
        Vector3 exhaustDirection;
        Vector4 color;
        float plumeLength = 0.0f;
        float plumeRadius = 0.0f;
    };

    std::vector<Burner> burners_;
    Vector3 placementAdjustment_ = { 0.0f, 0.0f, 0.0f };
    EffectSettings effectSettings_;
    bool showPlacementMarkers_ = false;
    bool editorPreviewAccelerating_ = true;
    float accelerationBlend_ = 0.0f;
    int lastMode_ = -1;
    float time_ = 0.0f;

    void SetupBurnersForMode(int playerMode);
};
