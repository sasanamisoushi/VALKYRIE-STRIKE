#include "StageEditorPanel.h"

#include "Game/enemy/Enemy.h"
#include "Game/obstacle/Obstacle.h"
#include "SimulationManager.h"
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/Utility/StageLoader.h"
#include "engine/Utility/StageValidation.h"
#include "engine/math/MyMath.h"

#include <Windows.h>
#include <externals/imgui/imgui.h>
#include <filesystem>
#include <shellapi.h>

#if defined(ENABLE_IMGUI)
namespace {
	std::filesystem::path FindStageBlendPath() {
		const std::filesystem::path relativeBlendPath = L"resources\\level_editor\\stage.blend";
		std::vector<std::filesystem::path> searchStarts;

		std::error_code ec;
		const std::filesystem::path currentPath = std::filesystem::current_path(ec);
		if (!ec) {
			searchStarts.push_back(currentPath);
		}

		wchar_t modulePath[MAX_PATH] = {};
		const DWORD modulePathLength = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (modulePathLength > 0) {
			searchStarts.push_back(std::filesystem::path(modulePath).parent_path());
		}

		for (const std::filesystem::path& startPath : searchStarts) {
			std::filesystem::path cursor = startPath;
			while (!cursor.empty()) {
				const std::filesystem::path candidates[] = {
					cursor / relativeBlendPath,
					cursor / L"project" / relativeBlendPath
				};

				for (const std::filesystem::path& candidate : candidates) {
					if (std::filesystem::exists(candidate)) {
						return std::filesystem::absolute(candidate);
					}
				}

				if (cursor == cursor.root_path()) {
					break;
				}
				cursor = cursor.parent_path();
			}
		}
		return {};
	}

	bool OpenStageBlendFile(std::string& outMessage) {
		const std::filesystem::path blendPath = FindStageBlendPath();
		if (blendPath.empty()) {
			outMessage = "stage.blend が見つかりません。";
			OutputDebugStringA("[OpenBlender] resources/level_editor/stage.blend not found.\n");
			return false;
		}

		HINSTANCE result = ShellExecuteW(
			nullptr, L"open", blendPath.c_str(), nullptr, blendPath.parent_path().c_str(), SW_SHOWNORMAL);
		if (reinterpret_cast<intptr_t>(result) <= 32) {
			result = ShellExecuteW(
				nullptr, L"open", L"blender.exe", blendPath.c_str(), blendPath.parent_path().c_str(), SW_SHOWNORMAL);
		}

		if (reinterpret_cast<intptr_t>(result) <= 32) {
			outMessage = "Blender を起動できませんでした。関連付けまたは PATH を確認してください。";
			OutputDebugStringW((L"[OpenBlender] Failed: " + blendPath.wstring() + L"\n").c_str());
			return false;
		}

		outMessage = "Blender を起動しました。";
		OutputDebugStringW((L"[OpenBlender] Opened: " + blendPath.wstring() + L"\n").c_str());
		return true;
	}

	std::filesystem::path FindProjectRoot() {
		wchar_t modulePath[MAX_PATH] = {};
		if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
			return {};
		}

