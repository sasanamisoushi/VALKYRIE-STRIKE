#include "TitleOverlayRenderer.h"

#include "TitleScene.h"
#include "2D/SpriteCommon.h"
#include "Game/base/GameSettings.h"
#include "engine/Debug/ImGuiManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <externals/imgui/imgui.h>
#include <string>

namespace {
Matrix4x4 MakeCenteredUVRotation(float radians) {
	const Matrix4x4 moveToCenter = MyMath::MakeTranslateMatrix({ -0.5f, -0.5f, 0.0f });
	const Matrix4x4 rotate = MyMath::MakeRotateZMatrix(radians);
	const Matrix4x4 moveBack = MyMath::MakeTranslateMatrix({ 0.5f, 0.5f, 0.0f });
	return MyMath::Multiply(MyMath::Multiply(moveToCenter, rotate), moveBack);
}
}

void TitleOverlayRenderer::DrawAttractDemoStartPrompt(const TitleScene& scene, float screenWidth, float screenHeight) const {
#if defined(CG2_RELEASE_BUILD) && defined(CG2_ENABLE_GAME_UI)
	if (!TitleScene::IsAttractDemoAvailable() || scene.IsSettingsOpen() || !ImGui::GetCurrentContext()) {
		return;
	}

	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const float width = displaySize.x > 1.0f ? displaySize.x : screenWidth;
	const float height = displaySize.y > 1.0f ? displaySize.y : screenHeight;
	const char* prompt = TitleScene::IsAttractDemoEnabled()
		? "[ TAB ]  STOP ATTRACT DEMO"
		: (TitleScene::ShouldPlayClearCinematicDemo() ? "[ TAB ]  START CLEAR CINEMATIC DEMO" : "[ TAB ]  START ATTRACT DEMO");
	ImFont* font = ImGuiManager::GetGameOverFont();
	if (!font) {
		font = ImGui::GetFont();
	}
	constexpr float kFontSize = 18.0f;
	const ImVec2 textSize = font->CalcTextSizeA(kFontSize, 1000.0f, 0.0f, prompt);
	const float x = (width - textSize.x) * 0.5f;
	const float y = height - 48.0f;
	const int alpha = 225;
	ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
	drawList->AddRectFilled(
		ImVec2(x - 20.0f, y - 7.0f), ImVec2(x + textSize.x + 20.0f, y + textSize.y + 8.0f),
		IM_COL32(2, 14, 31, 205), 3.0f);
	drawList->AddLine(ImVec2(x - 20.0f, y - 7.0f), ImVec2(x + 28.0f, y - 7.0f),
		IM_COL32(80, 226, 255, alpha), 2.0f);
	drawList->AddLine(ImVec2(x + textSize.x - 28.0f, y + textSize.y + 8.0f), ImVec2(x + textSize.x + 20.0f, y + textSize.y + 8.0f),
		IM_COL32(80, 226, 255, alpha), 2.0f);
	drawList->AddText(font, kFontSize, ImVec2(x + 1.5f, y + 1.5f), IM_COL32(0, 0, 0, alpha), prompt);
	drawList->AddText(font, kFontSize, ImVec2(x, y), IM_COL32(194, 246, 255, alpha), prompt);
#else
	(void)scene;
	(void)screenWidth;
	(void)screenHeight;
#endif
}

