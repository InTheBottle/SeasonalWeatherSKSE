#pragma once

#include "pch.h"
#include "Season.h"

#include <unordered_map>
#include <vector>
#include <mutex>

namespace SWF {

    struct RegionWeatherEntry {
        RE::TESWeather*   weather  = nullptr;
        std::uint32_t     baseChance = 0;     // original chance from the region record
        RE::TESGlobal*    global   = nullptr;  // optional global override
        WeatherClass      classification = WeatherClass::kUnknown;
    };

    struct RegionWeatherInfo {
        RE::TESRegion*                  region     = nullptr;
        RE::TESRegionDataWeather*       weatherData = nullptr;
        RE::TESWorldSpace*              worldSpace = nullptr;
        std::string                     editorID;
        std::vector<RegionWeatherEntry> originalWeatherEntries;  // snapshot of original weather list
        std::uint32_t                   totalBaseChance = 0;
    };

    class RegionScanner {
    public:
        static RegionScanner& GetSingleton() {
            static RegionScanner instance;
            return instance;
        }

        void ScanAllRegions();

        const std::vector<RegionWeatherInfo>& GetRegionWeatherInfos() const { return regionInfos_; }

        std::size_t GetWeatherRegionCount() const { return regionInfos_.size(); }

        std::size_t GetUniqueWeatherCount() const { return uniqueWeathers_.size(); }

        const std::vector<RE::TESWeather*>& GetUniqueWeathers() const { return uniqueWeathers_; }

        static WeatherClass ClassifyWeather(RE::TESWeather* weather);

        static std::string GetWeatherName(RE::TESWeather* weather);

        static std::string GetRegionName(RE::TESRegion* region);

    private:
        RegionScanner() = default;
        ~RegionScanner() = default;
        RegionScanner(const RegionScanner&) = delete;
        RegionScanner& operator=(const RegionScanner&) = delete;

        std::vector<RegionWeatherInfo> regionInfos_;
        std::vector<RE::TESWeather*>   uniqueWeathers_;
        mutable std::mutex             mutex_;
    };
}
