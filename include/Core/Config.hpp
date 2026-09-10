#pragma once
#include <Windows.h>
#include <string>

namespace Core
{
    struct Keybinds
    {
        int toggleMenu     = VK_INSERT; // 0x2D
        int godMode        = 0x31;      // '1'
        int noclip         = 0x32;      // '2'
        int noStumble      = 0x33;      // '3'
        int setTeleport    = 0x34;      // '4'
        int gotoTeleport   = 0x35;      // '5'
        int bhop           = 0x46;      // 'F'
        int decNoclipSpeed = 0x45;      // 'E'
        int incNoclipSpeed = 0x52;      // 'R'
    };

    struct Settings
    {
        bool isMenuOpen = false;
        bool trainerEnabled = false;
        bool showPlayerInfo = false;
        bool trainerOverlay = true;

        // Active Cheats
        bool godMode = false;
        bool noclip = false;
        float noclipSpeed = 0.5f;
        bool noStumble = false;
        bool bhop = false;
        bool sameWall = false;
        bool infiniteWallclimbs = false;
        bool infiniteWallruns = false;
        bool fastLoads = false;

        // Visuals / World
        bool freezeTime = false;
        float frozenTimeValue = 0.0f;
        float timeScale = 1.0f;
        bool bloom = false;
        bool blur = false;
        bool vignette = false;
        bool tpBeatLE = false;
        float maxFps = 200.0f;
        float simRate = 30.0f;
        bool screenInfo = false;
    };

    class ConfigManager
    {
    public:
        static ConfigManager& Get()
        {
            static ConfigManager instance;
            return instance;
        }

        void Load(const std::string& filename = "");
        void Save(const std::string& filename = "");

        std::string GetConfigPath() const { return m_ConfigPath; }

        static std::string GetKeyName(int vkCode);
        static int ParseKeyName(const std::string& name);

        Keybinds Keys;
        Settings State;

    private:
        ConfigManager();
        std::string m_ConfigPath;
    };
}
