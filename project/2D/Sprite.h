#pragma once
#include "MyMath.h"
#include <cstdint>
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <memory>


using Microsoft::WRL::ComPtr;


class SpriteCommon;

class Sprite {
public:
	//初期化
	void Initialize(SpriteCommon *spriteCommon, std::string textureFilePath);

	//更新処理
	void Update();

	//描画
	void Draw();

	//テクスチャ差し替え
	void textureReplacement(std::string textureFilePath);

	~Sprite();

	//頂点データ
	struct VertexData {
		Vector4 Position;
		Vector2 Texcoord;
		Vector3 normal;
	};

	//マテリアルデータ
	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
		float shininess;
	};

	//座標変換秒列データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};
	
	struct EulerTransform {
		Vector3 scale;
		Vector3 rotate;
		Vector3 translate;
	};

	//--------getter-------
	//位置
	const Vector2 &GetPosition()const { return position; }
	//回転
	float GetRotation() const { return rotation; }
	//色
	const Vector4 &GetColor()const { return materialData->color; }
	//サイズ
	const Vector2 &GetSize()const { return size; }
	//アンカーポイント
	const Vector2 &GetAnchorPoint()const { return anchorPoint; }
	//左右フリップ
	bool GetIsFlipX()const { return isFlipX_; }
	//上下フリップ
	bool GetIsFlipY()const { return isFlipY_; }
	//テクスチャ左上座標
	const Vector2 &GetTextureLeftTop()const { return textureLeftTop; }
	//テクスチャ切り出しサイズ
	const Vector2 &GetTextureSize()const { return textureSize; }

	//---------setter-----------
	//位置
	void SetPosition(const Vector2 &position) { this->position = position; }
	//回転
	void SetRotation(float rotation) { this->rotation = rotation; }
	//色
	void SetColor(const Vector4 &color) { materialData->color = color; }
	//サイズ
	void SetSize(const Vector2 &size) { this->size = size; }
	//アンカーポイント
	void SetAnchorPoint(const Vector2 &anchorPoint) { this->anchorPoint = anchorPoint; }
	//左右フリップ
	bool SetIsFlipX(bool isFlipX) { this->isFlipX_ = isFlipX; }
	//上下フリップ
	bool SetIsFlipY(bool isFlipY) { this->isFlipY_ = isFlipY; }
	//テクスチャ左上座標
	void SetTextureLeftTop(const Vector2 &textureLeftTop) { this->textureLeftTop = textureLeftTop; }
	//テクスチャ切り出しサイズ
	void SetTextureSize(const Vector2 &textureSize) { this->textureSize = textureSize; }
	// ピクセルシェーダーで適用するUV変換。回転やスクロールをスプライト本体の形状と分離できる。
	void SetUVTransform(const Matrix4x4& uvTransform) { materialData->uvTransform = uvTransform; }

private:
	SpriteCommon *spriteCommon=nullptr;
	//MyMath *math = nullptr;
	std::unique_ptr<MyMath> math;

	// 頂点バッファ
	ComPtr<ID3D12Resource> vertexResource;

	// インデックスバッファ
	ComPtr<ID3D12Resource> indexResource;

	//バッファリソース内のデータを指するポインタ
	VertexData *vertexData_ = nullptr;
	uint32_t *indexData_ = nullptr;

	//バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	//頂点データ作成
	void CreateVertexData();

	//マテリアルリソースを作る
	ComPtr<ID3D12Resource> materialResource;
	//マテリアルにデータを書き込む
	Material *materialData = nullptr;

	//マテリアルデータの作成
	void CreateMaterialData();

	//Sprite用のTransformationMatrix用のリソースをス来る
	ComPtr<ID3D12Resource> transformationMatrixResource;
	//データを書き込む
	TransformationMatrix *transformationMatrixData = nullptr;

	//座標変換行列データ作成
	void CreateTransformationData();

	//位置
	Vector2 position = { 0.0f,0.0f };

	//回転
	float rotation = 0.0f;

	//サイズ
	Vector2 size = { 640.0f,360.0f };

	//テクスチャ番号
	uint32_t textureIndex = 0;

	//アンカーポイント
	Vector2 anchorPoint = { 0.0f,0.0f };

	//左右フリップ
	bool isFlipX_ = false;

	//上下フリップ
	bool isFlipY_ = false;

	//テクスチャ左上座標
	Vector2 textureLeftTop = { 0.0f,0.0f };

	//テクスチャ切り出しサイズ
	Vector2 textureSize = { 60.0f,60.0f };

	//テクスチャサイズをイメージに合わせる
	void AdjustTextureSize();

	// テクスチャファイルパス
	std::string textureFilePath_;

};

