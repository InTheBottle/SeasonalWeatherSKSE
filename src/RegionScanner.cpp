#include "RegionScanner.h"

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
}