void TitleScene::DrawWarpCloudLayers(
	float screenWidth,
	float screenHeight) {
	if (!warpCloudRearSprite_ || !warpCloudFrontSprite_) {
		return;
	}

	const float progress = std::clamp(
		static_cast<float>(launchFrame_) / static_cast<float>(kLaunchDurationFrames), 0.0f, 1.0f);
	const float rampIn = std::clamp((progress - 0.12f) / 0.28f, 0.0f, 1.0f);
	const float rampOut = 1.0f - std::clamp((progress - 0.76f) / 0.22f, 0.0f, 1.0f);
	const float visibility = rampIn * rampIn * (3.0f - 2.0f * rampIn) * rampOut;
	if (visibility <= 0.001f) {
		return;
	}

	const float centerX = screenWidth * 0.50f;
	const float centerY = screenHeight * 0.47f;
	const float minDimension = (std::min)(screenWidth, screenHeight);
	const float rearProgress = std::clamp((progress - 0.10f) / 0.82f, 0.0f, 1.0f);
	const float frontProgress = std::clamp((progress - 0.24f) / 0.66f, 0.0f, 1.0f);

	auto prepareRing = [&](Sprite* ring, float size, float uvRotation, const Vector4& color) {
		ring->SetPosition({ centerX, centerY });
		ring->SetSize({ size, size });
		ring->SetRotation(0.0f);
		ring->SetUVTransform(MakeCenteredUVRotation(uvRotation));
		ring->SetColor(color);
		ring->Update();
	};

	// 奥はゆっくり時計回り、手前は速く反時計回りに動かし、同じ雲でも別の渦に見せる。
	prepareRing(warpCloudRearSprite_.get(), minDimension * (0.52f + rearProgress * 1.12f),
		-progress * 3.4f, { 0.24f, 0.72f, 1.0f, visibility * 0.36f });
	prepareRing(warpCloudFrontSprite_.get(), minDimension * (0.30f + frontProgress * 1.48f),
		progress * 5.8f + std::sin(progress * 12.0f) * 0.16f,
		{ 0.58f, 0.94f, 1.0f, visibility * 0.46f });

	SpriteCommon::GetInstance()->SetAdditivePipelineState();
	warpCloudRearSprite_->Draw();
	warpCloudFrontSprite_->Draw();
	// この後に描くフォールド粒子とUIは、従来のアルファ合成へ戻す。
	SpriteCommon::GetInstance()->SetCommonPipelineState();
}

