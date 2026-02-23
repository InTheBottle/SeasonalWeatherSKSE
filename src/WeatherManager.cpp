#include "WeatherManager.h"
#include "Config.h"
#include "RegionScanner.h"

namespace SWF {

    void WeatherManager::SetSeasonOverride(Season season) {
        std::lock_guard<std::mutex> lock(mutex_);
        seasonOverride_ = season;
        hasSeasonOverride_ = true;
        forceRefresh_.store(true, std::memory_order_relaxed);
        logs::info("Season override set to: {}", SeasonToString(season));
    }

    void WeatherManager::ClearSeasonOverride() {
        std::lock_guard<std::mutex> lock(mutex_);
        hasSeasonOverride_ = false;
        forceRefresh_.store(true, std::memory_order_relaxed);
        logs::info("Season override cleared");
    }

    std::string WeatherManager::GetStatusString() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!isActive_) return "Inactive (not in a managed exterior worldspace, or disabled)";

        std::string status = "Active - ";
        status += SeasonToString(currentSeason_);
        if (hasSeasonOverride_) status += " (Override)";
        if (currentWorldSpace_) {
            auto editorID = currentWorldSpace_->GetFormEditorID();
            if (editorID) status += std::string(" | ") + editorID;
        }
        return status;
    }

    RE::TESWorldSpace* WeatherManager::GetPlayerWorldSpace() const {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return nullptr;

        auto* cell = player->GetParentCell();
        if (!cell || cell->IsInteriorCell()) return nullptr;

        // Get worldspace from the player character
        return player->GetWorldspace();
    }

    bool WeatherManager::IsInManagedWorldSpace() const {
        auto* ws = GetPlayerWorldSpace();
        if (!ws) return false;

        auto editorID = ws->GetFormEditorID();
        if (!editorID) return false;

        return ConfigManager::GetSingleton().GetConfig().IsWorldspaceEnabled(editorID);
    }

    void WeatherManager::ApplySeasonToRegions(Season season) {
        auto& config = ConfigManager::GetSingleton().GetConfig();
        const auto& mults = config.GetMultipliers(season);

        auto& regionInfos = RegionScanner::GetSingleton().GetRegionWeatherInfos();
        std::uint32_t regionsModified = 0;

        for (const auto& info : regionInfos) {
            // Only touch regions that belong to an enabled worldspace.
            // Regions with no associated worldspace pointer are skipped — we
            // can't determine where they apply, so it's safer to leave them alone.
            if (!info.worldSpace) continue;
            auto wsID = info.worldSpace->GetFormEditorID();
            if (!wsID || !config.IsWorldspaceEnabled(wsID)) continue;

            if (!info.weatherData) continue;

            std::size_t i = 0;
            for (auto& wt : info.weatherData->weatherTypes) {
                if (!wt || i >= info.originalWeatherEntries.size()) break;
                const auto& orig = info.originalWeatherEntries[i];

                // For injected weathers (baseChance == 0), use a small base value
                // so season multipliers can give them a non-zero chance.
                // This allows e.g. snow to appear in regions that don't normally have it.
                constexpr float kInjectedBaseChance = 10.0f;
                float base = (orig.baseChance > 0)
                    ? static_cast<float>(orig.baseChance)
                    : kInjectedBaseChance;

                float adjusted = base;

                // Apply TESGlobal scale if the region record carries one.
                if (orig.global) {
                    adjusted *= orig.global->value;
                }

                // Apply the season multiplier for this weather's classification.
                switch (orig.classification) {
                    case WeatherClass::kPleasant: adjusted *= mults.pleasantMult; break;
                    case WeatherClass::kCloudy:   adjusted *= mults.cloudyMult;   break;
                    case WeatherClass::kRainy:    adjusted *= mults.rainyMult;    break;
                    case WeatherClass::kSnow:     adjusted *= mults.snowMult;     break;
                    default: break;
                }

                // If this was an injected entry with base 0, and the multiplier is 0,
                // make sure we write back 0 (not kInjectedBaseChance * 0 = 0, which is fine,
                // but explicitly zero for clarity).
                if (orig.baseChance == 0) {
                    float mult = 0.0f;
                    switch (orig.classification) {
                        case WeatherClass::kPleasant: mult = mults.pleasantMult; break;
                        case WeatherClass::kCloudy:   mult = mults.cloudyMult;   break;
                        case WeatherClass::kRainy:    mult = mults.rainyMult;    break;
                        case WeatherClass::kSnow:     mult = mults.snowMult;     break;
                        default: mult = 0.0f; break;
                    }
                    if (mult <= 0.0f) adjusted = 0.0f;
                }

                // Write back; clamp to zero but allow any positive float —
                // Skyrim normalises the table internally before selection.
                auto finalChance = static_cast<std::uint32_t>((std::max)(adjusted, 0.0f));
                wt->chance = finalChance;

                if (config.debugMode && finalChance > 0) {
                    auto wname = RegionScanner::GetWeatherName(orig.weather);
                    logs::info("  {} [{}]: base={} -> chance={} (mult applied for {})",
                        info.editorID, wname, orig.baseChance, finalChance,
                        WeatherClassToString(orig.classification));
                }
                ++i;
            }
            ++regionsModified;
        }

        logs::info("WeatherManager: Applied '{}' season weights to {} region records",
            SeasonToString(season), regionsModified);

        // Force Skyrim to re-evaluate weather from the modified table.
        // Without this, the engine keeps its already-selected weather.
        auto* sky = RE::Sky::GetSingleton();
        if (sky) {
            sky->ResetWeather();
            logs::info("WeatherManager: Called Sky::ResetWeather() to force re-evaluation");
        }
    }

    void WeatherManager::RestoreBaseChances() {
        // First remove any injected weather entries from the region lists
        RegionScanner::GetSingleton().RemoveInjectedWeathers();

        auto& regionInfos = RegionScanner::GetSingleton().GetRegionWeatherInfos();

        for (const auto& info : regionInfos) {
            if (!info.weatherData) continue;

            std::size_t i = 0;
            for (auto& wt : info.weatherData->weatherTypes) {
                if (!wt || i >= info.originalWeatherEntries.size()) break;
                wt->chance = info.originalWeatherEntries[i].baseChance;
                ++i;
            }
        }

        logs::info("WeatherManager: Restored original base chances to all region records");
    }

    void WeatherManager::Update() {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& config = ConfigManager::GetSingleton().GetConfig();

        if (!config.enabled) {
            if (hasApplied_) {
                RestoreBaseChances();
                hasApplied_ = false;
            }
            isActive_ = false;
            return;
        }

        currentWorldSpace_ = GetPlayerWorldSpace();
        isActive_ = IsInManagedWorldSpace();

        // Determine effective season.
        Season effectiveSeason;
        if (hasSeasonOverride_) {
            effectiveSeason = seasonOverride_;
        } else {
            effectiveSeason = config.GetSeasonForMonth(GetCurrentMonth());
        }

        bool seasonChanged = (effectiveSeason != currentSeason_);
        bool needsApply    = !hasApplied_ || seasonChanged ||
                             forceRefresh_.load(std::memory_order_relaxed);

        if (!needsApply) {
            return;
        }

        if (seasonChanged && hasApplied_ && config.enableNotifications) {
            logs::info("WeatherManager: Season changed to {}", SeasonToString(effectiveSeason));
        }

        currentSeason_ = effectiveSeason;

        ApplySeasonToRegions(effectiveSeason);

        hasApplied_        = true;
        lastAppliedSeason_ = effectiveSeason;
        forceRefresh_.store(false, std::memory_order_relaxed);
    }
}