		std::filesystem::path cursor = std::filesystem::path(modulePath).parent_path();
		while (!cursor.empty()) {
			if (std::filesystem::exists(cursor / "project" / "CG2.sln") &&
				std::filesystem::exists(cursor / "tools" / "package_release.ps1")) {
				return cursor;
			}
			const std::filesystem::path parent = cursor.parent_path();
			if (parent == cursor) {
				break;
			}
			cursor = parent;
		}
		return {};
	}

	std::filesystem::path GetExecutablePackageZipPath() {
		const std::filesystem::path projectRoot = FindProjectRoot();
		return projectRoot.empty() ? std::filesystem::path{} :
			projectRoot / "generated" / "packages" / "ValkyrieStrike_Playable-Release.zip";
	}

	bool CreateExecutablePackage(bool skipBuild, bool includeDemoPlay, bool includeClearCinematicDemo, HANDLE& processHandle) {
		const std::filesystem::path projectRoot = FindProjectRoot();
		if (projectRoot.empty()) {
			return false;
		}

		const std::filesystem::path scriptPath = projectRoot / "tools" / "package_release.ps1";
		std::wstring parameters = L"-NoProfile -ExecutionPolicy Bypass -File \"" + scriptPath.wstring() +
			L"\" -Configuration Release";
		if (skipBuild) {
			parameters += L" -SkipBuild";
		}
		if (includeDemoPlay) {
			parameters += L" -IncludeDemoPlay";
		}
		if (includeClearCinematicDemo) {
			parameters += L" -IncludeClearCinematicDemo";
		}

		SHELLEXECUTEINFOW executeInfo{};
		executeInfo.cbSize = sizeof(executeInfo);
		executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		executeInfo.lpVerb = L"open";
		executeInfo.lpFile = L"powershell.exe";
		executeInfo.lpParameters = parameters.c_str();
		executeInfo.lpDirectory = projectRoot.c_str();
		executeInfo.nShow = SW_SHOWNORMAL;
		if (!ShellExecuteExW(&executeInfo) || executeInfo.hProcess == nullptr) {
			return false;
		}

		processHandle = executeInfo.hProcess;
		return true;
	}

	void UpdateExecutablePackageStatus(HANDLE& processHandle, const std::filesystem::path& zipPath,
		std::string& packageMessage) {
		if (processHandle == nullptr || WaitForSingleObject(processHandle, 0) == WAIT_TIMEOUT) {
			return;
		}

		DWORD exitCode = 1;
		GetExitCodeProcess(processHandle, &exitCode);
		CloseHandle(processHandle);
		processHandle = nullptr;
		packageMessage = exitCode == 0 && std::filesystem::exists(zipPath)
			? "ZIP作成が完成しました！ generated/packages/ValkyrieStrike_Playable-Release.zip"
			: "ZIP作成に失敗しました。PowerShellのエラー内容を確認してください。";
	}

	bool IsCurrentExecutableReleaseBuild() {
		wchar_t modulePath[MAX_PATH] = {};
		if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
			return false;
		}
		const std::filesystem::path projectRoot = FindProjectRoot();
		if (projectRoot.empty()) {
			return false;
		}
		return std::filesystem::path(modulePath).lexically_normal() ==
			(projectRoot / "generated" / "outputs" / "Release" / "ValkyrieStrike.exe").lexically_normal();
	}

