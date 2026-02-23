#pragma once

#include "pch.h"
#include "Season.h"
#include "Config.h"
#include "RegionScanner.h"

#include <mutex>

namespace SWF {

    class WeatherManager {
    public:
        static WeatherManager& GetSingleton() {
            static WeatherManager instance;
            return instance;
        }

        void Update();

        void RestoreBaseChances();

        void SetSeasonOverride(Season season);
        void ClearSeasonOverride();

        // Get current effective season
        Season GetCurrentSeason() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return currentSeason_;
        }
        Season GetSeasonOverride() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return seasonOverride_;
        }
        bool HasSeasonOverride() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return hasSeasonOverride_;
        }

        RE::TESWorldSpace* GetCurrentWorldSpace() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return currentWorldSpace_;
        }

        // Status
        bool IsActive() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return isActive_;
        }
        std::string GetStatusString() const;

        // Force weather refresh on next update
        void ForceRefresh() { forceRefresh_.store(true, std::memory_order_relaxed); }

    private:
        WeatherManager() = default;
        ~WeatherManager() = default;
        WeatherManager(const WeatherManager&) = delete;
        WeatherManager& operator=(const WeatherManager&) = delete;

        bool IsInManagedWorldSpace() const;

        // Get the player's current worldspace (null if interior or unavailable)
        RE::TESWorldSpace* GetPlayerWorldSpace() const;

        void ApplySeasonToRegions(Season season);

        Season              currentSeason_    = Season::kWinter;
        Season              seasonOverride_   = Season::kWinter;
        bool                hasSeasonOverride_ = false;
        bool                isActive_          = false;
        std::atomic<bool>   forceRefresh_      = false;
        bool                hasApplied_        = false;  
        Season              lastAppliedSeason_ = Season::kWinter;
        RE::TESWorldSpace*  currentWorldSpace_ = nullptr;
        mutable std::mutex       mutex_;
    };
}