void TitleScene::DrawFoldTransition(
	float screenWidth,
	float screenHeight) {
	if (!foldVeilSprite_ || !foldDepthSprite_ || !foldPurpleCoreSprite_ ||
		!foldCoreSprite_ || !foldBeamSprite_ || !foldFlashSprite_) {
		return;
	}

	const float progress = std::clamp(
		static_cast<float>(launchFrame_) / static_cast<float>(kLaunchDurationFrames), 0.0f, 1.0f);
	// 0.22 から航法光が集まり、0.75 付近で跳躍光へ変化する。
	const float streakProgress = std::clamp((progress - 0.22f) / 0.54f, 0.0f, 1.0f);
	const float streakAlpha = std::clamp(streakProgress * 2.8f, 0.0f, 1.0f) *
		std::clamp((0.86f - progress) * 7.0f, 0.0f, 1.0f);
	const float flashAlpha = std::clamp((progress - 0.73f) / 0.22f, 0.0f, 1.0f);
	const float centerX = screenWidth * 0.50f;
	const float centerY = screenHeight * 0.47f;
	const float maxRadius = (std::min)(screenWidth, screenHeight) * 0.78f;

	// 静止画は使わず、薄い水色の空間色と多数の発光球を毎フレーム計算して重ねる。
	foldVeilSprite_->SetPosition({ 0.0f, 0.0f });
	foldVeilSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	foldVeilSprite_->SetSize({ screenWidth, screenHeight });
	foldVeilSprite_->SetColor({ 0.02f, 0.34f, 0.58f, streakAlpha * 0.30f });
	foldVeilSprite_->Update();
	foldVeilSprite_->Draw();

	const float depthProgress = std::clamp((progress - 0.26f) / 0.42f, 0.0f, 1.0f);
	const float depthEase = depthProgress * depthProgress * (3.0f - 2.0f * depthProgress);
	const float depthFade = 1.0f - std::clamp((progress - 0.72f) / 0.20f, 0.0f, 1.0f);
	const float depthAlpha = depthEase * depthFade;
	const float apertureSize = 38.0f + maxRadius * (0.12f + depthEase * 0.20f);
	foldDepthSprite_->SetPosition({ centerX, centerY });
	foldDepthSprite_->SetSize({ apertureSize * 1.20f, apertureSize * 1.20f });
	foldDepthSprite_->SetRotation(-progress * 0.7f);
	foldDepthSprite_->SetColor({ 0.015f, 0.055f, 0.26f, depthAlpha * 0.70f });
	foldDepthSprite_->Update();
	foldDepthSprite_->Draw();

	foldPurpleCoreSprite_->SetPosition({ centerX, centerY });
	foldPurpleCoreSprite_->SetSize({ apertureSize * 0.76f, apertureSize * 0.76f });
	foldPurpleCoreSprite_->SetRotation(progress * 1.1f);
	foldPurpleCoreSprite_->SetColor({ 0.16f, 0.025f, 0.34f, depthAlpha * 0.62f });
	foldPurpleCoreSprite_->Update();
	foldPurpleCoreSprite_->Draw();

	SpriteCommon::GetInstance()->SetAdditivePipelineState();
	for (size_t i = 0; i < foldStreakSprites_.size(); ++i) {
		Sprite* particle = foldStreakSprites_[i].get();
		if (!particle) {
			continue;
		}
		constexpr size_t kParticlesPerSpiral = 10;
		constexpr float kSpiralCount = 24.0f;
		const float spiralIndex = static_cast<float>(i / kParticlesPerSpiral);
		const float particleIndex = static_cast<float>(i % kParticlesPerSpiral);
		const float radialSeed = (particleIndex + 0.35f) / static_cast<float>(kParticlesPerSpiral);
		const float inwardFactor = 1.0f - streakProgress * 0.84f;
		const float radius = 16.0f + maxRadius * radialSeed * inwardFactor;
		const float wavePhase = spiralIndex * 1.19f + radialSeed * 5.8f + progress * 10.5f;
		const float angle = spiralIndex * (6.28318531f / kSpiralCount) + progress * 7.0f +
			std::sin(wavePhase) * (0.20f + radialSeed * 0.22f);
		const float curl = std::cos(wavePhase * 1.31f) * maxRadius * 0.052f * radialSeed;
		const float x = centerX + std::cos(angle) * radius - std::sin(angle) * curl;
		const float y = centerY + std::sin(angle) * radius + std::cos(angle) * curl;
		const float randomSeed = std::fmod(std::sin((static_cast<float>(i) + 1.0f) * 91.173f) * 43758.5453f, 1.0f);
		const float normalizedRandom = randomSeed < 0.0f ? randomSeed + 1.0f : randomSeed;
		const float diameter = (31.0f + radialSeed * 104.0f + std::sin(wavePhase * 1.9f) * 14.0f) *
			(0.58f + normalizedRandom * 0.94f);
		const float cyanVariation = 0.28f + std::fmod(spiralIndex * 0.11f, 0.24f);
		const float aspectSeed = std::fmod(std::sin((static_cast<float>(i) + 3.0f) * 47.417f) * 12515.873f, 1.0f);
		const float normalizedAspect = aspectSeed < 0.0f ? aspectSeed + 1.0f : aspectSeed;
		const float widthScale = 0.76f + normalizedAspect * 0.82f;
		const float heightScale = 0.38f + (1.0f - normalizedAspect) * 0.56f;
		particle->SetPosition({ x, y });
		particle->SetSize({ diameter * widthScale, diameter * heightScale });
		particle->SetRotation(angle + 1.57079633f + std::sin(wavePhase) * 0.24f);
		particle->SetColor({ cyanVariation, 0.72f + radialSeed * 0.24f, 1.0f,
			streakAlpha * (0.14f + normalizedRandom * 0.19f + radialSeed * 0.16f) });
		particle->Update();
		particle->Draw();
	}

	const float coreSize = 54.0f + streakProgress * (screenHeight * 0.22f);
	foldCoreSprite_->SetPosition({ centerX, centerY });
	foldCoreSprite_->SetSize({ coreSize * 1.42f, coreSize * 0.74f });
	foldCoreSprite_->SetRotation(-progress * 3.4f);
	foldCoreSprite_->SetColor({ 0.70f, 0.96f, 1.0f, streakAlpha * 0.68f });
	foldCoreSprite_->Update();
	foldCoreSprite_->Draw();
	SpriteCommon::GetInstance()->SetCommonPipelineState();

	foldBeamSprite_->SetPosition({ centerX, centerY });
	foldBeamSprite_->SetSize({ screenWidth * (0.08f + streakProgress * 1.04f), 2.0f + streakProgress * 11.0f });
	foldBeamSprite_->SetRotation(std::sin(progress * 7.0f) * 0.025f);
	foldBeamSprite_->SetColor({ 0.58f, 0.96f, 1.0f, streakAlpha * 0.74f });
	foldBeamSprite_->Update();
	foldBeamSprite_->Draw();

	foldFlashSprite_->SetPosition({ 0.0f, 0.0f });
	foldFlashSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	foldFlashSprite_->SetSize({ screenWidth, screenHeight });
	foldFlashSprite_->SetColor({
		0.46f + flashAlpha * 0.54f,
		0.86f + flashAlpha * 0.14f,
		1.00f,
		flashAlpha * 0.88f,
	});
	foldFlashSprite_->Update();
	foldFlashSprite_->Draw();
}

