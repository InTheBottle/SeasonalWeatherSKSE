#include "Config.h"
#include "RegionScanner.h"
#include "WeatherManager.h"
#include "UpdateHook.h"
#include "MenuUI.h"

namespace {

    void InitializeLogging() {
        auto path = logs::log_directory();
        if (!path) return;

        // Ensure the log directory exists
        std::filesystem::create_directories(*path);

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

        // Inject missing weathers so every weather type can play in every region
        SWF::RegionScanner::GetSingleton().InjectMissingWeathers();

        // Install the update hook
        SWF::UpdateHook::GetSingleton().Install();

        logs::info("=== Seasonal Weather Framework: Initialization Complete ===");
    }

    void OnPostPostLoad() {
        // Register with SKSEMenuFramework after all plugins have finished kPostLoad,
        // so SKSEMenuFramework.dll is guaranteed to be loaded into the process.
        SWF::MenuUI::GetSingleton().Register();
    }

    void OnGameLoaded() {
        // Runs after loading a save or starting a new game
        logs::info("OnGameLoaded: Refreshing weather for loaded game");
        SWF::WeatherManager::GetSingleton().ForceRefresh();
        SWF::WeatherManager::GetSingleton().Update();
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                OnDataLoaded();
                break;
            case SKSE::MessagingInterface::kPostPostLoad:
                OnPostPostLoad();
                break;
            case SKSE::MessagingInterface::kPostLoadGame:
            case SKSE::MessagingInterface::kNewGame:
                OnGameLoaded();
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

    logs::info("=== Seasonal Weather Framework: SKSEPluginLoad start ===");
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
