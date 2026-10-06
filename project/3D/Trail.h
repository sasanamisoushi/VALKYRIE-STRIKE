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

    // 毎フレーム、ミサイル（発生源）の現在座標を渡して更新する
    void Update(const Vector3 &currentPos);

    // 機体に固定する噴射口。ワールド座標の移動履歴から独立させる。
    void SetOrigin(const Vector3& localOrigin);

    // カメラの情報を元に、常にカメラの方を向く「板ポリゴンの頂点リスト」を生成する
    std::vector<VertexData> GenerateVertices(Camera *camera, float thickness);

    // 推進炎用。排気方向へ伸びる、視点に依存しない立体チューブを生成する。
    std::vector<VertexData> GenerateThrusterVertices(Camera *camera, float thickness,
        const Vector3& exhaustDirection, float plumeLength, float phase = 0.0f, float intensity = 1.0f,
        float coreLengthRatio = 0.52f, float glowScale = 1.0f);

private:
    // 過去の座標の履歴（追加・削除が早い deque を使います）
    std::deque<Vector3> points_;
    int maxPoints_ = 30;
};

