#pragma once
#include <vector>
#include <deque>
#include "engine/math/MyMath.h"
#include "engine/Camera/Camera.h"
#include "3D/Model.h"

class Trail {
public:
    // maxPoints: 軌跡の長さ（何フレーム分の過去座標を保持するか）
    void Initialize(int maxPoints = 30);

    // 毎フレーム、発生源の現在座標と寿命を渡して更新する。
    // 各点はワールド空間に残り、寿命を過ぎた点だけが消える。履歴点は
    // フレーム数ではなく移動距離ごとに追加するため、速度やFPSで密度が崩れない。
    void Update(const Vector3& currentPos, float deltaTime = 1.0f / 60.0f,
        float lifeTime = 0.55f);

    // 機体に固定する噴射口。ワールド座標の移動履歴から独立させる。
    void SetOrigin(const Vector3& localOrigin);

    // カメラの情報を元に、常にカメラの方を向く「板ポリゴンの頂点リスト」を生成する
    std::vector<VertexData> GenerateVertices(Camera *camera, float thickness);

    // 短い移動履歴を、スラスター炎の末端から連続する発光帯として生成する。
    // 立体炎とは別の平面レイヤーで、帯の幅と色は専用シェーダーで滑らかに落とす。
    std::vector<VertexData> GenerateSpeedTrailVertices(Camera* camera, float thickness,
        const Vector3& fixedFlameStart, const Vector3& exhaustDirection,
        float intensity, float lifeTime);

    // 固定炎の代わりに、光跡と同じ空間曲線へ沿わせる立体的な噴射炎を生成する。
    std::vector<VertexData> GenerateCurvedThrusterVertices(Camera* camera, float thickness,
        const Vector3& nozzlePosition, const Vector3& exhaustDirection, float plumeLength,
        float phase, float intensity, float coreLengthRatio, float glowScale) const;

    // ノズル根元に重ねる、カメラ正対の加算フレアを生成する。
    std::vector<VertexData> GenerateThrusterFlareVertices(Camera* camera,
        const Vector3& center, float radius, float intensity) const;

    // 推進炎用。ノズルから一体で伸びる、視点側へ扁平な発光メッシュを生成する。
    std::vector<VertexData> GenerateThrusterVertices(Camera *camera, float thickness,
        const Vector3& exhaustDirection, float plumeLength, float phase = 0.0f, float intensity = 1.0f,
        float coreLengthRatio = 0.52f, float glowScale = 1.0f);

private:
    struct TrailPoint {
        Vector3 position;
        float age = 0.0f;
    };

    // リボンと立体炎で必ず同じ経路を使うための、寿命付き曲線サンプル。
    std::vector<TrailPoint> BuildSpeedTrailCurve(const Vector3& nozzlePosition,
        const Vector3& exhaustDirection, float lifeTime) const;

    // 過去のワールド座標と生成からの経過時間。フレーム数ではなく寿命で消す。
    std::deque<TrailPoint> points_;
    int maxPoints_ = 30;
    float lifeTime_ = 0.55f;
    // 現在のノズル先端は履歴点とは別に常に保持する。これで点を間引いても、
    // 光跡の先頭が固定炎から遅れて離れることはない。
    Vector3 headPosition_ = { 0.0f, 0.0f, 0.0f };
    bool hasHeadPosition_ = false;
    float distanceSinceLastSample_ = 0.0f;
    static constexpr float kSampleSpacing_ = 0.12f;
};