#if defined(CG2_ENABLE_STAGE_VALIDATION)
	ImVec4 ValidationLevelColor(const std::string& level) {
		if (level == "ERROR") return ImVec4(1.0f, 0.25f, 0.2f, 1.0f);
		if (level == "WARNING") return ImVec4(1.0f, 0.75f, 0.2f, 1.0f);
		return ImVec4(0.35f, 1.0f, 0.45f, 1.0f);
	}

	const char* ValidationLevelText(const std::string& level) {
		if (level == "ERROR") return "ERROR";
		if (level == "WARNING") return "WARNING";
		return "OK";
	}

	void DrawValidationMessages(const char* title, const std::vector<std::string>& messages, const ImVec4& color) {
		ImGui::TextColored(color, "%s: %d", title, static_cast<int>(messages.size()));
		for (const std::string& message : messages) {
			ImGui::BulletText("%s", message.c_str());
		}
	}

	void DrawStageValidationWindow(const StageValidation::Report& report) {
		ImGui::Begin("Level Validation");
		ImGui::Text("Source: %s", report.source.c_str());

		DrawValidationMessages("Errors", report.errors, ValidationLevelColor("ERROR"));
		DrawValidationMessages("Warnings", report.warnings, ValidationLevelColor("WARNING"));

		ImGui::Separator();
		ImGui::Text("Check Items");
		for (const StageValidation::CheckItemResult& item : report.checkItems) {
			ImGui::TextColored(ValidationLevelColor(item.level), "%s", ValidationLevelText(item.level));
			ImGui::SameLine(90.0f);
			if (item.detail.empty()) {
				ImGui::TextWrapped("%s", item.label.c_str());
			} else {
				ImGui::TextWrapped("%s: %s", item.label.c_str(), item.detail.c_str());
			}
		}

		ImGui::End();
	}

	struct ValidationLabelRect {
		ImVec2 min;
		ImVec2 max;
	};

	float ClampValidationFloat(float value, float minValue, float maxValue) {
		if (maxValue < minValue) return minValue;
		if (value < minValue) return minValue;
		if (value > maxValue) return maxValue;
		return value;
	}

	ValidationLabelRect MakeValidationLabelRect(const ImVec2& textPos, const ImVec2& textSize, float padding) {
		return {
			ImVec2(textPos.x - padding, textPos.y - padding),
			ImVec2(textPos.x + textSize.x + padding, textPos.y + textSize.y + padding),
		};
	}

	bool ValidationRectsOverlap(const ValidationLabelRect& lhs, const ValidationLabelRect& rhs) {
		return lhs.min.x < rhs.max.x && lhs.max.x > rhs.min.x &&
			lhs.min.y < rhs.max.y && lhs.max.y > rhs.min.y;
	}

	bool ValidationRectOverlapsAny(const ValidationLabelRect& rect, const std::vector<ValidationLabelRect>& usedRects) {
		for (const ValidationLabelRect& usedRect : usedRects) {
			if (ValidationRectsOverlap(rect, usedRect)) return true;
		}
		return false;
	}

	void ClampValidationLabelToGameView(
		ImVec2& textPos, const ImVec2& textSize, float padding,
		float minX, float minY, float maxX, float maxY) {
		textPos.x = ClampValidationFloat(textPos.x, minX + padding, maxX - textSize.x - padding);
		textPos.y = ClampValidationFloat(textPos.y, minY + padding, maxY - textSize.y - padding);
	}

	ImVec2 ResolveValidationLabelPosition(
		const ImVec2& anchor, const ImVec2& textSize, float padding,
		float minX, float minY, float maxX, float maxY,
		std::vector<ValidationLabelRect>& usedRects) {
		const float rectWidth = textSize.x + padding * 2.0f;
		const float rectHeight = textSize.y + padding * 2.0f;
		const float verticalStep = rectHeight + 4.0f;
		const float horizontalStep = rectWidth * 0.55f;
		const int horizontalSlots[] = { 0, -1, 1, -2, 2 };

		for (int stackIndex = 0; stackIndex < 10; ++stackIndex) {
			for (int side = 0; side < 2; ++side) {
				for (int horizontalSlot : horizontalSlots) {
					const float x = anchor.x - textSize.x * 0.5f + horizontalStep * static_cast<float>(horizontalSlot);
					const float y = (side == 0)
						? anchor.y - textSize.y - 22.0f - verticalStep * static_cast<float>(stackIndex)
						: anchor.y + 16.0f + verticalStep * static_cast<float>(stackIndex);
					ImVec2 candidate(x, y);
					ClampValidationLabelToGameView(candidate, textSize, padding, minX, minY, maxX, maxY);

					const ValidationLabelRect rect = MakeValidationLabelRect(candidate, textSize, padding);
					if (!ValidationRectOverlapsAny(rect, usedRects)) {
						usedRects.push_back(rect);
						return candidate;
					}
				}
			}
		}

		ImVec2 fallback(anchor.x - textSize.x * 0.5f, anchor.y - textSize.y - 22.0f);
		ClampValidationLabelToGameView(fallback, textSize, padding, minX, minY, maxX, maxY);
		usedRects.push_back(MakeValidationLabelRect(fallback, textSize, padding));
		return fallback;
	}

	void DrawStageValidationOverlay(const StageValidation::Report& report, const Matrix4x4& viewProjectionMatrix) {
		if (!report.HasMarkers()) return;

		float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
		if (!FlyCamera::GetGameViewBounds(minX, minY, maxX, maxY)) return;

		const float width = maxX - minX;
		const float height = maxY - minY;
		if (!ImGui::GetCurrentContext()) return;
		ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
		std::vector<ValidationLabelRect> usedRects;

		for (const StageValidation::ValidationMarker& marker : report.markers) {
			const Vector3 worldPosition = {
				static_cast<float>(marker.x),
				static_cast<float>(marker.y) + 1.2f,
				static_cast<float>(marker.z),
			};
			const Vector3 screenPosition = MyMath::WorldToScreen(worldPosition, viewProjectionMatrix, width, height);
			if (screenPosition.z < 0.0f || screenPosition.z > 1.0f) continue;
			if (screenPosition.x < 0.0f || screenPosition.x > width ||
				screenPosition.y < 0.0f || screenPosition.y > height) continue;

			const bool isError = marker.level == "ERROR";
			const std::string text = std::string(isError ? "ERROR: " : "WARNING: ") + marker.message;
			const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
			const float padding = 4.0f;
			const ImVec2 anchor(minX + screenPosition.x, minY + screenPosition.y);
			const ImVec2 textPos = ResolveValidationLabelPosition(anchor, textSize, padding, minX, minY, maxX, maxY, usedRects);
			const ValidationLabelRect rect = MakeValidationLabelRect(textPos, textSize, padding);

			const ImU32 fillColor = isError ? IM_COL32(90, 16, 16, 220) : IM_COL32(92, 63, 0, 220);
			const ImU32 borderColor = isError ? IM_COL32(255, 75, 55, 255) : IM_COL32(255, 200, 45, 255);
			drawList->AddRectFilled(rect.min, rect.max, fillColor, 3.0f);
			drawList->AddRect(rect.min, rect.max, borderColor, 3.0f);
			drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), text.c_str());
		}
	}
