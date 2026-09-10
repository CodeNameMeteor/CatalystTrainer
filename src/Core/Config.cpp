#include "Core/Config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace Core
{
    static std::unordered_map<std::string, int> s_NameToKeyMap = {
        {"INSERT", VK_INSERT}, {"INS", VK_INSERT},
        {"DELETE", VK_DELETE}, {"DEL", VK_DELETE},
        {"HOME", VK_HOME},     {"END", VK_END},
        {"PAGEUP", VK_PRIOR},  {"PGUP", VK_PRIOR},
        {"PAGEDOWN", VK_NEXT}, {"PGDN", VK_NEXT},
        {"F1", VK_F1},   {"F2", VK_F2},   {"F3", VK_F3},   {"F4", VK_F4},
        {"F5", VK_F5},   {"F6", VK_F6},   {"F7", VK_F7},   {"F8", VK_F8},
        {"F9", VK_F9},   {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
        {"TAB", VK_TAB}, {"SPACE", VK_SPACE}, {"RETURN", VK_RETURN}, {"ENTER", VK_RETURN},
        {"ESCAPE", VK_ESCAPE}, {"ESC", VK_ESCAPE}, {"BACKSPACE", VK_BACK},
        {"SHIFT", VK_SHIFT},   {"LSHIFT", VK_LSHIFT}, {"RSHIFT", VK_RSHIFT},
        {"CONTROL", VK_CONTROL}, {"CTRL", VK_CONTROL}, {"LCTRL", VK_LCONTROL},
        {"ALT", VK_MENU},      {"CAPSLOCK", VK_CAPITAL},
        {"NUM0", VK_NUMPAD0},  {"NUM1", VK_NUMPAD1}, {"NUM2", VK_NUMPAD2},
        {"NUM3", VK_NUMPAD3},  {"NUM4", VK_NUMPAD4}, {"NUM5", VK_NUMPAD5},
        {"NUM6", VK_NUMPAD6},  {"NUM7", VK_NUMPAD7}, {"NUM8", VK_NUMPAD8},
        {"NUM9", VK_NUMPAD9},  {"GRAVE", VK_OEM_3}, {"TILDE", VK_OEM_3}
    };

    static std::string Trim(const std::string& str)
    {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    static std::string ToUpper(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
        return s;
    }

    std::string ConfigManager::GetKeyName(int vkCode)
    {
        for (const auto& pair : s_NameToKeyMap)
        {
            if (pair.second == vkCode)
                return pair.first;
        }

        // Standard alphanumeric keys
        if ((vkCode >= 'A' && vkCode <= 'Z') || (vkCode >= '0' && vkCode <= '9'))
        {
            return std::string(1, static_cast<char>(vkCode));
        }

        // Win32 API fallback
        UINT scanCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC);
        CHAR keyName[64] = {0};
        if (GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName)) > 0)
        {
            return std::string(keyName);
        }

        char hexBuf[16];
        snprintf(hexBuf, sizeof(hexBuf), "0x%X", vkCode);
        return std::string(hexBuf);
    }

    int ConfigManager::ParseKeyName(const std::string& name)
    {
        std::string upper = ToUpper(Trim(name));
        if (upper.empty()) return 0;

        if (s_NameToKeyMap.find(upper) != s_NameToKeyMap.end())
            return s_NameToKeyMap[upper];

        if (upper.size() == 1)
        {
            char c = upper[0];
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                return static_cast<int>(c);
        }

        // Parse hex strings like "0x2D"
        if (upper.rfind("0X", 0) == 0)
        {
            try { return std::stoi(upper, nullptr, 16); } catch (...) {}
        }

        try { return std::stoi(upper); } catch (...) {}

        return 0;
    }

    ConfigManager::ConfigManager()
    {
        HMODULE hDll = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, 
            reinterpret_cast<LPCSTR>(&ConfigManager::Get), &hDll);

        char dllPath[MAX_PATH] = {0};
        if (hDll && GetModuleFileNameA(hDll, dllPath, MAX_PATH) > 0)
        {
            std::string path = dllPath;
            size_t lastSlash = path.find_last_of("\\/");
            if (lastSlash != std::string::npos)
            {
                m_ConfigPath = path.substr(0, lastSlash) + "\\trainer_config.ini";
            }
            else
            {
                m_ConfigPath = "trainer_config.ini";
            }
        }
        else
        {
            m_ConfigPath = "trainer_config.ini";
        }
    }

    void ConfigManager::Load(const std::string& filename)
    {
        std::string targetFile = filename.empty() ? m_ConfigPath : filename;
        std::ifstream file(targetFile);
        if (!file.is_open())
        {
            // Create default file if it doesn't exist yet
            Save(targetFile);
            return;
        }

        std::string line;
        std::string section;

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
                if (vk != 0)
                {
                    if (key == "TOGGLEMENU" || key == "SHOWMENU") Keys.toggleMenu = vk;
                    else if (key == "GODMODE" || key == "GOD") Keys.godMode = vk;
                    else if (key == "NOCLIP") Keys.noclip = vk;
                    else if (key == "NOSTUMBLE") Keys.noStumble = vk;
                    else if (key == "SETTELEPORT" || key == "SETTP") Keys.setTeleport = vk;
                    else if (key == "GOTOTELEPORT" || key == "GOTOTP") Keys.gotoTeleport = vk;
                    else if (key == "BHOP") Keys.bhop = vk;
                    else if (key == "DECNOCLIPSPEED") Keys.decNoclipSpeed = vk;
                    else if (key == "INCNOCLIPSPEED") Keys.incNoclipSpeed = vk;
                }
            }
            else if (section == "SETTINGS")
            {
                if (key == "NOCLIPSPEED")
                {
                    try { State.noclipSpeed = std::stof(val); } catch (...) {}
                }
                else if (key == "TRAINERENABLED")
                {
                    State.trainerEnabled = (val == "1" || ToUpper(val) == "TRUE");
                }
                else if (key == "SHOWPLAYERINFO")
                {
                    State.showPlayerInfo = (val == "1" || ToUpper(val) == "TRUE");
                }
                else if (key == "TRAINEROVERLAY")
                {
                    State.trainerOverlay = (val == "1" || ToUpper(val) == "TRUE");
                }
            }
        }
    }

    void ConfigManager::Save(const std::string& filename)
    {
        std::string targetFile = filename.empty() ? m_ConfigPath : filename;
        std::ofstream file(targetFile);
        if (!file.is_open()) return;

        file << "; =====================================================\n";
        file << "; Mirror's Edge Catalyst Trainer - Configuration File\n";
        file << "; You can edit any keybind or setting here with Notepad.\n";
        file << "; Supported Keys: INSERT, DELETE, HOME, END, F1-F12, TAB,\n";
        file << "; A-Z, 0-9, NUM0-NUM9, SPACE, etc.\n";
        file << "; =====================================================\n\n";

        file << "[Keybinds]\n";
        file << "ToggleMenu="     << GetKeyName(Keys.toggleMenu)     << "       ; Key to open/close trainer menu\n";
        file << "GodMode="        << GetKeyName(Keys.godMode)        << "              ; Immortality toggle\n";
        file << "Noclip="         << GetKeyName(Keys.noclip)         << "              ; Noclip fly toggle\n";
        file << "NoStumble="      << GetKeyName(Keys.noStumble)      << "              ; No stumble on hard landing\n";
        file << "SetTeleport="    << GetKeyName(Keys.setTeleport)    << "              ; Save current position\n";
        file << "GotoTeleport="   << GetKeyName(Keys.gotoTeleport)   << "              ; Teleport to saved position\n";
        file << "Bhop="           << GetKeyName(Keys.bhop)           << "              ; Bunnyhop boost\n";
        file << "DecNoclipSpeed=" << GetKeyName(Keys.decNoclipSpeed) << "              ; Decrease noclip speed\n";
        file << "IncNoclipSpeed=" << GetKeyName(Keys.incNoclipSpeed) << "              ; Increase noclip speed\n\n";

        file << "[Settings]\n";
        file << "NoclipSpeed="    << State.noclipSpeed << "\n";
        file << "TrainerEnabled=" << (State.trainerEnabled ? "1" : "0") << "\n";
        file << "ShowPlayerInfo=" << (State.showPlayerInfo ? "1" : "0") << "\n";
        file << "TrainerOverlay=" << (State.trainerOverlay ? "1" : "0") << "\n";
    }
}
