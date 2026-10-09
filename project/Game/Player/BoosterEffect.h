#pragma once
#include "3D/Object3d.h"
#include "3D/Trail.h"
#include "engine/math/MyMath.h"
#include "engine/Camera/Camera.h"
#include <memory>
#include <vector>
#include <string>
#include <array>

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
    struct PlacementSettings {
        Vector3 positionOffset = { 0.0f, 0.0f, 0.0f };
        Vector3 scale = { 1.0f, 1.0f, 1.0f };
        // 機体ローカル座標系での回転（度）。UI上で編集し、保存も行う。
        Vector3 rotationDegrees = { 0.0f, 0.0f, 0.0f };
    };
    // 既定の左右ノズルとは別に、編集画面から追加できるノズル1基分の設定。
    // 座標は機体ローカル座標（メートル）、回転は度で保存する。
    struct ExtraThrusterSettings {
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 scale = { 1.0f, 1.0f, 1.0f };
        Vector3 rotationDegrees = { 0.0f, 0.0f, 0.0f };
    };
    void Initialize();
    void Update(const Vector3& position, const Quaternion& rotation, int playerMode, float speedRatio, bool isAccelerating);
    void UpdateEditorPreview(const Vector3& position, const Quaternion& rotation, int playerMode);
    void DrawNozzles(Camera* camera = nullptr);
    void Draw(Camera* camera);
    const PlacementSettings& GetPlacementSettings(int playerMode) const;
    void SetPlacementSettings(int playerMode, const PlacementSettings& settings);
    Vector3 GetPlacementAdjustment(int playerMode = 0) const;
    void ResetPlacementAdjustment(int playerMode = 0);
    const std::vector<ExtraThrusterSettings>& GetExtraThrusters(int playerMode) const;
    bool SetExtraThrusterSettings(int playerMode, size_t index, const ExtraThrusterSettings& settings);
    bool AddExtraThruster(int playerMode);
    bool RemoveExtraThruster(int playerMode, size_t index);
    void RefreshPlacement(const Vector3& position, const Quaternion& rotation, int playerMode);
    void SetPlacementMarkersVisible(bool visible) { showPlacementMarkers_ = visible; }
    bool GetEditorPreviewAcceleration() const { return editorPreviewAccelerating_; }
    void SetEditorPreviewAcceleration(bool accelerating) { editorPreviewAccelerating_ = accelerating; }
    const EffectSettings& GetEffectSettings() const { return effectSettings_; }
    void SetEffectSettings(const EffectSettings& settings);
    void ResetEffectSettings();
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
        Vector3 adjustedExhaustDirection;
        Vector3 placementScale = { 1.0f, 1.0f, 1.0f };
        bool isExtraThruster = false;
        size_t extraThrusterIndex = 0;
        Vector4 color;
        float plumeLength = 0.0f;
        float plumeRadius = 0.0f;
    };

    std::vector<Burner> burners_;
    std::array<PlacementSettings, 3> placementSettings_{};
    std::array<std::vector<ExtraThrusterSettings>, 3> extraThrustersByMode_{};
    EffectSettings effectSettings_;
    bool showPlacementMarkers_ = false;
    bool editorPreviewAccelerating_ = true;
    float accelerationBlend_ = 0.0f;
    int lastMode_ = -1;
    float time_ = 0.0f;

    static size_t ToModeIndex(int playerMode);
    void SetupBurnersForMode(int playerMode);
};
