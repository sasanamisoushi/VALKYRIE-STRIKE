#include "ExplosionManager.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/GroundEnemy.h"
#include "Game/enemy/JammerEnemy.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace {
constexpr bool kEnableExplosionMeshes = true;
}

void ExplosionManager::Initialize(ParticleManager *particleManager) {
    particleManager_ = particleManager;

	// 個別設定を初めて有効にした時は、現在の共通設定をそのまま引き継ぐ。
	for (ExplosionConfig& config : targetConfigs_) {
		config = commonConfig_;
	}
	useCommonConfig_.fill(true);
    // アプリ起動時に前回の設定があれば読み込む。
    // 個別プロファイルもここで復元するため、初期化後に読み込む。
    LoadFromJson("resources/explosionConfig.json");
    if (particleManager_) {
        particleManager_->CreateParticleGroup("explosionCore", "resources/circle2.png");
		particleManager_->CreateParticleGroup("explosionSmoke", "resources/circle.png");
    }

    if (kEnableExplosionMeshes) {
        // ヒット演出用のモデルは全Explosionで共有する。
        ModelManager::GetInstance()->CreateRingModel(
            "Explosion_OrbitRing", 64, 1.0f, 0.72f, true,
            { 1.0f, 0.4f, 0.05f, 0.0f }, { 1.0f, 1.0f, 0.75f, 0.95f });
        ModelManager::GetInstance()->CreateCylinderModel(
            "Explosion_Cylinder", 32, 0.9f, 0.75f, 1.5f);

        if (Model *cylinderModel = ModelManager::GetInstance()->FindModel("Explosion_Cylinder")) {
            cylinderModel->SetColor({ 1.0f, 0.25f, 0.02f, 0.25f });
            cylinderModel->SetAlphaReference(0.0f);
        }

        for (Explosion &explosion : explosions_) {
            InitializeExplosionObjects(explosion);
            explosion.active = false;
            explosion.age = 0.0f;
            explosion.duration = 0.7f;
        }
    }
}

void ExplosionManager::InitializeExplosionObjects(Explosion &explosion) {
    Object3dCommon *object3dCommon = Object3dCommon::GetInstance();
    for (auto &ring : explosion.rings) {
        if (!ring) {
            ring = std::make_unique<Object3d>();
            ring->Initialize(object3dCommon);
        }
        ring->SetModel("Explosion_OrbitRing");
        ring->SetScale({ 0.0f, 0.0f, 0.0f });
        ring->Update();
    }

    if (!explosion.cylinder) {
        explosion.cylinder = std::make_unique<Object3d>();
        explosion.cylinder->Initialize(object3dCommon);
    }
    explosion.cylinder->SetModel("Explosion_Cylinder");
    explosion.cylinder->SetScale({ 0.0f, 0.0f, 0.0f });
    explosion.cylinder->Update();
}

ExplosionManager::Explosion *ExplosionManager::AcquireExplosion() {
    Explosion *reuseTarget = nullptr;

    for (Explosion &explosion : explosions_) {
        if (!explosion.active) {
            reuseTarget = &explosion;
            break;
        }

        if (!reuseTarget || explosion.age > reuseTarget->age) {
            reuseTarget = &explosion;
        }
    }

    if (!reuseTarget) {
        return nullptr;
    }

    InitializeExplosionObjects(*reuseTarget);
    reuseTarget->active = true;
    reuseTarget->age = 0.0f;
    reuseTarget->duration = 0.7f;
    return reuseTarget;
}

void ExplosionManager::CreateExplosions(const std::vector<Vector3> &hitPositions, EffectTarget target) {
    CreateDestructionEffects(hitPositions, target);
}

void ExplosionManager::CreateHitEffects(const std::vector<Vector3> &hitPositions) {
    CreateEffects(hitPositions, false, false, commonConfig_);
}

void ExplosionManager::CreateDestructionEffects(const std::vector<Vector3> &hitPositions, EffectTarget target) {
    CreateEffects(hitPositions, true, true, GetConfig(target));
}

