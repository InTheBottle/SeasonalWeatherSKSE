#pragma once

#include "pch.h"

namespace SWF {

    enum class Season : std::uint32_t {
        kSpring = 0,
        kSummer = 1,
        kFall   = 2,
        kWinter = 3,

        kTotal  = 4
    };

    inline const char* SeasonToString(Season season) {
        switch (season) {
            case Season::kSpring: return "Spring";
            case Season::kSummer: return "Summer";
            case Season::kFall:   return "Fall";
            case Season::kWinter: return "Winter";
            default:              return "Unknown";
        }
    }

    // Tamriel month names
    inline const char* MonthToString(std::uint32_t month) {
        switch (month) {
            case 0:  return "Morning Star";
            case 1:  return "Sun's Dawn";
            case 2:  return "First Seed";
            case 3:  return "Rain's Hand";
            case 4:  return "Second Seed";
            case 5:  return "Midyear";
            case 6:  return "Sun's Height";
            case 7:  return "Last Seed";
            case 8:  return "Hearthfire";
            case 9:  return "Frostfall";
            case 10: return "Sun's Dusk";
            case 11: return "Evening Star";
            default: return "Unknown";
        }
    }

    inline Season GetSeasonFromMonth(std::uint32_t month) {
        if (month >= 2 && month <= 4)  return Season::kSpring;
        if (month >= 5 && month <= 7)  return Season::kSummer;
        if (month >= 8 && month <= 10) return Season::kFall;
        return Season::kWinter; // 0, 1, 11
    }

    inline std::uint32_t GetCurrentMonth() {
        auto* calendar = RE::Calendar::GetSingleton();
        if (!calendar) return 0;
        return calendar->GetMonth();
    }

    enum class WeatherClass : std::uint32_t {
        kPleasant = 0,
        kCloudy   = 1,
        kRainy    = 2,
        kSnow     = 3,
        kUnknown  = 4
    };

    inline const char* WeatherClassToString(WeatherClass wc) {
        switch (wc) {
            case WeatherClass::kPleasant: return "Pleasant";
            case WeatherClass::kCloudy:   return "Cloudy";
            case WeatherClass::kRainy:    return "Rainy";
            case WeatherClass::kSnow:     return "Snow";
            default:                      return "Unknown";
        }
    }
}
