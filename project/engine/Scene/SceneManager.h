#pragma once
#include "Game/base/BaseScene.h"
#include "Game/base/AbstractSceneFactory.h"
#include <memory>
#include <string>

class SceneManager {
public:
	//シングストンインスタンスの取得
	static SceneManager *GetInstance();

	//ゲーム終了時の処理
	void Finalize();

	//ファクトリーのセット
	void SetSceneFactory(AbstractSceneFactory* sceneFactory){ sceneFactory_ = sceneFactory; }

	//シーンの切り替え予約
	void ChangeScene(const std::string &sceneName);
	// 初期化済みのシーンを、次の画面で使うまで保持する。
	// タイトル背景で読み込み済みの本編をワープ画面へ渡すために使う。
	void PrepareScene(std::unique_ptr<BaseScene> scene);
	std::unique_ptr<BaseScene> TakePreparedScene();
	void DiscardPreparedScene();

	// 初期化済みのシーンへ切り替える。Initialize() を再実行しない。
	void ChangeToInitializedScene(std::unique_ptr<BaseScene> scene);

	//毎フレームの更新処理
	void Update();

	//描画処理
	void Draw();
private:
	SceneManager() = default;
	~SceneManager() = default;
	SceneManager(const SceneManager &) = delete;
	SceneManager operator=(const SceneManager &) = delete;

	//現在のシーン
	std::unique_ptr<BaseScene> currentScene_=nullptr;
	//次のシーン
	std::unique_ptr<BaseScene> nextScene_ = nullptr;
	bool nextSceneIsInitialized_ = false;
	// ワープ中に保持する、すでに初期化済みの本編シーン。
	std::unique_ptr<BaseScene> preparedScene_ = nullptr;

	//シーンファクトリー
	AbstractSceneFactory *sceneFactory_ = nullptr;
}; 

