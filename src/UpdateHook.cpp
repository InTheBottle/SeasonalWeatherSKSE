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
                WeatherManager::GetSingleton().Update();

                if (config.debugMode) {
                    logs::info("UpdateHook: Weather refresh triggered by menu close: {}", menuName);
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
            WeatherManager::GetSingleton().Update();

            if (config.debugMode) {
                logs::info("UpdateHook: Weather refresh triggered by cell change");
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

        // Do an initial weather update
        WeatherManager::GetSingleton().Update();

        installed_ = true;
        logs::info("UpdateHook: Installation complete");
    }
}
