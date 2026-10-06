#include "Core/Config.hpp"
#include "Core/Log.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <utility>

namespace Core
{
    // Canonical names first: GetKeyName returns the first match, ParseKeyName accepts all of them
    static const std::vector<std::pair<const char*, int>> s_KeyNames = {
        {"INSERT", VK_INSERT},   {"INS", VK_INSERT},
        {"DELETE", VK_DELETE},   {"DEL", VK_DELETE},
        {"HOME", VK_HOME},       {"END", VK_END},
        {"PAGEUP", VK_PRIOR},    {"PGUP", VK_PRIOR},
        {"PAGEDOWN", VK_NEXT},   {"PGDN", VK_NEXT},
        {"UP", VK_UP}, {"DOWN", VK_DOWN}, {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT},
        {"F1", VK_F1},   {"F2", VK_F2},   {"F3", VK_F3},   {"F4", VK_F4},
        {"F5", VK_F5},   {"F6", VK_F6},   {"F7", VK_F7},   {"F8", VK_F8},
        {"F9", VK_F9},   {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
        {"TAB", VK_TAB}, {"SPACE", VK_SPACE}, {"ENTER", VK_RETURN}, {"RETURN", VK_RETURN},
        {"ESCAPE", VK_ESCAPE}, {"ESC", VK_ESCAPE}, {"BACKSPACE", VK_BACK},
        {"SHIFT", VK_SHIFT},   {"LSHIFT", VK_LSHIFT}, {"RSHIFT", VK_RSHIFT},
        {"CTRL", VK_CONTROL},  {"CONTROL", VK_CONTROL}, {"LCTRL", VK_LCONTROL},
        {"ALT", VK_MENU},      {"CAPSLOCK", VK_CAPITAL},
        {"NUM0", VK_NUMPAD0},  {"NUM1", VK_NUMPAD1}, {"NUM2", VK_NUMPAD2},
        {"NUM3", VK_NUMPAD3},  {"NUM4", VK_NUMPAD4}, {"NUM5", VK_NUMPAD5},
        {"NUM6", VK_NUMPAD6},  {"NUM7", VK_NUMPAD7}, {"NUM8", VK_NUMPAD8},
        {"NUM9", VK_NUMPAD9},  {"GRAVE", VK_OEM_3}, {"TILDE", VK_OEM_3}
    };

    const std::vector<KeybindEntry>& GetKeybindTable()
    {
        static const std::vector<KeybindEntry> table = {
            {"ToggleMenu",     "ShowMenu", "Toggle Menu",           "Key to open/close trainer menu", &Keybinds::toggleMenu},
            {"GodMode",        "God",      "God Mode",              "Immortality toggle",             &Keybinds::godMode},
            {"Noclip",         nullptr,    "Noclip",                "Noclip fly toggle",              &Keybinds::noclip},
            {"NoStumble",      nullptr,    "No Stumble",            "No stumble on hard landing",     &Keybinds::noStumble},
            {"SetTeleport",    "SetTP",    "Set Teleport",          "Save current position",          &Keybinds::setTeleport},
            {"GotoTeleport",   "GotoTP",   "Goto Teleport",         "Teleport to saved position",     &Keybinds::gotoTeleport},
            {"Bhop",           nullptr,    "Bunnyhop",              "Bunnyhop boost",                 &Keybinds::bhop},
            {"DecNoclipSpeed", nullptr,    "Decrease Noclip Speed", "Decrease noclip speed (hold)",   &Keybinds::decNoclipSpeed},
            {"IncNoclipSpeed", nullptr,    "Increase Noclip Speed", "Increase noclip speed (hold)",   &Keybinds::incNoclipSpeed},
            {"NoclipForward",  nullptr,    "Noclip Forward",        "Noclip: move forward",           &Keybinds::noclipForward},
            {"NoclipBack",     nullptr,    "Noclip Back",           "Noclip: move back",              &Keybinds::noclipBack},
            {"NoclipLeft",     nullptr,    "Noclip Left",           "Noclip: strafe left",            &Keybinds::noclipLeft},
            {"NoclipRight",    nullptr,    "Noclip Right",          "Noclip: strafe right",           &Keybinds::noclipRight},
            {"NoclipUp",       nullptr,    "Noclip Up",             "Noclip: ascend",                 &Keybinds::noclipUp},
            {"NoclipDown",     nullptr,    "Noclip Down",           "Noclip: descend",                &Keybinds::noclipDown},
            {"NoclipFast",     nullptr,    "Noclip Fast (hold)",    "Noclip: 1.5x speed while held",  &Keybinds::noclipFast},
            {"NoclipSlow",     nullptr,    "Noclip Slow (hold)",    "Noclip: 0.3x speed while held",  &Keybinds::noclipSlow},
        };
        return table;
    }

    static std::string Trim(const std::string& str)
    {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    static std::string ToUpper(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
        return s;
    }

    static bool ParseBool(const std::string& val)
    {
        return val == "1" || ToUpper(val) == "TRUE";
    }

    static bool ParseFloat(const std::string& val, float minValue, float maxValue, float& out)
    {
        try
        {
            float parsed = std::stof(val);
            if (!std::isfinite(parsed)) return false;
            out = (std::clamp)(parsed, minValue, maxValue);
            return true;
        }
        catch (...) {}
        return false;
    }

    std::string WideToUtf8(const std::wstring& wide)
    {
        if (wide.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), len, nullptr, nullptr);
        return out;
    }

    std::string ConfigManager::GetKeyName(int vkCode)
    {
        for (const auto& [name, vk] : s_KeyNames)
        {
            if (vk == vkCode)
                return name;
        }

        // Standard alphanumeric keys
        if ((vkCode >= 'A' && vkCode <= 'Z') || (vkCode >= '0' && vkCode <= '9'))
        {
            return std::string(1, static_cast<char>(vkCode));
        }

        // Everything else is stored as hex so it always loads back (e.g. ';' would otherwise be read as a comment)
        char hexBuf[16];
        snprintf(hexBuf, sizeof(hexBuf), "0x%02X", vkCode);
        return std::string(hexBuf);
    }

    std::string ConfigManager::GetKeyDisplayName(int vkCode)
    {
        for (const auto& [name, vk] : s_KeyNames)
        {
            if (vk == vkCode)
                return name;
        }

        if ((vkCode >= 'A' && vkCode <= 'Z') || (vkCode >= '0' && vkCode <= '9'))
        {
            return std::string(1, static_cast<char>(vkCode));
        }

        // Win32 fallback, with the extended-key bit so e.g. arrow keys are not reported as numpad keys
        UINT scanCode = MapVirtualKeyW(static_cast<UINT>(vkCode), MAPVK_VK_TO_VSC);
        LONG lParam = static_cast<LONG>(scanCode << 16);
        switch (vkCode)
        {
        case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
        case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
        case VK_DIVIDE: case VK_NUMLOCK: case VK_RCONTROL: case VK_RMENU:
        case VK_LWIN: case VK_RWIN: case VK_APPS: case VK_SNAPSHOT:
            lParam |= (1 << 24);
            break;
        default:
            break;
        }

        wchar_t keyName[64] = {0};
        if (scanCode != 0 && GetKeyNameTextW(lParam, keyName, 64) > 0)
        {
            return WideToUtf8(keyName);
        }

        return GetKeyName(vkCode);
    }

    int ConfigManager::ParseKeyName(const std::string& name)
    {
        std::string upper = ToUpper(Trim(name));
        if (upper.empty()) return 0;

        for (const auto& [keyName, vk] : s_KeyNames)
        {
            if (upper == keyName)
                return vk;
        }

        if (upper.size() == 1)
        {
            char c = upper[0];
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                return static_cast<int>(c);
            return 0;
        }

        int value = 0;
        try
        {
            size_t consumed = 0;
            if (upper.rfind("0X", 0) == 0)
                value = std::stoi(upper, &consumed, 16);
            else
                value = std::stoi(upper, &consumed, 10);
            if (consumed != upper.size()) return 0;
        }
        catch (...)
        {
            return 0;
        }

        // Valid virtual-key codes are 1..254
        return (value >= 1 && value <= 254) ? value : 0;
    }

    ConfigManager::ConfigManager()
    {
        HMODULE hDll = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ConfigManager::Get), &hDll);

