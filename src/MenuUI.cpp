#include "MenuUI.h"
#include "Config.h"
#include "Season.h"
#include "WeatherManager.h"
#include "RegionScanner.h"

#include <SKSEMenuFramework.h>

namespace SWF {

    void MenuUI::Register() {
        if (registered_) return;

        if (!SKSEMenuFramework::IsInstalled()) {
            logs::info("MenuUI: SKSEMenuFramework is not installed, menu will not be available");
            return;
        }

        // Re-fetch the module handle here — the file-scope static in the header
        // is captured at DLL load time, before SKSEMenuFramework is in the process.
        menuFramework = GetModuleHandle(L"SKSEMenuFramework");
        if (!menuFramework) {
            logs::error("MenuUI: SKSEMenuFramework.dll is on disk but not loaded into the process");
            return;
        }

        SKSEMenuFramework::SetSection("Seasonal Weather");

        SKSEMenuFramework::AddSectionItem("Status", RenderStatus);
        SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
        SKSEMenuFramework::AddSectionItem("Region Browser", RenderRegionBrowser);
        SKSEMenuFramework::AddSectionItem("Debug", RenderDebug);

        registered_ = true;
        logs::info("MenuUI: Registered with SKSEMenuFramework");
    }

    void __stdcall MenuUI::RenderStatus() {
        auto& wm = WeatherManager::GetSingleton();

        ImGuiMCP::SeparatorText("Current Status");

        // Active state
        if (wm.IsActive()) {
            ImGuiMCP::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "ACTIVE");
        } else {
            ImGuiMCP::TextColored({ 1.0f, 0.3f, 0.3f, 1.0f }, "INACTIVE");
        }

        ImGuiMCP::Separator();

        // Season info
        auto season = wm.GetCurrentSeason();
        ImGuiMCP::Text("Current Season: %s", SeasonToString(season));

        auto month = GetCurrentMonth();
        ImGuiMCP::Text("Current Month: %s (index %d)", MonthToString(month), month);

