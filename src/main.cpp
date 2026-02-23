#include "Config.h"
#include "RegionScanner.h"
#include "WeatherManager.h"
#include "UpdateHook.h"
#include "MenuUI.h"

namespace {

    void InitializeLogging() {
        auto path = logs::log_directory();
        if (!path) return;

        *path /= "SeasonalWeatherFramework.log"sv;
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

        auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v"s);
    }

    void OnDataLoaded() {
        logs::info("=== Seasonal Weather Framework: Data Loaded ===");

        // Load config
        SWF::ConfigManager::GetSingleton().Load();

        // Scan all region records from all loaded mods
        SWF::RegionScanner::GetSingleton().ScanAllRegions();

        // Install the update hook
        SWF::UpdateHook::GetSingleton().Install();

        logs::info("=== Seasonal Weather Framework: Initialization Complete ===");
    }

    void OnPostLoad() {
        // Register with SKSEMenuFramework (must be done after other plugins load)
        SWF::MenuUI::GetSingleton().Register();
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                OnDataLoaded();
                break;
            case SKSE::MessagingInterface::kPostLoad:
                OnPostLoad();
                break;
            default:
                break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);

    InitializeLogging();

    logs::info("Seasonal Weather Framework SKSE v1.0.0 loading...");
    logs::info("  Game version: {}", a_skse->RuntimeVersion().string());

    // Register for SKSE messages
    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", MessageHandler)) {
        logs::error("Failed to register SKSE message listener");
        return false;
    }

    logs::info("Seasonal Weather Framework SKSE: Registered message listener");

    return true;
}