#endif
}
#endif

void StageEditorPanel::DrawEnemyAndObstaclePanel(const StagePlacementPanelContext& context) const {
#if defined(ENABLE_IMGUI)
	if (!context.blenderEnemyName || !context.blenderEnemyTypeIndex || !context.blenderEnemyPosition ||
		!context.blenderEnemyRotationDegrees || !context.enemyMousePlacementEnabled ||
		!context.enemyMousePlacementHeight || !context.enemySpawns || !context.enemies ||
		!context.obstacles || !context.simulationManager) {
		return;
	}

	ImGui::Text("Blender互換の敵配置");
	ImGui::TextWrapped("ここで追加・編集した敵は Blender と共通の scene.json に保存されます。座標はゲーム座標で入力し、保存時に Blender 座標へ自動変換します。");
	ImGui::Separator();

	const char* enemyTypes[] = { "VF1", "VF1-1", "VF3", "Jammer", "Boss" };
	ImGui::InputText("敵の名前", context.blenderEnemyName, context.blenderEnemyNameSize);
	ImGui::Combo("敵タイプ", context.blenderEnemyTypeIndex, enemyTypes, IM_ARRAYSIZE(enemyTypes));
	ImGui::DragFloat3("配置座標 (Game X,Y,Z)", context.blenderEnemyPosition, 0.1f);
	ImGui::DragFloat3("回転 (度)", context.blenderEnemyRotationDegrees, 1.0f);
	if (ImGui::Checkbox("Game View をクリックして敵を配置", context.enemyMousePlacementEnabled) &&
		*context.enemyMousePlacementEnabled && context.setDebugCameraActive) {
		context.setDebugCameraActive(true);
	}
	if (*context.enemyMousePlacementEnabled) {
		ImGui::DragFloat("クリック配置の高さ (Y)", context.enemyMousePlacementHeight, 0.1f, -100.0f, 300.0f);
		ImGui::TextColored(ImVec4(0.35f, 0.9f, 1.0f, 1.0f), "フリーカメラで見ながら Game View を左クリックすると敵を配置します。");
	}

	if (ImGui::Button("敵を配置して scene.json に保存")) {
		const float degreesToRadians = 3.141592654f / 180.0f;
		const Vector3 position = { context.blenderEnemyPosition[0], context.blenderEnemyPosition[1], context.blenderEnemyPosition[2] };
		const Vector3 rotation = {
			context.blenderEnemyRotationDegrees[0] * degreesToRadians,
			context.blenderEnemyRotationDegrees[1] * degreesToRadians,
			context.blenderEnemyRotationDegrees[2] * degreesToRadians,
		};
		context.simulationManager->AddEnemySpawnToSceneJson(
			"resources/scene.json", context.blenderEnemyName, enemyTypes[*context.blenderEnemyTypeIndex], position, rotation);
	}

	if (context.simulationSaveMessage && !context.simulationSaveMessage->empty()) {
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", context.simulationSaveMessage->c_str());
	}

	ImGui::Separator();
	ImGui::Text("配置済みの敵 (総数: %d)", static_cast<int>(context.enemySpawns->size()));
	int spawnOrderSource = -1;
	int spawnOrderTarget = -1;
	for (size_t spawnIndex = 0; spawnIndex < context.enemySpawns->size(); ++spawnIndex) {
		EnemySpawnData& spawn = (*context.enemySpawns)[spawnIndex];
		ImGui::PushID(static_cast<int>(spawnIndex));
		if (ImGui::TreeNode(spawn.name.c_str())) {
			ImGui::Text("初期スポーン順: %d", static_cast<int>(spawnIndex + 1));
			if (spawnIndex > 0 && ImGui::SmallButton("上へ")) {
				spawnOrderSource = static_cast<int>(spawnIndex);
				spawnOrderTarget = static_cast<int>(spawnIndex - 1);
			}
			ImGui::SameLine();
			if (spawnIndex + 1 < context.enemySpawns->size() && ImGui::SmallButton("下へ")) {
				spawnOrderSource = static_cast<int>(spawnIndex);
				spawnOrderTarget = static_cast<int>(spawnIndex + 1);
			}
			float position[3] = { spawn.position.x, spawn.position.y, spawn.position.z };
			float rotationDegrees[3] = {
				spawn.rotation.x * 180.0f / 3.141592654f,
				spawn.rotation.y * 180.0f / 3.141592654f,
				spawn.rotation.z * 180.0f / 3.141592654f,
			};
			bool changed = ImGui::DragFloat3("座標 (Game X,Y,Z)", position, 0.1f);
			changed |= ImGui::DragFloat3("回転 (度)", rotationDegrees, 1.0f);
			if (changed) {
				spawn.position = { position[0], position[1], position[2] };
				const float degreesToRadians = 3.141592654f / 180.0f;
				spawn.rotation = {
					rotationDegrees[0] * degreesToRadians,
					rotationDegrees[1] * degreesToRadians,
					rotationDegrees[2] * degreesToRadians,
				};
				for (const auto& enemy : *context.enemies) {
					if (enemy && enemy->GetSpawnPointIndex() == spawnIndex) {
						enemy->SetPosition(spawn.position);
						enemy->SetRotation(spawn.rotation);
						break;
					}
				}
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
		if (spawnOrderTarget >= 0) break;
	}

	if (spawnOrderSource >= 0 && spawnOrderTarget >= 0) {
		context.simulationManager->MoveEnemySpawnInSceneJson(
			"resources/scene.json", static_cast<size_t>(spawnOrderSource), static_cast<size_t>(spawnOrderTarget));
	}

	if (ImGui::Button("配置済みの敵の変更を scene.json に保存")) {
		context.simulationManager->SaveCurrentSimulationLayoutToSceneJson("resources/scene.json");
	}

	ImGui::Separator();
	ImGui::Text("=== 障害物のリスト (総数: %d) ===", static_cast<int>(context.obstacles->size()));
	if (ImGui::Button("障害物の設定をJSONに保存")) {
		context.simulationManager->SaveCurrentSimulationLayoutToSceneJson("resources/scene.json");
	}
	if (context.simulationSaveMessage && !context.simulationSaveMessage->empty()) {
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", context.simulationSaveMessage->c_str());
	}

	int obsIndex = 0;
	for (const auto& obstacle : *context.obstacles) {
		Vector3 pos = obstacle->GetPosition();
		Vector3 colOff = obstacle->GetCollisionOffset();
		Vector3 colScale = obstacle->GetCollisionScale();
		bool colEnabled = obstacle->IsCollisionEnabled();
		bool useMeshCol = obstacle->IsUseMeshCollider();

		ImGui::PushID(obsIndex);
		ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", obsIndex, pos.x, pos.y, pos.z);
		if (ImGui::Checkbox("Collision Enabled", &colEnabled)) obstacle->SetCollisionEnabled(colEnabled);
		if (ImGui::Checkbox("Use Mesh Collider", &useMeshCol)) obstacle->SetUseMeshCollider(useMeshCol);
		if (ImGui::DragFloat3("Collision Offset", &colOff.x, 0.1f)) obstacle->SetCollisionOffset(colOff);
		if (ImGui::DragFloat3("Collision Scale", &colScale.x, 0.05f)) obstacle->SetCollisionScale(colScale);
		ImGui::PopID();
		++obsIndex;
	}
	if (context.obstacles->empty()) {
		ImGui::Text("現在、障害物は存在しません。");
	}
#else
	(void)context;
#endif
}

void StageEditorPanel::DrawBlenderAndPreviewPanel(
	bool& isEditorPreviewPlaying, const std::function<void()>& resetEditorPreview) {
#if defined(ENABLE_IMGUI)
#if defined(CG2_ENABLE_STAGE_VALIDATION)
	ImGui::Text("レベル検査表示");
	ImGui::Checkbox("検査一覧を表示", &showStageValidationWindow_);
	ImGui::Checkbox("警告ラベルを表示", &showStageValidationLabels_);
	ImGui::Separator();
#endif

	if (ImGui::Button("Open Blender")) {
		OpenStageBlendFile(blenderLaunchMessage_);
	}
	if (!blenderLaunchMessage_.empty()) {
		ImGui::TextWrapped("%s", blenderLaunchMessage_.c_str());
	}

	ImGui::Separator();
	ImGui::Text("敵機ルート確認");
	if (ImGui::Button("リセット") && resetEditorPreview) {
		resetEditorPreview();
	}
	ImGui::SameLine();
	if (ImGui::Button("再生")) {
		isEditorPreviewPlaying = true;
		OutputDebugStringA("[EditorPreview] Play.\n");
	}
	ImGui::SameLine();
	if (ImGui::Button("ストップ")) {
		isEditorPreviewPlaying = false;
		OutputDebugStringA("[EditorPreview] Stop.\n");
	}

	ImGui::TextColored(
		isEditorPreviewPlaying ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f),
		"状態: %s", isEditorPreviewPlaying ? "再生中" : "停止中");
	ImGui::TextWrapped("リセットでscene.jsonを読み直して初期状態に戻し、停止状態にします。再生で敵機ルートなどのゲーム更新が進み、ストップでその場に止まります。");
#else
	(void)isEditorPreviewPlaying;
	(void)resetEditorPreview;
#endif
}

void StageEditorPanel::DrawPackageGeneratorPanel() {
#if defined(ENABLE_IMGUI)
	static std::string packageMessage;
	static HANDLE packageProcess = nullptr;
	static bool includeDemoPlay = false;
	static bool includeClearCinematicDemo = false;
	const bool projectFound = !FindProjectRoot().empty();
	const bool runningRelease = IsCurrentExecutableReleaseBuild();
	const std::filesystem::path zipPath = GetExecutablePackageZipPath();
	UpdateExecutablePackageStatus(packageProcess, zipPath, packageMessage);
	const bool packageCreationRunning = packageProcess != nullptr;
	ImGui::Text("実行ファイル生成");
	ImGui::TextWrapped("Release版のValkyrieStrike.exe、DLL、resourcesをまとめて、配布用ZIPを作成します。");
	ImGui::Separator();
	ImGui::BeginDisabled(packageCreationRunning);
	ImGui::Checkbox("デモプレイを含める", &includeDemoPlay);
	ImGui::Checkbox("クリア演出デモを含める", &includeClearCinematicDemo);
	ImGui::EndDisabled();
	ImGui::TextDisabled("クリア演出デモをONにすると、タイトルのTABでボス撃破からリザルトまでを自動再生します。");
	ImGui::Separator();
	if (!projectFound) {
		ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "プロジェクトフォルダーから起動した場合のみ利用できます。");
	} else if (packageCreationRunning) {
		ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.25f, 1.0f), "ZIP作成中です。完了までお待ちください。");
	} else if (runningRelease) {
		ImGui::TextDisabled("現在Release版を実行中です。ビルドは行わず、現在のRelease版をZIPにします。");
		if (ImGui::Button("現在のRelease版をZIP作成")) {
			packageMessage = CreateExecutablePackage(true, includeDemoPlay, includeClearCinematicDemo, packageProcess)
				? "ZIP作成中です。完了を確認しています。"
				: "ZIP作成を開始できませんでした。";
		}
	} else if (ImGui::Button("ReleaseビルドしてZIP作成")) {
		packageMessage = CreateExecutablePackage(false, includeDemoPlay, includeClearCinematicDemo, packageProcess)
			? "ReleaseビルドとZIP作成中です。完了を確認しています。"
			: "実行ファイル生成を開始できませんでした。";
	}
	ImGui::TextDisabled("出力先: generated/packages/ValkyrieStrike_Playable-Release.zip");
	if (!packageMessage.empty()) {
		ImGui::Spacing();
		ImGui::TextWrapped("%s", packageMessage.c_str());
	}
#endif
}

void StageEditorPanel::DrawStageValidationOverlays(Camera* activeCamera) const {
#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)
	const StageValidation::Report& stageValidationReport = StageValidation::GetLastReport();
	if (stageValidationReport.HasMessages() || stageValidationReport.HasCheckItems()) {
		if (showStageValidationWindow_) {
			DrawStageValidationWindow(stageValidationReport);
		}
		if (showStageValidationLabels_ && activeCamera) {
			DrawStageValidationOverlay(stageValidationReport, activeCamera->GetViewProjectionMatrix());
		}
	}
#else
	(void)activeCamera;
#endif
}