void ExplosionManager::CreateDestructionPreview(const std::vector<Vector3>& hitPositions,
	EffectTarget target, bool includeShockwaveMeshes) {
	CreateEffects(hitPositions, true, includeShockwaveMeshes, GetConfig(target));
}

void ExplosionManager::ClearEffects() {
	if (particleManager_) {
		particleManager_->ClearParticleGroup("explosionCore");
		particleManager_->ClearParticleGroup("explosionSmoke");
	}
	for (Explosion& explosion : explosions_) {
		explosion.active = false;
		explosion.age = 0.0f;
		for (auto& ring : explosion.rings) {
			if (ring) {
				ring->SetScale({ 0.0f, 0.0f, 0.0f });
				ring->Update();
			}
		}
		if (explosion.cylinder) {
			explosion.cylinder->SetScale({ 0.0f, 0.0f, 0.0f });
			explosion.cylinder->Update();
		}
	}
}

std::size_t ExplosionManager::GetActiveEffectCount() const {
	std::size_t activeCount = 0;
	for (const Explosion& explosion : explosions_) {
		if (explosion.active) {
			++activeCount;
		}
	}
	if (particleManager_) {
		activeCount += particleManager_->GetParticleCount("explosionCore");
		activeCount += particleManager_->GetParticleCount("explosionSmoke");
	}
	return activeCount;
}

void ExplosionManager::CreateEffects(const std::vector<Vector3> &hitPositions, bool useDestructionParticle,
	bool includeMeshes,
	const ExplosionConfig& config) {
    if (!particleManager_) return;

    for (const Vector3 &pos : hitPositions) {
		const Vector4 flameColor{ config.color[0], config.color[1], config.color[2], config.color[3] };
		const Vector4 smokeColor{ config.smokeColor[0], config.smokeColor[1], config.smokeColor[2], config.smokeColor[3] };

		if (useDestructionParticle) {
			switch (static_cast<DestructionParticleType>(config.destructionParticleType)) {
			case DestructionParticleType::FlameToSmoke:
				particleManager_->EmitFlameToSmoke("explosionCore", pos, config.count,
					flameColor, smokeColor, config.scale, config.scale * config.flameToSmokeScale,
					config.speed, config.speedVariance, config.lifeTimeMin, config.lifeTimeMax,
					config.posVariance, config.flameToSmokeTransition);
				break;
			case DestructionParticleType::Fireball:
				particleManager_->EmitFireball("explosionCore", pos, config.count / 3 + 1,
					flameColor, config.scale * 1.25f, config.scale * 4.0f,
					config.lifeTimeMax, config.posVariance);
				break;
			case DestructionParticleType::SmokeCloud:
				particleManager_->EmitSmoke("explosionSmoke", pos, config.count / 2 + 1,
					smokeColor, config.scale, config.scale * config.flameToSmokeScale,
					0.035f, config.speed * 0.22f, config.lifeTimeMin, config.lifeTimeMax * 1.8f,
					config.posVariance);
				break;
			case DestructionParticleType::Sparks:
				particleManager_->Emit("test", pos, config.count, flameColor,
					config.speed, config.speedVariance, config.scale, config.scaleVariance,
					config.lifeTimeMin, config.lifeTimeMax, config.posVariance);
				break;
			}
		} else {
			// 通常ヒットは従来どおり短い火花と火球に留め、撃破時だけを切り替える。
			particleManager_->Emit("test", pos, config.count, flameColor,
				config.speed, config.speedVariance, config.scale, config.scaleVariance,
				config.lifeTimeMin, config.lifeTimeMax, config.posVariance);
			particleManager_->EmitFireball("explosionCore", pos, 14,
				{ 1.0f, 0.32f, 0.04f, 0.72f }, 0.45f, 2.6f, 0.7f, 0.22f);
		}

		if (includeMeshes && kEnableExplosionMeshes) {
            Explosion *explosion = AcquireExplosion();
            if (!explosion) {
                continue;
            }

            explosion->position = pos;
            for (auto &ring : explosion->rings) {
                if (!ring) {
                    continue;
                }
                ring->SetModel("Explosion_OrbitRing");
                ring->SetTranslate(pos);
                ring->SetScale({ 0.05f, 0.05f, 0.05f });
            }

            if (explosion->cylinder) {
                explosion->cylinder->SetModel("Explosion_Cylinder");
                explosion->cylinder->SetTranslate(pos);
                explosion->cylinder->SetScale({ 0.2f, 0.2f, 0.2f });
            }
        }
    }
}