        std::wstring dllPath(MAX_PATH, L'\0');
        DWORD len = 0;
        while (hDll)
        {
            len = GetModuleFileNameW(hDll, dllPath.data(), static_cast<DWORD>(dllPath.size()));
            if (len == 0 || len < dllPath.size()) break;
            dllPath.resize(dllPath.size() * 2);
        }
        dllPath.resize(len);

        if (!dllPath.empty())
        {
            m_Directory = std::filesystem::path(dllPath).parent_path();
            m_ConfigPath = m_Directory / L"trainer_config.ini";
        }
        else
        {
            m_ConfigPath = L"trainer_config.ini";
        }
    }

    std::string ConfigManager::GetConfigPathUtf8() const
    {
        return WideToUtf8(m_ConfigPath.wstring());
    }

    void ConfigManager::Load()
    {
        {
            std::lock_guard<std::mutex> lock(m_FileMutex);
            std::ifstream file(m_ConfigPath);
            if (file.is_open())
            {
                std::string line;
                std::string section;
                const auto& keyTable = GetKeybindTable();

                while (std::getline(file, line))
                {
                    line = Trim(line);
                    if (line.empty() || line[0] == ';' || line[0] == '#')
                        continue;

                    if (line.front() == '[' && line.back() == ']')
                    {
                        section = ToUpper(Trim(line.substr(1, line.size() - 2)));
                        continue;
                    }

                    size_t eqPos = line.find('=');
                    if (eqPos == std::string::npos) continue;

                    std::string key = ToUpper(Trim(line.substr(0, eqPos)));
                    std::string val = Trim(line.substr(eqPos + 1));

                    // Strip trailing comments
                    size_t commentPos = val.find_first_of(";#");
                    if (commentPos != std::string::npos)
                        val = Trim(val.substr(0, commentPos));

                    if (section == "KEYBINDS")
                    {
                        int vk = ParseKeyName(val);
                        if (vk == 0)
                        {
                            Log("Config: ignoring invalid key '%s' for %s", val.c_str(), key.c_str());
                            continue;
                        }

                        for (const auto& entry : keyTable)
                        {
                            if (key == ToUpper(entry.iniName) || (entry.iniAlias && key == ToUpper(entry.iniAlias)))
                            {
                                (Keys.*(entry.member)).store(vk);
                                break;
                            }
                        }
                    }
                    else if (section == "SETTINGS")
                    {
                        float f = 0.0f;
                        if (key == "NOCLIPSPEED")
                        {
                            if (ParseFloat(val, kMinNoclipSpeed, kMaxNoclipSpeed, f)) State.noclipSpeed = f;
                        }
                        else if (key == "UISCALE")
                        {
                            if (ParseFloat(val, 0.0f, kMaxUiScale, f)) State.uiScale = (f < kMinUiScale) ? 0.0f : f;
                        }
                        else if (key == "OVERLAYCORNER")
                        {
                            if (ParseFloat(val, 0.0f, 3.0f, f)) State.overlayCorner = static_cast<int>(f);
                        }
                        else if (key == "SHOWPLAYERINFO")      State.showPlayerInfo = ParseBool(val);
                        else if (key == "TRAINEROVERLAY")      State.trainerOverlay = ParseBool(val);
                        else if (key == "NOSTUMBLE")           State.noStumble = ParseBool(val);
                        else if (key == "SAMEWALL")            State.sameWall = ParseBool(val);
                        else if (key == "INFINITEWALLCLIMBS")  State.infiniteWallclimbs = ParseBool(val);
                        else if (key == "INFINITEWALLRUNS")    State.infiniteWallruns = ParseBool(val);
                    }
                }
                return;
            }
        }

        // Create default file if it doesn't exist yet
        Save();
    }

    void ConfigManager::Save()
    {
        std::lock_guard<std::mutex> lock(m_FileMutex);

        std::ostringstream file;
        file << "; =====================================================\n";
        file << "; Mirror's Edge Catalyst Trainer - Configuration File\n";
        file << "; You can edit any keybind or setting here with Notepad.\n";
        file << "; Supported Keys: INSERT, DELETE, HOME, END, PAGEUP, PAGEDOWN,\n";
        file << "; UP, DOWN, LEFT, RIGHT, F1-F12, TAB, SPACE, SHIFT, CTRL, ALT,\n";
        file << "; A-Z, 0-9, NUM0-NUM9, or any virtual-key code in hex (e.g. 0xBA)\n";
        file << "; =====================================================\n\n";

        file << "[Keybinds]\n";
        for (const auto& entry : GetKeybindTable())
        {
            std::string lineStart = std::string(entry.iniName) + "=" + GetKeyName((Keys.*(entry.member)).load());
            file << lineStart;
            if (lineStart.size() < 30) file << std::string(30 - lineStart.size(), ' ');
            file << " ; " << entry.comment << "\n";
        }

        file << "\n[Settings]\n";
        file << "NoclipSpeed="        << State.noclipSpeed.load() << "\n";
        file << "UiScale="            << State.uiScale.load() << "          ; 0 = automatic (follows Windows display scaling)\n";
        file << "OverlayCorner="      << State.overlayCorner.load() << "    ; 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right\n";
        file << "ShowPlayerInfo="     << (State.showPlayerInfo ? "1" : "0") << "\n";
        file << "TrainerOverlay="     << (State.trainerOverlay ? "1" : "0") << "\n";
        file << "NoStumble="          << (State.noStumble ? "1" : "0") << "\n";
        file << "SameWall="           << (State.sameWall ? "1" : "0") << "\n";
        file << "InfiniteWallclimbs=" << (State.infiniteWallclimbs ? "1" : "0") << "\n";
        file << "InfiniteWallruns="   << (State.infiniteWallruns ? "1" : "0") << "\n";

        // Write to a temporary file first so a crash mid-write can't leave a truncated config behind
        std::filesystem::path tempPath = m_ConfigPath;
        tempPath += L".tmp";
        {
            std::ofstream out(tempPath, std::ios::trunc);
            if (!out.is_open())
            {
                Log("Config: could not write %s", GetConfigPathUtf8().c_str());
                return;
            }
            out << file.str();
            if (!out.good())
            {
                Log("Config: write failed for %s", GetConfigPathUtf8().c_str());
                return;
            }
        }

        if (!MoveFileExW(tempPath.c_str(), m_ConfigPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            Log("Config: could not replace %s (error %lu)", GetConfigPathUtf8().c_str(), GetLastError());
        }
    }
}
