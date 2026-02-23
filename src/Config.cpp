#include "Config.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace SWF {

    // Simple INI-style config parser (no external dependency needed)
    // Format: key = value (one per line, # for comments, [section] headers)

    namespace {
        std::string Trim(const std::string& str) {
            auto start = str.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) return "";
            auto end = str.find_last_not_of(" \t\r\n");
            return str.substr(start, end - start + 1);
        }

        float ParseFloat(const std::string& val, float def) {
            try { return std::stof(val); } catch (...) { return def; }
        }

        int ParseInt(const std::string& val, int def) {
            try { return std::stoi(val); } catch (...) { return def; }
        }

        bool ParseBool(const std::string& val, bool def) {
            auto v = Trim(val);
            if (v == "true" || v == "1" || v == "yes") return true;
            if (v == "false" || v == "0" || v == "no") return false;
            return def;
        }

        void WriteSection(std::ofstream& file, const char* section) {
            file << "\n[" << section << "]\n";
        }

        void WriteFloat(std::ofstream& file, const char* key, float val) {
            file << key << " = " << std::fixed << std::setprecision(2) << val << "\n";
        }

        void WriteInt(std::ofstream& file, const char* key, int val) {
            file << key << " = " << val << "\n";
        }

        void WriteBool(std::ofstream& file, const char* key, bool val) {
            file << key << " = " << (val ? "true" : "false") << "\n";
        }

        void WriteComment(std::ofstream& file, const char* comment) {
            file << "# " << comment << "\n";
        }

        // Split a comma-separated string into trimmed tokens.
        std::vector<std::string> SplitCSV(const std::string& str) {
            std::vector<std::string> tokens;
            std::stringstream ss(str);
            std::string token;
            while (std::getline(ss, token, ',')) {
                auto t = Trim(token);
                if (!t.empty()) tokens.push_back(std::move(t));
            }
            return tokens;
        }

        std::string JoinCSV(const std::unordered_set<std::string>& set) {
            std::string result;
            for (const auto& s : set) {
                if (!result.empty()) result += ",";
                result += s;
            }
            return result;
        }
    } 

    std::string ConfigManager::GetConfigPath() const {
        wchar_t modulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(GetModuleHandleW(L"SeasonalWeatherFramework.dll"),
                               modulePath, MAX_PATH)) {
            auto path = std::filesystem::path(modulePath)
                .parent_path()
                / "SeasonalWeatherFramework.ini";
            return path.string();
        }

        return "Data/SKSE/Plugins/SeasonalWeatherFramework.ini";
    }

    void ConfigManager::Load() {
        std::unique_lock<std::mutex> lock(mutex_);

        auto path = GetConfigPath();
        if (!std::filesystem::exists(path)) {
            lock.unlock();  
            logs::info("Config file not found at {}, using defaults and creating one", path);
            Save();
            return;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            logs::warn("Failed to open config file: {}", path);
            return;
        }

        std::string line;
        std::string currentSection;

        while (std::getline(file, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;

            // Section header
            if (line[0] == '[') {
                auto end = line.find(']');
                if (end != std::string::npos) {
                    currentSection = Trim(line.substr(1, end - 1));
                }
                continue;
            }

            // Key = Value
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            auto key = Trim(line.substr(0, eq));
            auto val = Trim(line.substr(eq + 1));

            if (currentSection == "General") {
                if (key == "bEnabled")             config_.enabled             = ParseBool(val, config_.enabled);
                if (key == "bEnableNotifications") config_.enableNotifications = ParseBool(val, config_.enableNotifications);
                if (key == "bDebugMode")           config_.debugMode           = ParseBool(val, config_.debugMode);
            }
            else if (currentSection == "SeasonMonths") {
                if (key == "iSpringStart") config_.springStart = ParseInt(val, config_.springStart);
                if (key == "iSpringEnd")   config_.springEnd   = ParseInt(val, config_.springEnd);
                if (key == "iSummerStart") config_.summerStart  = ParseInt(val, config_.summerStart);
                if (key == "iSummerEnd")   config_.summerEnd    = ParseInt(val, config_.summerEnd);
                if (key == "iFallStart")   config_.fallStart    = ParseInt(val, config_.fallStart);
                if (key == "iFallEnd")     config_.fallEnd      = ParseInt(val, config_.fallEnd);
            }
            else if (currentSection == "Worldspaces") {
                if (key == "sEnabledWorldspaces") {
                    // Comma-separated list of worldspace EditorIDs.
                    config_.enabledWorldspaces.clear();
                    for (auto& ws : SplitCSV(val))
                        config_.enabledWorldspaces.insert(ws);
                }
                if (key == "bEnableTamriel") {
                    if (ParseBool(val, true))
                        config_.enabledWorldspaces.insert("Tamriel");
                    else
                        config_.enabledWorldspaces.erase("Tamriel");
                }
                if (key == "bEnableSolstheim") {
                    if (ParseBool(val, true))
                        config_.enabledWorldspaces.insert("DLC2SolstheimWorld");
                    else
                        config_.enabledWorldspaces.erase("DLC2SolstheimWorld");
                }
            }
            else if (currentSection == "Transitions") {

            }
            else if (currentSection == "SpringMultipliers") {
                if (key == "fPleasant") config_.springMultipliers.pleasantMult = ParseFloat(val, config_.springMultipliers.pleasantMult);
                if (key == "fCloudy")   config_.springMultipliers.cloudyMult   = ParseFloat(val, config_.springMultipliers.cloudyMult);
                if (key == "fRainy")    config_.springMultipliers.rainyMult    = ParseFloat(val, config_.springMultipliers.rainyMult);
                if (key == "fSnow")     config_.springMultipliers.snowMult     = ParseFloat(val, config_.springMultipliers.snowMult);
            }
            else if (currentSection == "SummerMultipliers") {
                if (key == "fPleasant") config_.summerMultipliers.pleasantMult = ParseFloat(val, config_.summerMultipliers.pleasantMult);
                if (key == "fCloudy")   config_.summerMultipliers.cloudyMult   = ParseFloat(val, config_.summerMultipliers.cloudyMult);
                if (key == "fRainy")    config_.summerMultipliers.rainyMult    = ParseFloat(val, config_.summerMultipliers.rainyMult);
                if (key == "fSnow")     config_.summerMultipliers.snowMult     = ParseFloat(val, config_.summerMultipliers.snowMult);
            }
            else if (currentSection == "FallMultipliers") {
                if (key == "fPleasant") config_.fallMultipliers.pleasantMult = ParseFloat(val, config_.fallMultipliers.pleasantMult);
                if (key == "fCloudy")   config_.fallMultipliers.cloudyMult   = ParseFloat(val, config_.fallMultipliers.cloudyMult);
                if (key == "fRainy")    config_.fallMultipliers.rainyMult    = ParseFloat(val, config_.fallMultipliers.rainyMult);
                if (key == "fSnow")     config_.fallMultipliers.snowMult     = ParseFloat(val, config_.fallMultipliers.snowMult);
            }
            else if (currentSection == "WinterMultipliers") {
                if (key == "fPleasant") config_.winterMultipliers.pleasantMult = ParseFloat(val, config_.winterMultipliers.pleasantMult);
                if (key == "fCloudy")   config_.winterMultipliers.cloudyMult   = ParseFloat(val, config_.winterMultipliers.cloudyMult);
                if (key == "fRainy")    config_.winterMultipliers.rainyMult    = ParseFloat(val, config_.winterMultipliers.rainyMult);
                if (key == "fSnow")     config_.winterMultipliers.snowMult     = ParseFloat(val, config_.winterMultipliers.snowMult);
            }
        }

        logs::info("Config loaded successfully from {}", path);
    }

    void ConfigManager::Save() {

        Config snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = config_;
        }

        auto path = GetConfigPath();

        auto dir = std::filesystem::path(path).parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            logs::error("Failed to write config file: {}", path);
            return;
        }

        WriteComment(file, "Seasonal Weather Framework SKSE - Configuration");
        WriteComment(file, "Modifies region weather probabilities based on the current in-game season.");
        WriteComment(file, "");

        WriteSection(file, "General");
        WriteComment(file, "Master toggle for the framework");
        WriteBool(file, "bEnabled", snapshot.enabled);
        WriteComment(file, "Show HUD notification when season changes");
        WriteBool(file, "bEnableNotifications", snapshot.enableNotifications);
        WriteComment(file, "Enable debug logging");
        WriteBool(file, "bDebugMode", snapshot.debugMode);

        WriteSection(file, "SeasonMonths");
        WriteComment(file, "Month indices (0 = Morning Star ... 11 = Evening Star)");
        WriteInt(file, "iSpringStart", snapshot.springStart);
        WriteInt(file, "iSpringEnd", snapshot.springEnd);
        WriteInt(file, "iSummerStart", snapshot.summerStart);
        WriteInt(file, "iSummerEnd", snapshot.summerEnd);
        WriteInt(file, "iFallStart", snapshot.fallStart);
        WriteInt(file, "iFallEnd", snapshot.fallEnd);

        WriteSection(file, "Worldspaces");
        WriteComment(file, "Comma-separated list of worldspace EditorIDs to apply seasonal weather to.");
        WriteComment(file, "Add any modded worldspace EditorID here (e.g. Tamriel,DLC2SolstheimWorld,Falskaar).");
        file << "sEnabledWorldspaces = " << JoinCSV(snapshot.enabledWorldspaces) << "\n";

        WriteSection(file, "Transitions");
        WriteComment(file, "Note: smoothed transitions and transition speed have been removed.");
        WriteComment(file, "Skyrim's own weather system handles all transitions naturally now.");

        auto writeMultipliers = [&](const char* section, const SeasonWeatherMultipliers& m) {
            WriteSection(file, section);
            WriteComment(file, "Multipliers applied to base region weather chances for this season.");
            WriteComment(file, "Values > 1.0 increase probability, < 1.0 decrease, 0.0 removes entirely.");
            WriteFloat(file, "fPleasant", m.pleasantMult);
            WriteFloat(file, "fCloudy", m.cloudyMult);
            WriteFloat(file, "fRainy", m.rainyMult);
            WriteFloat(file, "fSnow", m.snowMult);
        };

        writeMultipliers("SpringMultipliers", snapshot.springMultipliers);
        writeMultipliers("SummerMultipliers", snapshot.summerMultipliers);
        writeMultipliers("FallMultipliers",   snapshot.fallMultipliers);
        writeMultipliers("WinterMultipliers", snapshot.winterMultipliers);

        logs::info("Config saved to {}", path);
    }
}