ExplosionManager::ExplosionConfig& ExplosionManager::GetConfig(EffectTarget target) {
	const size_t index = static_cast<size_t>(target);
	return (index >= targetConfigs_.size() || useCommonConfig_[index]) ? commonConfig_ : targetConfigs_[index];
}

const ExplosionManager::ExplosionConfig& ExplosionManager::GetConfig(EffectTarget target) const {
	const size_t index = static_cast<size_t>(target);
	return (index >= targetConfigs_.size() || useCommonConfig_[index]) ? commonConfig_ : targetConfigs_[index];
}

bool ExplosionManager::UsesCommonConfig(EffectTarget target) const {
	const size_t index = static_cast<size_t>(target);
	return index >= useCommonConfig_.size() || useCommonConfig_[index];
}

void ExplosionManager::SetUseCommonConfig(EffectTarget target, bool useCommon) {
	const size_t index = static_cast<size_t>(target);
	if (index >= targetConfigs_.size() || target == EffectTarget::StandardEnemy) {
		return;
	}
	if (useCommon && !useCommonConfig_[index]) {
		targetConfigs_[index] = commonConfig_;
	}
	if (!useCommon && useCommonConfig_[index]) {
		targetConfigs_[index] = commonConfig_;
	}
	useCommonConfig_[index] = useCommon;
}

ExplosionManager::EffectTarget ExplosionManager::ClassifyEnemy(const Enemy& enemy) {
	if (enemy.IsBoss()) {
		return EffectTarget::Boss;
	}
	if (dynamic_cast<const GroundEnemy*>(&enemy)) {
		return EffectTarget::GroundEnemy;
	}
	if (dynamic_cast<const JammerEnemy*>(&enemy)) {
		return EffectTarget::JammerEnemy;
	}
	return EffectTarget::StandardEnemy;
}

void ExplosionManager::Update() {
    if (!kEnableExplosionMeshes) {
        return;
    }

    constexpr float kDeltaTime = 1.0f / 60.0f;

    for (Explosion &explosion : explosions_) {
        if (!explosion.active) {
            continue;
        }

        explosion.age += kDeltaTime;
        if (explosion.age >= explosion.duration) {
            explosion.active = false;
            for (auto &ring : explosion.rings) {
                if (ring) {
                    ring->SetScale({ 0.0f, 0.0f, 0.0f });
                    ring->Update();
                }
            }
            if (explosion.cylinder) {
                explosion.cylinder->SetScale({ 0.0f, 0.0f, 0.0f });
                explosion.cylinder->Update();
            }
            continue;
        }

        const float progress = explosion.age / explosion.duration;
        const float pulse = std::sin(progress * 3.14159265f);
        const float ringScale = pulse * 3.0f;
        const float cylinderRadius = pulse * 1.8f;
        const float cylinderHeight = pulse * 0.28f;

        if (explosion.rings[0]) {
            explosion.rings[0]->SetScale({ ringScale, ringScale, ringScale });
            explosion.rings[0]->SetRotate({ 0.75f, 0.15f, progress * 2.5f });
            explosion.rings[0]->Update();
        }

        if (explosion.rings[1]) {
            explosion.rings[1]->SetScale({ ringScale * 0.92f, ringScale * 0.92f, ringScale * 0.92f });
            explosion.rings[1]->SetRotate({ -0.5f, 0.4f, -progress * 2.1f });
            explosion.rings[1]->Update();
        }

        if (explosion.rings[2]) {
            explosion.rings[2]->SetScale({ ringScale * 0.82f, ringScale * 0.82f, ringScale * 0.82f });
            explosion.rings[2]->SetRotate({ 0.2f, -0.65f, progress * 1.8f });
            explosion.rings[2]->Update();
        }

        if (explosion.cylinder) {
            explosion.cylinder->SetTranslate({
                explosion.position.x,
                explosion.position.y - cylinderHeight * 0.75f,
                explosion.position.z
            });
            explosion.cylinder->SetScale({ cylinderRadius, cylinderHeight, cylinderRadius });
            explosion.cylinder->SetRotate({ 0.0f, progress * 1.5f, 0.0f });
            explosion.cylinder->Update();
        }
    }
}

