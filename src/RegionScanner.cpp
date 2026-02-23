#include "RegionScanner.h"
#include "Config.h"

#include <unordered_map>
#include <unordered_set>

namespace SWF {

    WeatherClass RegionScanner::ClassifyWeather(RE::TESWeather* weather) {
        if (!weather) return WeatherClass::kUnknown;

        if (weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kSnow))
            return WeatherClass::kSnow;
        if (weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy))
            return WeatherClass::kRainy;
        if (weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kCloudy))
            return WeatherClass::kCloudy;
        if (weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kPleasant))
            return WeatherClass::kPleasant;

        return WeatherClass::kUnknown;
    }

    std::string RegionScanner::GetWeatherName(RE::TESWeather* weather) {
        if (!weather) return "None";

        auto editorID = weather->GetFormEditorID();
        if (editorID && editorID[0] != '\0') {
            return editorID;
        }

        // Fallback to FormID
        char buf[32];
        snprintf(buf, sizeof(buf), "Weather [%08X]", weather->GetFormID());
        return buf;
    }

    std::string RegionScanner::GetRegionName(RE::TESRegion* region) {
        if (!region) return "None";

        auto editorID = region->GetFormEditorID();
        if (editorID && editorID[0] != '\0') {
            return editorID;
        }

        char buf[32];
        snprintf(buf, sizeof(buf), "Region [%08X]", region->GetFormID());
        return buf;
    }

    void RegionScanner::ScanAllRegions() {
        std::lock_guard<std::mutex> lock(mutex_);

        regionInfos_.clear();
        uniqueWeathers_.clear();

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            logs::error("RegionScanner: TESDataHandler not available");
            return;
        }

        auto& regions = dataHandler->GetFormArray<RE::TESRegion>();
        std::unordered_set<RE::FormID> seenWeathers;

        logs::info("RegionScanner: Scanning {} total region records...", regions.size());

        for (auto* region : regions) {
            if (!region) continue;
            if (!region->dataList) continue;

            // Find weather data in this region
            RE::TESRegionDataWeather* weatherData = nullptr;

            for (auto& data : region->dataList->regionDataList) {
                if (data && data->GetType() == RE::TESRegionData::Type::kWeather) {
                    weatherData = static_cast<RE::TESRegionDataWeather*>(data);
                    break;
                }
            }

            if (!weatherData) continue;

            RegionWeatherInfo info;
            info.region      = region;
            info.weatherData = weatherData;
            info.worldSpace  = region->worldSpace;
            info.editorID    = GetRegionName(region);
            info.totalBaseChance = 0;

            // Iterate weather types in this region
            for (auto& wt : weatherData->weatherTypes) {
                if (!wt) continue;

                RegionWeatherEntry entry;
                entry.weather        = wt->weather;
                entry.baseChance     = wt->chance;
                entry.global         = wt->global;
                entry.classification = ClassifyWeather(wt->weather);

                info.totalBaseChance += entry.baseChance;
                info.originalWeatherEntries.push_back(entry);

                // Track unique weathers
                if (wt->weather && seenWeathers.find(wt->weather->GetFormID()) == seenWeathers.end()) {
                    seenWeathers.insert(wt->weather->GetFormID());
                    uniqueWeathers_.push_back(wt->weather);
                }
            }

            if (!info.originalWeatherEntries.empty()) {
                logs::info("  Region '{}' [{}]: {} weather entries, worldspace={}",
                    info.editorID,
                    fmt::format("{:08X}", region->GetFormID()),
                    info.originalWeatherEntries.size(),
                    info.worldSpace ? info.worldSpace->GetFormEditorID() : "none");
                regionInfos_.push_back(std::move(info));
            }
        }

        logs::info("RegionScanner: Found {} regions with weather data, {} unique weather forms",
            regionInfos_.size(), uniqueWeathers_.size());

        // Log weather
        std::uint32_t pleasant = 0, cloudy = 0, rainy = 0, snow = 0, unknown = 0;
        for (auto* w : uniqueWeathers_) {
            switch (ClassifyWeather(w)) {
                case WeatherClass::kPleasant: pleasant++; break;
                case WeatherClass::kCloudy:   cloudy++; break;
                case WeatherClass::kRainy:    rainy++; break;
                case WeatherClass::kSnow:     snow++; break;
                default:                      unknown++; break;
            }
        }
        logs::info("  Weather classification: {} pleasant, {} cloudy, {} rainy, {} snow, {} unknown",
            pleasant, cloudy, rainy, snow, unknown);
    }

    void RegionScanner::InjectMissingWeathers() {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& config = ConfigManager::GetSingleton().GetConfig();

        // Step 1: Build a per-worldspace pool of all weathers found in any region.
        // Key = worldspace FormID, Value = map of weather FormID → weather pointer.
        std::unordered_map<RE::FormID, std::unordered_map<RE::FormID, RE::TESWeather*>> wsWeatherPools;

        for (const auto& info : regionInfos_) {
            if (!info.worldSpace) continue;
            auto wsID = info.worldSpace->GetFormEditorID();
            if (!wsID || !config.IsWorldspaceEnabled(wsID)) continue;

            auto wsFormID = info.worldSpace->GetFormID();
            auto& pool = wsWeatherPools[wsFormID];

            for (const auto& entry : info.originalWeatherEntries) {
                if (entry.weather) {
                    pool[entry.weather->GetFormID()] = entry.weather;
                }
            }
        }

        // Step 2: For each region in an enabled worldspace, inject weathers
        // that exist in the pool but not in this region's list.
        std::uint32_t totalInjected = 0;

        for (auto& info : regionInfos_) {
            if (!info.worldSpace || !info.weatherData) continue;
            auto wsID = info.worldSpace->GetFormEditorID();
            if (!wsID || !config.IsWorldspaceEnabled(wsID)) continue;

            auto wsFormID = info.worldSpace->GetFormID();
            auto poolIt = wsWeatherPools.find(wsFormID);
            if (poolIt == wsWeatherPools.end()) continue;

            // Record original entry count before injection
            info.originalEntryCount = info.originalWeatherEntries.size();

            // Collect weather FormIDs already present in this region
            std::unordered_set<RE::FormID> existing;
            for (const auto& entry : info.originalWeatherEntries) {
                if (entry.weather) existing.insert(entry.weather->GetFormID());
            }

            // Inject missing weathers with base chance 0.
            // Skip weathers classified as kUnknown — these are typically
            // quest / scripted weathers (e.g. DA02) that should never play
            // from region tables.
            std::uint32_t injectedCount = 0;
            for (const auto& [weatherFormID, weather] : poolIt->second) {
                if (existing.count(weatherFormID)) continue;

                auto wclass = ClassifyWeather(weather);
                if (wclass == WeatherClass::kUnknown) {
                    logs::info("  Region '{}': skipping quest/unknown weather '{}' [{:08X}] from injection",
                        info.editorID, GetWeatherName(weather), weatherFormID);
                    continue;
                }

                // Allocate a new WeatherType and add it to the BSSimpleList
                auto* newEntry = new RE::WeatherType();
                newEntry->weather = weather;
                newEntry->chance  = 0;   // base chance 0 — season multipliers will set actual value
                newEntry->unk0C   = 0;
                newEntry->global  = nullptr;

                // Find the last element in the list, insert after it
                auto it = info.weatherData->weatherTypes.begin();
                auto lastIt = it;
                for (; it != info.weatherData->weatherTypes.end(); ++it) {
                    lastIt = it;
                }
                info.weatherData->weatherTypes.insert_after(lastIt, newEntry);

                // Also add to our tracking info
                RegionWeatherEntry trackEntry;
                trackEntry.weather        = weather;
                trackEntry.baseChance     = 0;
                trackEntry.global         = nullptr;
                trackEntry.classification = ClassifyWeather(weather);
                info.originalWeatherEntries.push_back(trackEntry);

                ++injectedCount;
                ++totalInjected;
            }

            if (injectedCount > 0) {
                info.hasInjectedWeathers = true;
                logs::info("  Region '{}': injected {} missing weathers from worldspace pool",
                    info.editorID, injectedCount);
            }
        }

        logs::info("RegionScanner: Injected {} total weather entries across all regions", totalInjected);
    }

    void RegionScanner::RemoveInjectedWeathers() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Rather than removing nodes from the BSSimpleList (which is fragile),
        // we simply zero out the chance on all injected entries.  Skyrim will
        // never pick a weather with chance 0.  The entries stay in the list
        // harmlessly and will be overwritten again on the next season apply.
        std::uint32_t totalZeroed = 0;

        for (auto& info : regionInfos_) {
            if (!info.hasInjectedWeathers || !info.weatherData) continue;

            // Walk the BSSimpleList to the injected entries (those past originalEntryCount)
            std::size_t idx = 0;
            for (auto& wt : info.weatherData->weatherTypes) {
                if (!wt) { ++idx; continue; }
                if (idx >= info.originalEntryCount) {
                    wt->chance = 0;
                    ++totalZeroed;
                }
                ++idx;
            }
        }

        logs::info("RegionScanner: Zeroed {} injected weather entries", totalZeroed);
    }
}
