#include "SceneManager.h"
#include <cassert>

SceneManager *SceneManager::GetInstance() {
	static SceneManager instance;
	return &instance;
}

void SceneManager::Finalize() {
	if (currentScene_) {
		currentScene_->Finalize();
		currentScene_.reset();
	}
	DiscardPreparedScene();
}

void SceneManager::ChangeScene(const std::string &sceneName) {
	// ファクトリーがセットされていなければエラー
	assert(sceneFactory_ != nullptr && "SceneFactory is not set!");

	nextScene_ = sceneFactory_->CreateScene(sceneName);
	nextSceneIsInitialized_ = false;
}

void SceneManager::PrepareScene(std::unique_ptr<BaseScene> scene) {
	if (preparedScene_) {
		preparedScene_->Finalize();
	}
	preparedScene_ = std::move(scene);
}

std::unique_ptr<BaseScene> SceneManager::TakePreparedScene() {
	return std::move(preparedScene_);
}

void SceneManager::DiscardPreparedScene() {
	if (preparedScene_) {
		preparedScene_->Finalize();
		preparedScene_.reset();
	}
}

void SceneManager::ChangeToInitializedScene(std::unique_ptr<BaseScene> scene) {
	assert(scene != nullptr && "Initialized scene must not be null!");
	nextScene_ = std::move(scene);
	nextSceneIsInitialized_ = true;
}

void SceneManager::Update() {
	//次のシーンを予約
	if (nextScene_) {
		//現在のシーンがあれば終了処理を呼ぶ
		if (currentScene_) {
			currentScene_->Finalize();
		}

		//シーンを移行する
		currentScene_ = std::move(nextScene_);

		// 初期化済みの本編をワープ画面から引き渡す場合は、再初期化しない。
		if (!nextSceneIsInitialized_) {
			currentScene_->Initialize();
		}
		nextSceneIsInitialized_ = false;
	}

	//現在のシーンの更新処理
	if (currentScene_) {
		currentScene_->Update();
	}
}

void SceneManager::Draw() {
	//現在のシーンの描画処理
	if (currentScene_) {
		currentScene_->Draw();
	}
}