void ExplosionManager::Draw() {
    if (!kEnableExplosionMeshes) {
        return;
    }

    for (const Explosion &explosion : explosions_) {
        if (!explosion.active) {
            continue;
        }

        for (const auto &ring : explosion.rings) {
            if (ring) {
                ring->Draw();
            }
        }
        if (explosion.cylinder) {
            explosion.cylinder->Draw();
        }
    }
}

void ExplosionManager::SaveToJson(const std::string &filepath) {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return;

    auto writeConfig = [&ofs](const char* prefix, const ExplosionConfig& config) {
		ofs << "  \"" << prefix << "count\": " << config.count << ",\n";
		ofs << "  \"" << prefix << "color\": ["
			<< config.color[0] << ", " << config.color[1] << ", "
			<< config.color[2] << ", " << config.color[3] << "],\n";
		ofs << "  \"" << prefix << "speed\": " << config.speed << ",\n";
		ofs << "  \"" << prefix << "speedVariance\": " << config.speedVariance << ",\n";
		ofs << "  \"" << prefix << "scale\": " << config.scale << ",\n";
		ofs << "  \"" << prefix << "scaleVariance\": " << config.scaleVariance << ",\n";
		ofs << "  \"" << prefix << "lifeTimeMin\": " << config.lifeTimeMin << ",\n";
		ofs << "  \"" << prefix << "lifeTimeMax\": " << config.lifeTimeMax << ",\n";
		ofs << "  \"" << prefix << "posVariance\": " << config.posVariance << ",\n";
		ofs << "  \"" << prefix << "destructionParticleType\": " << config.destructionParticleType << ",\n";
		ofs << "  \"" << prefix << "smokeColor\": ["
			<< config.smokeColor[0] << ", " << config.smokeColor[1] << ", "
			<< config.smokeColor[2] << ", " << config.smokeColor[3] << "],\n";
		ofs << "  \"" << prefix << "flameToSmokeScale\": " << config.flameToSmokeScale << ",\n";
		ofs << "  \"" << prefix << "flameToSmokeTransition\": " << config.flameToSmokeTransition << ",\n";
	};

    ofs << "{\n";
	writeConfig("", commonConfig_);
	const struct { EffectTarget target; const char* key; } profiles[] = {
		{ EffectTarget::GroundEnemy, "ground" },
		{ EffectTarget::JammerEnemy, "jammer" },
		{ EffectTarget::Boss, "boss" },
		{ EffectTarget::Player, "player" },
	};
	for (const auto& profile : profiles) {
		const size_t index = static_cast<size_t>(profile.target);
		ofs << "  \"" << profile.key << "UseCommon\": "
			<< (useCommonConfig_[index] ? "true" : "false") << ",\n";
		const std::string prefix = std::string(profile.key) + "_";
		writeConfig(prefix.c_str(), targetConfigs_[index]);
	}
	ofs << "  \"schemaVersion\": 2\n";
    ofs << "}\n";
}

