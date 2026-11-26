#include "AmmoManager.h"

#include "engine/base/Logger.h"

#include <externals/json.hpp>

#include <algorithm>
#include <fstream>

namespace {
constexpr const char* kDefaultAmmoSettingsPath = "resources/ammo_settings.json";
constexpr int kMinimumMagazineCapacity = 1;
constexpr int kMaximumMagazineCapacity = 999;
}

void AmmoManager::Initialize() {
    normalMagazineCapacity_ = kDefaultNormalMagazineCapacity;
    homingMagazineCapacity_ = kDefaultHomingMagazineCapacity;
    isNormalReloading_ = false;
    isHomingReloading_ = false;
    normalReloadFrame_ = 0;
    homingReloadFrame_ = 0;
    // 保存済みの弾数があれば起動時から反映し、なければ既定値を使う。
    LoadSettings(kDefaultAmmoSettingsPath);
    normalMagazine_ = normalMagazineCapacity_;
    homingMagazine_ = homingMagazineCapacity_;
}

bool AmmoManager::TryConsume(MissileType type) {
    const bool isNormal = type == MissileType::Normal;
    bool& isReloading = isNormal ? isNormalReloading_ : isHomingReloading_;
    int& reloadFrame = isNormal ? normalReloadFrame_ : homingReloadFrame_;
    int& magazine = isNormal ? normalMagazine_ : homingMagazine_;
    if (isReloading || magazine <= 0) {
        return false;
    }
    --magazine;
    // 使い切った瞬間に自動補充を開始する。予備弾や手動操作は使わない。
    if (magazine == 0) {
        isReloading = true;
        reloadFrame = 0;
    }
    return true;
}

void AmmoManager::UpdateAutoReload() {
    if (isNormalReloading_ && ++normalReloadFrame_ >= kReloadDurationFrames) {
        normalMagazine_ = normalMagazineCapacity_;
        isNormalReloading_ = false;
        normalReloadFrame_ = 0;
    }
    if (isHomingReloading_ && ++homingReloadFrame_ >= kReloadDurationFrames) {
        homingMagazine_ = homingMagazineCapacity_;
        isHomingReloading_ = false;
        homingReloadFrame_ = 0;
    }
}

void AmmoManager::SetMagazineCapacities(int normalCapacity, int homingCapacity) {
    normalMagazineCapacity_ = std::clamp(normalCapacity, kMinimumMagazineCapacity, kMaximumMagazineCapacity);
    homingMagazineCapacity_ = std::clamp(homingCapacity, kMinimumMagazineCapacity, kMaximumMagazineCapacity);
    normalMagazine_ = (std::min)(normalMagazine_, normalMagazineCapacity_);
    homingMagazine_ = (std::min)(homingMagazine_, homingMagazineCapacity_);
}

void AmmoManager::ReloadAll() {
    normalMagazine_ = normalMagazineCapacity_;
    homingMagazine_ = homingMagazineCapacity_;
    isNormalReloading_ = false;
    isHomingReloading_ = false;
    normalReloadFrame_ = 0;
    homingReloadFrame_ = 0;
}

bool AmmoManager::SaveSettings(const std::string& filePath) const {
    nlohmann::json root;
    root["normalMagazineCapacity"] = normalMagazineCapacity_;
    root["homingMagazineCapacity"] = homingMagazineCapacity_;

    std::ofstream ofs(filePath, std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << root.dump(4);
    return static_cast<bool>(ofs);
}

bool AmmoManager::LoadSettings(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        return false;
    }

    nlohmann::json root;
    try {
        ifs >> root;
    } catch (const std::exception& e) {
        GetLogger().Log("[AmmoManager] Failed to load " + filePath + ": " + e.what());
        return false;
    }
    if (!root.is_object()) {
        return false;
    }

    const int normalCapacity = root.value("normalMagazineCapacity", normalMagazineCapacity_);
    const int homingCapacity = root.value("homingMagazineCapacity", homingMagazineCapacity_);
    SetMagazineCapacities(normalCapacity, homingCapacity);
    // 読み込みは設定の切り替えと同時に使えるよう、現在の弾も満タンに戻す。
    normalMagazine_ = normalMagazineCapacity_;
    homingMagazine_ = homingMagazineCapacity_;
    isNormalReloading_ = false;
    isHomingReloading_ = false;
    normalReloadFrame_ = 0;
    homingReloadFrame_ = 0;
    return true;
}
