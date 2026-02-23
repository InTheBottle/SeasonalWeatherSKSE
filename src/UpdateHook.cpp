#include "UpdateHook.h"
#include "WeatherManager.h"
#include "Config.h"

namespace SWF {

    RE::BSEventNotifyControl UpdateHook::MenuEventSink::ProcessEvent(
        const RE::MenuOpenCloseEvent* a_event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) 
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        auto& config = ConfigManager::GetSingleton().GetConfig();
        if (!config.enabled) return RE::BSEventNotifyControl::kContinue;

        if (!a_event->opening) {
            std::string_view menuName(a_event->menuName);

            // Update after menus that may advance game time
            if (menuName == RE::SleepWaitMenu::MENU_NAME ||
                menuName == RE::MapMenu::MENU_NAME ||
                menuName == RE::LoadingMenu::MENU_NAME ||
                menuName == RE::FaderMenu::MENU_NAME) {

                WeatherManager::GetSingleton().ForceRefresh();

                if (config.debugMode) {
                    logs::info("UpdateHook: Weather refresh queued by menu close: {}", menuName);
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl UpdateHook::CellChangeEventSink::ProcessEvent(
        const RE::TESCellAttachDetachEvent* a_event,
        RE::BSTEventSource<RE::TESCellAttachDetachEvent>*)
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        auto& config = ConfigManager::GetSingleton().GetConfig();
        if (!config.enabled) return RE::BSEventNotifyControl::kContinue;

        if (a_event->attached) {
            WeatherManager::GetSingleton().ForceRefresh();

            if (config.debugMode) {
                logs::info("UpdateHook: Weather refresh queued by cell change");
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    void UpdateHook::Install() {
        if (installed_) return;

        // Register for menu open/close events
        auto* ui = RE::UI::GetSingleton();
        if (ui) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(&MenuEventSink::GetSingleton());
            logs::info("UpdateHook: Registered MenuOpenCloseEvent sink");
        }

        // Register for cell attach/detach events
        auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSourceHolder) {
            eventSourceHolder->AddEventSink<RE::TESCellAttachDetachEvent>(
                &CellChangeEventSink::GetSingleton());
            logs::info("UpdateHook: Registered TESCellAttachDetachEvent sink");
        }

        // Install frame hook for periodic updating while the player is idle
        SKSE::GetTrampoline().create(14);
        MainUpdateHook::func = reinterpret_cast<MainUpdateHook::FuncType>(
            SKSE::GetTrampoline().write_call<5>(
                MainUpdateHook::id.address(),
                reinterpret_cast<std::uintptr_t>(MainUpdateHook::thunk)));
        logs::info("UpdateHook: Installed Main::Update frame hook");

        // Do an initial weather update
        WeatherManager::GetSingleton().Update();

        installed_ = true;
        logs::info("UpdateHook: Installation complete");
    }

    void UpdateHook::MainUpdateHook::thunk(RE::Main* a_this, float a_interval) {
        func(a_this, a_interval);  // call original via saved pointer

        auto& config = ConfigManager::GetSingleton().GetConfig();
        if (config.enabled) {
            WeatherManager::GetSingleton().Update();
        }
    }
}
