#pragma once

#include "pch.h"

namespace SWF {

    class MenuUI {
    public:
        static MenuUI& GetSingleton() {
            static MenuUI instance;
            return instance;
        }

        // Register with SKSEMenuFramework. Call after kPostLoad.
        void Register();

        bool IsRegistered() const { return registered_; }

    private:
        MenuUI() = default;
        ~MenuUI() = default;
        MenuUI(const MenuUI&) = delete;
        MenuUI& operator=(const MenuUI&) = delete;

        bool registered_ = false;

        // Render callbacks (static, called by SKSEMenuFramework)
        static void __stdcall RenderSettings();
        static void __stdcall RenderStatus();
        static void __stdcall RenderRegionBrowser();
        static void __stdcall RenderDebug();

        // Helpers
        static void RenderSeasonMultipliers(const char* label, int seasonIdx);
        static void RenderWeatherList(const struct RegionWeatherInfo& info);
    };
}