void ExplosionManager::LoadFromJson(const std::string &filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return;

    std::string line;
	bool hasProfileSettings = false;
    while (std::getline(ifs, line)) {
        // キーと値の区切りを検索
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        // キーを取得
        std::string key = line.substr(0, colonPos);
        // クォーテーションを除去
        size_t firstQuote = key.find('"');
        size_t lastQuote = key.rfind('"');
        if (firstQuote == std::string::npos || lastQuote == std::string::npos || firstQuote == lastQuote) continue;
        key = key.substr(firstQuote + 1, lastQuote - firstQuote - 1);

        // 値の部分を取得
        std::string valStr = line.substr(colonPos + 1);
        // 末尾のカンマやスペース、閉じブラケット等を除去
        while (!valStr.empty() && (valStr.back() == ',' || valStr.back() == ' ' || valStr.back() == '\r' || valStr.back() == '\n' || valStr.back() == '}')) {
            valStr.pop_back();
        }
        // 先頭のスペースを除去
        size_t firstNonSpace = valStr.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos) {
            valStr = valStr.substr(firstNonSpace);
        }

        ExplosionConfig* config = &commonConfig_;
		const struct { EffectTarget target; const char* key; } profiles[] = {
			{ EffectTarget::GroundEnemy, "ground" },
			{ EffectTarget::JammerEnemy, "jammer" },
			{ EffectTarget::Boss, "boss" },
			{ EffectTarget::Player, "player" },
		};
		bool isUseCommonSetting = false;
		for (const auto& profile : profiles) {
			const std::string useCommonKey = std::string(profile.key) + "UseCommon";
			const std::string prefix = std::string(profile.key) + "_";
			if (key == useCommonKey) {
				useCommonConfig_[static_cast<size_t>(profile.target)] = valStr == "true" || valStr == "1";
				hasProfileSettings = true;
				isUseCommonSetting = true;
				break;
			}
			if (key.rfind(prefix, 0) == 0) {
				config = &targetConfigs_[static_cast<size_t>(profile.target)];
				key = key.substr(prefix.size());
				break;
			}
		}
		if (isUseCommonSetting || key == "schemaVersion") {
			continue;
		}

        if (key == "count") {
            config->count = std::stoi(valStr);
        } else if (key == "color") {
            // [r, g, b, a] のパース
            size_t openBracket = valStr.find('[');
            size_t closeBracket = valStr.find(']');
            if (openBracket != std::string::npos && closeBracket != std::string::npos) {
                std::string arrayContent = valStr.substr(openBracket + 1, closeBracket - openBracket - 1);
                std::stringstream ss(arrayContent);
                std::string item;
                int i = 0;
                while (std::getline(ss, item, ',') && i < 4) {
					config->color[i++] = std::stof(item);
                }
            }
        } else if (key == "speed") {
            config->speed = std::stof(valStr);
        } else if (key == "speedVariance") {
            config->speedVariance = std::stof(valStr);
        } else if (key == "scale") {
            config->scale = std::stof(valStr);
        } else if (key == "scaleVariance") {
            config->scaleVariance = std::stof(valStr);
        } else if (key == "lifeTimeMin") {
            config->lifeTimeMin = std::stof(valStr);
        } else if (key == "lifeTimeMax") {
            config->lifeTimeMax = std::stof(valStr);
        } else if (key == "posVariance") {
			config->posVariance = std::stof(valStr);
		} else if (key == "destructionParticleType") {
			config->destructionParticleType = (std::clamp)(std::stoi(valStr), 0, 3);
		} else if (key == "smokeColor") {
			size_t openBracket = valStr.find('[');
			size_t closeBracket = valStr.find(']');
			if (openBracket != std::string::npos && closeBracket != std::string::npos) {
				std::stringstream ss(valStr.substr(openBracket + 1, closeBracket - openBracket - 1));
				std::string item;
				int i = 0;
				while (std::getline(ss, item, ',') && i < 4) {
					config->smokeColor[i++] = std::stof(item);
				}
			}
		} else if (key == "flameToSmokeScale") {
			config->flameToSmokeScale = std::stof(valStr);
		} else if (key == "flameToSmokeTransition") {
			config->flameToSmokeTransition = std::stof(valStr);
        }
    }

	// 旧形式（共通設定のみ）のJSONも従来どおり読めるようにする。
	if (!hasProfileSettings) {
		for (ExplosionConfig& config : targetConfigs_) {
			config = commonConfig_;
		}
		useCommonConfig_.fill(true);
	}
}
