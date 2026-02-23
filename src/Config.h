#pragma once

#include "pch.h"
#include "Season.h"

#include <fstream>
#include <mutex>

namespace SWF {

    struct SeasonWeatherMultipliers {
        float pleasantMult = 1.0f;
        float cloudyMult   = 1.0f;
        float rainyMult    = 1.0f;
        float snowMult     = 1.0f;
    };

    struct Config {
        // General
        bool  enabled             = true;
        bool  enableNotifications = true;     // show notification on season change

        // Season month ranges (configurable)
        std::uint32_t springStart = 2;  // First Seed
        std::uint32_t springEnd   = 4;  // Second Seed
        std::uint32_t summerStart = 5;  // Midyear
        std::uint32_t summerEnd   = 7;  // Last Seed
        std::uint32_t fallStart   = 8;  // Hearthfire
        std::uint32_t fallEnd     = 10; // Sun's Dusk

        // Per-season weather weight multipliers (applied to region base chances)
        SeasonWeatherMultipliers springMultipliers = { 1.2f, 1.0f, 1.5f, 0.1f };  // More rain in spring, almost no snow
        SeasonWeatherMultipliers summerMultipliers = { 1.5f, 0.8f, 0.5f, 0.0f };  // Mostly pleasant, no snow
        SeasonWeatherMultipliers fallMultipliers   = { 0.8f, 1.3f, 1.2f, 0.5f };  // More clouds and rain, some snow
        SeasonWeatherMultipliers winterMultipliers = { 0.3f, 1.0f, 0.8f, 2.5f };  // Heavy snow, few pleasant days

        // Worldspace settings — add any modded worldspace EditorID to this set
        // to enable seasonal weather there. Serialised as a comma-separated list
        // in the INI under [Worldspaces] sEnabledWorldspaces.
        std::unordered_set<std::string> enabledWorldspaces = { "Tamriel", "DLC2SolstheimWorld" };

        bool IsWorldspaceEnabled(std::string_view name) const {
            return enabledWorldspaces.count(std::string(name)) > 0;
        }

        // Advanced
        bool  debugMode              = false;

        Season GetSeasonForMonth(std::uint32_t month) const {
            if (month >= springStart && month <= springEnd)  return Season::kSpring;
            if (month >= summerStart && month <= summerEnd)  return Season::kSummer;
            if (month >= fallStart   && month <= fallEnd)    return Season::kFall;
            return Season::kWinter;
        }

        const SeasonWeatherMultipliers& GetMultipliers(Season season) const {
            switch (season) {
                case Season::kSpring: return springMultipliers;
                case Season::kSummer: return summerMultipliers;
                case Season::kFall:   return fallMultipliers;
                case Season::kWinter: return winterMultipliers;
                default:              return winterMultipliers;
            }
        }

        SeasonWeatherMultipliers& GetMultipliersMut(Season season) {
            switch (season) {
                case Season::kSpring: return springMultipliers;
                case Season::kSummer: return summerMultipliers;
                case Season::kFall:   return fallMultipliers;
                case Season::kWinter: return winterMultipliers;
                default:              return winterMultipliers;
            }
        }
    };

    class ConfigManager {
    public:
        static ConfigManager& GetSingleton() {
            static ConfigManager instance;
            return instance;
        }

        Config& GetConfig() { return config_; }
        const Config& GetConfig() const { return config_; }

        void Load();
        void Save();

        std::string GetConfigPath() const;

    private:
        ConfigManager() = default;
        ~ConfigManager() = default;
        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;

        Config config_;
        mutable std::mutex mutex_;
    };
}
