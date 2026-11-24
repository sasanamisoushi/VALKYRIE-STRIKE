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
    // 基本の左／右スラスターごとの補正。左右共通の PlacementSettings に加算して使う。
    struct BaseThrusterSettings {
        Vector3 positionOffset = { 0.0f, 0.0f, 0.0f };
        Vector3 scale = { 1.0f, 1.0f, 1.0f };
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
    // localMovementIntent は機体ローカルの移動入力。ガウォーク時は使用するノズルを切り替える。
    void Update(const Vector3& position, const Quaternion& rotation, int playerMode, float speedRatio,
        bool isAccelerating, const Vector3& localMovementIntent = { 0.0f, 0.0f, 0.0f },
        bool showAllGerwalkThrusters = false);
    void UpdateEditorPreview(const Vector3& position, const Quaternion& rotation, int playerMode);
    void DrawNozzles(Camera* camera = nullptr);
    void Draw(Camera* camera);
    const PlacementSettings& GetPlacementSettings(int playerMode) const;
    void SetPlacementSettings(int playerMode, const PlacementSettings& settings);
    const BaseThrusterSettings& GetBaseThrusterSettings(int playerMode, size_t index) const;
    bool SetBaseThrusterSettings(int playerMode, size_t index, const BaseThrusterSettings& settings);
    size_t GetBaseThrusterCount(int playerMode) const;
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
    void SetThrusterPlumeEnabled(bool enabled) { thrusterPlumeEnabled_ = enabled; }
    bool IsThrusterPlumeEnabled() const { return thrusterPlumeEnabled_; }
    const EffectSettings& GetEffectSettings() const { return effectSettings_; }
    // 自機モデルと同じ縮尺でノズル・炎・光跡の取付座標を計算する。
    void SetPlayerModelScale(float scale);
    void SetEffectSettings(const EffectSettings& settings);
    void ResetEffectSettings();
    bool SaveSettings(const std::string& filePath) const;
    bool LoadSettings(const std::string& filePath);

private:
    enum class GerwalkThrusterRole {
        None,
        Upper,
        Lower,
    };

    struct Burner {
        std::unique_ptr<Trail> trail;
        std::unique_ptr<Object3d> trailObject;
        // 固定炎の末端から続く、短時間だけ残る平面発光帯。
        std::unique_ptr<Trail> speedTrail;
        std::unique_ptr<Object3d> speedTrailObject;
        // ノズル根元の境目を消す、カメラ正対の加算フレア。
        std::unique_ptr<Object3d> speedTrailFlareObject;
        // 左右別の実モデルを機体へ固定し、その後端から噴射する。
        std::unique_ptr<Object3d> nozzleObject;
        std::unique_ptr<Object3d> positionMarkerObject;
        Vector3 offset;
        Vector3 nozzleOriginOffset;
        Vector3 exhaustDirection;
        Vector3 adjustedExhaustDirection;
        Vector3 worldExhaustDirection = { 0.0f, 0.0f, -1.0f };
        Vector3 placementScale = { 1.0f, 1.0f, 1.0f };
        size_t baseThrusterIndex = 0;
        GerwalkThrusterRole gerwalkRole = GerwalkThrusterRole::None;
        bool isExtraThruster = false;
        size_t extraThrusterIndex = 0;
        Vector4 color;
        float plumeLength = 0.0f;
        float plumeRadius = 0.0f;
        float thrustIntensity = 1.0f;
        float speedTrailIntensity = 0.0f;
        float speedTrailLifetime = 0.55f;
        Vector3 speedTrailStart = { 0.0f, 0.0f, 0.0f };
        Vector3 speedTrailFlareCenter = { 0.0f, 0.0f, 0.0f };
        bool hasSpeedTrailOrigin = false;
    };

    std::vector<Burner> burners_;
    std::array<PlacementSettings, 3> placementSettings_{};
    // 基本スラスターは各形態で左右2基。ガウォークの上側は追加ノズルとして扱う。
    std::array<std::array<BaseThrusterSettings, 2>, 3> baseThrusterSettingsByMode_{};
    std::array<std::vector<ExtraThrusterSettings>, 3> extraThrustersByMode_{};
    EffectSettings effectSettings_;
    bool showPlacementMarkers_ = false;
    bool editorPreviewAccelerating_ = true;
    bool thrusterPlumeEnabled_ = true;
    float accelerationBlend_ = 0.0f;
    float playerModelScale_ = 0.08f;
    int lastMode_ = -1;
    float time_ = 0.0f;
    Vector3 localMovementIntent_ = { 0.0f, 0.0f, 0.0f };
    bool showAllGerwalkThrusters_ = false;

    static size_t ToModeIndex(int playerMode);
    void SetupBurnersForMode(int playerMode);
};