        if (wm.HasSeasonOverride()) {
            ImGuiMCP::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "Season Override: %s",
                SeasonToString(wm.GetSeasonOverride()));
        }

        ImGuiMCP::Separator();

        auto* sky = RE::Sky::GetSingleton();
        if (sky && sky->currentWeather) {
            auto name   = RegionScanner::GetWeatherName(sky->currentWeather);
            auto wclass = RegionScanner::ClassifyWeather(sky->currentWeather);
            ImGuiMCP::Text("Current Weather: %s (%s)", name.c_str(), WeatherClassToString(wclass));
        } else {
            ImGuiMCP::Text("Current Weather: None");
        }

        if (sky && sky->region) {
            auto regionName = RegionScanner::GetRegionName(sky->region);
            ImGuiMCP::Text("Current Region: %s", regionName.c_str());

            auto& regionInfos = RegionScanner::GetSingleton().GetRegionWeatherInfos();
            for (const auto& info : regionInfos) {
                if (info.region == sky->region) {
                    ImGuiMCP::Text("  Weather entries: %d", (int)info.originalWeatherEntries.size());
                    break;
                }
            }
        } else {
            ImGuiMCP::Text("Current Region: None detected");
        }

        // Worldspace
        auto* ws = wm.GetCurrentWorldSpace();
        if (ws) {
            auto editorID = ws->GetFormEditorID();
            ImGuiMCP::Text("Worldspace: %s", editorID ? editorID : "Unknown");
        }

        ImGuiMCP::Separator();

        auto& scanner = RegionScanner::GetSingleton();
        ImGuiMCP::Text("Scanned Regions: %d", (int)scanner.GetWeatherRegionCount());
        ImGuiMCP::Text("Unique Weathers: %d", (int)scanner.GetUniqueWeatherCount());

        ImGuiMCP::Spacing();

        if (ImGuiMCP::Button("Re-apply Season Weights")) {
            wm.ForceRefresh();
            wm.Update();
        }
    }

    void __stdcall MenuUI::RenderSettings() {
        auto& config = ConfigManager::GetSingleton().GetConfig();

        ImGuiMCP::SeparatorText("General");

        ImGuiMCP::Checkbox("Enabled", &config.enabled);
        ImGuiMCP::Checkbox("Show Season Change Notifications", &config.enableNotifications);

        ImGuiMCP::Separator();
        ImGuiMCP::SeparatorText("Worldspaces");
        ImGuiMCP::Text("Seasonal weather is applied to every region in these worldspaces.");
        ImGuiMCP::Text("Use the exact EditorID as it appears in xEdit (case-sensitive).");
        ImGuiMCP::Spacing();

        std::vector<std::string> toRemove;
        for (const auto& ws : config.enabledWorldspaces) {
            ImGuiMCP::Text("  %s", ws.c_str());
            ImGuiMCP::SameLine();
            auto removeLabel = std::string("Remove##") + ws;
            if (ImGuiMCP::SmallButton(removeLabel.c_str())) {
                toRemove.push_back(ws);
            }
        }
        for (const auto& ws : toRemove) {
            config.enabledWorldspaces.erase(ws);
            WeatherManager::GetSingleton().ForceRefresh();
        }

        static char wsInputBuf[128] = {};
        ImGuiMCP::InputText("##wsInput", wsInputBuf, sizeof(wsInputBuf));
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Add Worldspace")) {
            std::string newWS = wsInputBuf;
            auto start = newWS.find_first_not_of(" \t");
            auto end   = newWS.find_last_not_of(" \t");
            if (start != std::string::npos) {
                newWS = newWS.substr(start, end - start + 1);
                if (!newWS.empty()) {
                    config.enabledWorldspaces.insert(newWS);
                    WeatherManager::GetSingleton().ForceRefresh();
                    wsInputBuf[0] = '\0';
                }
            }
        }

        ImGuiMCP::Separator();

        ImGuiMCP::SeparatorText("Season Month Ranges");
        ImGuiMCP::Text("Month indices: 0=Morning Star ... 11=Evening Star");

        int springStart = static_cast<int>(config.springStart);
        int springEnd   = static_cast<int>(config.springEnd);
        int summerStart = static_cast<int>(config.summerStart);
        int summerEnd   = static_cast<int>(config.summerEnd);
        int fallStart   = static_cast<int>(config.fallStart);
        int fallEnd     = static_cast<int>(config.fallEnd);

        if (ImGuiMCP::SliderInt("Spring Start", &springStart, 0, 11)) config.springStart = springStart;
        if (ImGuiMCP::SliderInt("Spring End",   &springEnd,   0, 11)) config.springEnd   = springEnd;
        if (ImGuiMCP::SliderInt("Summer Start", &summerStart, 0, 11)) config.summerStart  = summerStart;
        if (ImGuiMCP::SliderInt("Summer End",   &summerEnd,   0, 11)) config.summerEnd    = summerEnd;
        if (ImGuiMCP::SliderInt("Fall Start",   &fallStart,   0, 11)) config.fallStart    = fallStart;
        if (ImGuiMCP::SliderInt("Fall End",     &fallEnd,     0, 11)) config.fallEnd      = fallEnd;
        ImGuiMCP::Text("Winter = everything outside the above ranges");

        ImGuiMCP::Separator();
        ImGuiMCP::SeparatorText("Season Weather Multipliers");
        ImGuiMCP::Text("Multipliers adjust region weather chance per type per season.");
        ImGuiMCP::Text("> 1.0 = more likely, < 1.0 = less likely, 0.0 = never");

        RenderSeasonMultipliers("Spring", 0);
        RenderSeasonMultipliers("Summer", 1);
        RenderSeasonMultipliers("Fall",   2);
        RenderSeasonMultipliers("Winter", 3);

        ImGuiMCP::Spacing();
        ImGuiMCP::Separator();

        // Season override
        ImGuiMCP::SeparatorText("Season Override");

        auto& wm = WeatherManager::GetSingleton();
        int overrideIdx = wm.HasSeasonOverride()
            ? static_cast<int>(wm.GetSeasonOverride()) + 1  // 1-4
            : 0;                                             // 0 = Auto
        const char* overrideItems[] = { "Auto (Calendar)", "Spring", "Summer", "Fall", "Winter" };
        if (ImGuiMCP::Combo("Season", &overrideIdx, overrideItems, 5)) {
            if (overrideIdx == 0) {
                wm.ClearSeasonOverride();
            } else {
                wm.SetSeasonOverride(static_cast<Season>(overrideIdx - 1));
            }
        }

        ImGuiMCP::Spacing();

        // Save/Load buttons
        ImGuiMCP::Separator();
        if (ImGuiMCP::Button("Save Settings")) {
            ConfigManager::GetSingleton().Save();
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Load Settings")) {
            ConfigManager::GetSingleton().Load();
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Reset to Defaults")) {
            config = Config{};
        }
    }

    void MenuUI::RenderSeasonMultipliers(const char* label, int seasonIdx) {
        auto& config = ConfigManager::GetSingleton().GetConfig();
        auto& mults = config.GetMultipliersMut(static_cast<Season>(seasonIdx));

        if (ImGuiMCP::CollapsingHeader(label)) {
            ImGuiMCP::PushItemWidth(200);

            std::string id = std::string("##") + label;
            ImGuiMCP::SliderFloat(("Pleasant" + id).c_str(), &mults.pleasantMult, 0.0f, 5.0f, "%.2f");
            ImGuiMCP::SliderFloat(("Cloudy" + id).c_str(),   &mults.cloudyMult,   0.0f, 5.0f, "%.2f");
            ImGuiMCP::SliderFloat(("Rainy" + id).c_str(),    &mults.rainyMult,    0.0f, 5.0f, "%.2f");
            ImGuiMCP::SliderFloat(("Snow" + id).c_str(),     &mults.snowMult,     0.0f, 5.0f, "%.2f");

            ImGuiMCP::PopItemWidth();
        }
    }

    void MenuUI::RenderWeatherList(const RegionWeatherInfo& info) {
        if (ImGuiMCP::BeginTable("##weatherTable", 4,
            ImGuiMCP::ImGuiTableFlags_Borders | ImGuiMCP::ImGuiTableFlags_RowBg |
            ImGuiMCP::ImGuiTableFlags_SizingStretchProp)) {
            ImGuiMCP::TableSetupColumn("Weather");
            ImGuiMCP::TableSetupColumn("Type");
            ImGuiMCP::TableSetupColumn("Base Chance");
            ImGuiMCP::TableSetupColumn("FormID");
            ImGuiMCP::TableHeadersRow();

            for (const auto& entry : info.originalWeatherEntries) {
                ImGuiMCP::TableNextRow();
                ImGuiMCP::TableNextColumn();

                auto name = RegionScanner::GetWeatherName(entry.weather);
                ImGuiMCP::Text("%s", name.c_str());

                ImGuiMCP::TableNextColumn();
                ImGuiMCP::Text("%s", WeatherClassToString(entry.classification));

                ImGuiMCP::TableNextColumn();
                ImGuiMCP::Text("%u", entry.baseChance);

                ImGuiMCP::TableNextColumn();
                if (entry.weather) {
                    ImGuiMCP::Text("%08X", entry.weather->GetFormID());
                } else {
                    ImGuiMCP::Text("N/A");
                }
            }

            ImGuiMCP::EndTable();
        }
    }

    void __stdcall MenuUI::RenderRegionBrowser() {
        auto& scanner = RegionScanner::GetSingleton();
        auto& regionInfos = scanner.GetRegionWeatherInfos();

        ImGuiMCP::SeparatorText("Loaded Regions with Weather Data");
        ImGuiMCP::Text("Total: %d regions, %d unique weather forms",
            (int)scanner.GetWeatherRegionCount(), (int)scanner.GetUniqueWeatherCount());
        ImGuiMCP::Separator();

        for (const auto& info : regionInfos) {
            std::string header = info.editorID;
            if (info.worldSpace) {
                auto editorID = info.worldSpace->GetFormEditorID();
                if (editorID) {
                    header += " [" + std::string(editorID) + "]";
                }
            }
            header += " (" + std::to_string(info.originalWeatherEntries.size()) + " weathers)";

            if (ImGuiMCP::CollapsingHeader(header.c_str())) {
                ImGuiMCP::Text("Region FormID: %08X", info.region ? info.region->GetFormID() : 0);
                ImGuiMCP::Text("Total Base Chance: %u", info.totalBaseChance);
                ImGuiMCP::Spacing();

                RenderWeatherList(info);

                ImGuiMCP::Spacing();
            }
        }
    }

    void __stdcall MenuUI::RenderDebug() {
        auto& config = ConfigManager::GetSingleton().GetConfig();

        ImGuiMCP::SeparatorText("Debug");

        ImGuiMCP::Checkbox("Debug Mode (verbose logging)", &config.debugMode);
        ImGuiMCP::Separator();

        // Calendar info
        auto* calendar = RE::Calendar::GetSingleton();
        if (calendar) {
            ImGuiMCP::Text("Game Year: %d", calendar->GetYear());
            ImGuiMCP::Text("Game Month: %d (%s)", calendar->GetMonth(), MonthToString(calendar->GetMonth()));
            ImGuiMCP::Text("Game Day: %.1f", calendar->GetDay());
            ImGuiMCP::Text("Game Hour: %.2f", calendar->GetHour());
            ImGuiMCP::Text("Days Passed: %.2f", calendar->GetDaysPassed());
        }

        ImGuiMCP::Separator();

        // Sky info
        auto* sky = RE::Sky::GetSingleton();
        if (sky) {
            if (sky->currentWeather) {
                auto name = RegionScanner::GetWeatherName(sky->currentWeather);
                auto wclass = RegionScanner::ClassifyWeather(sky->currentWeather);
                ImGuiMCP::Text("Sky Current Weather: %s (%s)", name.c_str(), WeatherClassToString(wclass));
            }
            if (sky->lastWeather) {
                auto name = RegionScanner::GetWeatherName(sky->lastWeather);
                ImGuiMCP::Text("Sky Last Weather: %s", name.c_str());
            }
            if (sky->overrideWeather) {
                auto name = RegionScanner::GetWeatherName(sky->overrideWeather);
                ImGuiMCP::Text("Sky Override Weather: %s", name.c_str());
            }
            if (sky->defaultWeather) {
                auto name = RegionScanner::GetWeatherName(sky->defaultWeather);
                auto wclass = RegionScanner::ClassifyWeather(sky->defaultWeather);
                ImGuiMCP::Text("Next Queued Weather: %s (%s)", name.c_str(), WeatherClassToString(wclass));
            } else {
                ImGuiMCP::Text("Next Queued Weather: None");
            }
            if (sky->region) {
                auto name = RegionScanner::GetRegionName(sky->region);
                ImGuiMCP::Text("Sky Region: %s", name.c_str());
            }
            ImGuiMCP::Text("Weather Blend: %.2f%%", sky->currentWeatherPct * 100.0f);
        }

        ImGuiMCP::Separator();

        // Player info
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            auto* cell = player->GetParentCell();
            if (cell) {
                ImGuiMCP::Text("Cell: %s (%s)",
                    cell->GetFormEditorID() ? cell->GetFormEditorID() : "Unknown",
                    cell->IsInteriorCell() ? "Interior" : "Exterior");
            }
        }

        ImGuiMCP::Separator();

        // Weather manager status
        ImGuiMCP::Text("Status: %s", WeatherManager::GetSingleton().GetStatusString().c_str());
    }
}