void TitleScene::DrawMenuOverlay(
	float screenWidth,
	float screenHeight,
	float opacity) {
	if (!menuPanelSprite_ || !menuRowSprite_) {
		return;
	}

	struct LabelRegion {
		float x;
		float y;
		float width;
		float height;
	};
	constexpr std::array<LabelRegion, 8> kLabelRegions = {{
		{ 75.0f, 45.0f, 1065.0f, 140.0f }, // START GAME
		{ 170.0f, 195.0f, 880.0f, 130.0f }, // SETTINGS
		{ 145.0f, 345.0f, 930.0f, 140.0f }, // EXIT GAME
		{ 35.0f, 500.0f, 1145.0f, 140.0f }, // MASTER VOLUME
		{ 10.0f, 650.0f, 1195.0f, 140.0f }, // MOUSE SENSITIVITY
		{ 75.0f, 805.0f, 1060.0f, 140.0f }, // CONTROL GUIDE
		{ 330.0f, 960.0f, 555.0f, 135.0f }, // BACK
		{ 245.0f, 1100.0f, 725.0f, 145.0f }, // ON / OFF
	}};
	const auto drawLabel = [&](size_t labelIndex, float centerX, float topY, float targetHeight, float alpha = 1.0f) {
		if (labelIndex >= menuLabelSprites_.size() || !menuLabelSprites_[labelIndex]) {
			return;
		}
		const LabelRegion& region = kLabelRegions[labelIndex];
		const float targetWidth = targetHeight * region.width / region.height;
		Sprite* label = menuLabelSprites_[labelIndex].get();
		label->SetTextureLeftTop({ region.x, region.y });
		label->SetTextureSize({ region.width, region.height });
		label->SetPosition({ centerX - targetWidth * 0.5f, topY });
		label->SetSize({ targetWidth, targetHeight });
		label->SetColor({ 1.0f, 1.0f, 1.0f, alpha * opacity });
		label->Update();
		label->Draw();
	};
	if (!isSettingsOpen_) {
		const auto drawMainMenuLabel = [&](size_t labelIndex, MenuItem item, float topY) {
			const bool selected = selectedMenuItem_ == item;
			if (selected) {
				const float rowLeft = screenWidth * 0.5f - 202.0f;
				menuRowSprite_->SetPosition({ rowLeft, topY - 10.0f });
				menuRowSprite_->SetSize({ 404.0f, 62.0f });
				menuRowSprite_->SetColor({ 0.02f, 0.09f, 0.15f, 0.78f * opacity });
				menuRowSprite_->Update();
				menuRowSprite_->Draw();

				menuRowSprite_->SetPosition({ rowLeft, topY - 10.0f });
				menuRowSprite_->SetSize({ 4.0f, 62.0f });
				menuRowSprite_->SetColor({ 0.28f, 0.86f, 0.96f, opacity });
				menuRowSprite_->Update();
				menuRowSprite_->Draw();
			}
			const float labelHeight = selected ? 48.0f : 42.0f;
			drawLabel(labelIndex, screenWidth * 0.5f,
				topY - (labelHeight - 42.0f) * 0.5f, labelHeight,
				selected ? 1.0f : 0.58f);
		};
		drawMainMenuLabel(0, MenuItem::Start, screenHeight - 225.0f);
		drawMainMenuLabel(1, MenuItem::Settings, screenHeight - 145.0f);
		drawMainMenuLabel(2, MenuItem::Exit, screenHeight - 75.0f);
		return;
	}

	const GameSettings& settings = GameSettings::GetInstance();
	drawLabel(1, screenWidth * 0.5f, screenHeight * 0.10f, 60.0f);
	const float rowStartY = screenHeight * 0.27f;
	const float rowSpacing = 82.0f;
	const float labelCenterX = screenWidth * 0.32f;
	const float sliderX = screenWidth * 0.56f;
	const float sliderWidth = screenWidth * 0.30f;

	const auto drawSettingLabel = [&](int index, size_t labelIndex, float topY) {
		const bool selected = index == static_cast<int>(selectedSettingsItem_);
		const float labelHeight = selected ? 54.0f : 46.0f;
		drawLabel(labelIndex, labelCenterX, topY - (labelHeight - 46.0f) * 0.5f,
			labelHeight, selected ? 1.0f : 0.58f);
	};
	const auto drawLevelMeter = [&](int meterIndex, float topY, float valueRatio,
		int currentStep, int maxStep, bool selected) {
		constexpr int kSegmentCount = 20;
		const float ratio = std::clamp(valueRatio, 0.0f, 1.0f);
		const int litSegmentCount = static_cast<int>(ratio * static_cast<float>(kSegmentCount) + 0.5f);
		const float segmentGap = 4.0f;
		const float segmentWidth = (sliderWidth - segmentGap * static_cast<float>(kSegmentCount - 1)) /
			static_cast<float>(kSegmentCount);
		const float segmentY = topY + 17.0f;
		for (int index = 0; index < kSegmentCount; ++index) {
			Sprite* segment = settingMeterSegments_[meterIndex * kSegmentCount + index].get();
			segment->SetPosition({ sliderX + static_cast<float>(index) * (segmentWidth + segmentGap), segmentY });
			segment->SetSize({ segmentWidth, 20.0f });
			const bool isLit = index < litSegmentCount;
			const bool isCurrent = isLit && index == litSegmentCount - 1;
			segment->SetColor(isCurrent
				? Vector4{ 0.92f, 0.98f, 1.0f, 1.0f }
				: (isLit
					? (selected ? Vector4{ 1.0f, 0.46f, 0.08f, 1.0f } : Vector4{ 0.58f, 0.27f, 0.07f, 0.92f })
					: Vector4{ 0.26f, 0.29f, 0.32f, 1.0f }));
			segment->Update();
			segment->Draw();
		}

		const std::string levelText = std::to_string(currentStep) + "/" + std::to_string(maxStep);
		const float digitWidth = 20.0f;
		const float digitHeight = 26.0f;
		const float digitStartX = sliderX + sliderWidth + 18.0f;
		for (size_t index = 0; index < levelText.size() && index < settingValueDigitSprites_[meterIndex].size(); ++index) {
			Sprite* digit = settingValueDigitSprites_[meterIndex][index].get();
			const int glyphIndex = levelText[index] == '/' ? 10 : levelText[index] - '0';
			digit->SetTextureLeftTop({ static_cast<float>(glyphIndex * 64), 0.0f });
			digit->SetTextureSize({ 64.0f, 80.0f });
			digit->SetPosition({ digitStartX + digitWidth * static_cast<float>(index), topY + 13.0f });
			digit->SetSize({ digitWidth, digitHeight });
			digit->SetColor(selected ? Vector4{ 1.0f, 0.82f, 0.24f, 1.0f } : Vector4{ 0.62f, 0.66f, 0.70f, 0.86f });
			digit->Update();
			digit->Draw();
		}
	};

	const int volumeStep = static_cast<int>(settings.GetMasterVolume() * 20.0f + 0.5f);
	const float sensitivityRatio = (settings.GetMouseSensitivity() - 0.0005f) / 0.0095f;
	const int sensitivityStep = static_cast<int>(sensitivityRatio * 19.0f + 0.5f);
	drawSettingLabel(0, 3, rowStartY);
	drawLevelMeter(0, rowStartY, settings.GetMasterVolume(), volumeStep, 20,
		selectedSettingsItem_ == SettingsItem::MasterVolume);
	drawSettingLabel(1, 4, rowStartY + rowSpacing);
	drawLevelMeter(1, rowStartY + rowSpacing, sensitivityRatio, sensitivityStep, 19,
		selectedSettingsItem_ == SettingsItem::MouseSensitivity);
	drawSettingLabel(2, 5, rowStartY + rowSpacing * 2.0f);
	const bool guideSelected = selectedSettingsItem_ == SettingsItem::ControlGuide;
	drawLabel(7, screenWidth * 0.75f, rowStartY + rowSpacing * 2.0f + 3.0f,
		guideSelected ? 42.0f : 36.0f,
		guideSelected ? 1.0f : (settings.IsControlGuideVisible() ? 0.74f : 0.40f));
	const bool backSelected = selectedSettingsItem_ == SettingsItem::Back;
	drawLabel(6, screenWidth * 0.5f, rowStartY + rowSpacing * 3.0f,
		backSelected ? 54.0f : 46.0f, backSelected ? 1.0f : 0.58f);
}
