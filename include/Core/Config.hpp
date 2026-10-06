#pragma once
#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace Core
{
    // Keybinds and settings are shared between the game's window thread (hotkeys), the render thread (menu)
    // and the trainer's logic thread, so every field is atomic.
    struct Keybinds
    {
        std::atomic<int> toggleMenu     { VK_F8 };
        std::atomic<int> godMode        { '1' };
        std::atomic<int> noclip         { '2' };
        std::atomic<int> noStumble      { '3' };
        std::atomic<int> setTeleport    { '4' };
        std::atomic<int> gotoTeleport   { '5' };
        std::atomic<int> bhop           { 'F' };
        std::atomic<int> decNoclipSpeed { 'E' };
        std::atomic<int> incNoclipSpeed { 'R' };

        // Noclip movement
        std::atomic<int> noclipForward  { 'W' };
        std::atomic<int> noclipBack     { 'S' };
        std::atomic<int> noclipLeft     { 'A' };
        std::atomic<int> noclipRight    { 'D' };
        std::atomic<int> noclipUp       { VK_SPACE };
        std::atomic<int> noclipDown     { 'C' };
        std::atomic<int> noclipFast     { VK_SHIFT };
        std::atomic<int> noclipSlow     { VK_MENU };
    };

    struct KeybindEntry
    {
        const char* iniName;
        const char* iniAlias;   // Older name accepted when loading, may be nullptr
        const char* label;      // Shown in the menu
        const char* comment;    // Written to the config file
        std::atomic<int> Keybinds::* member;
    };

    // All rebindable actions, in menu order
    const std::vector<KeybindEntry>& GetKeybindTable();

    enum class OverlayCorner : int
    {
        TopLeft = 0,
        TopRight = 1,
        BottomLeft = 2,
        BottomRight = 3
    };

    struct Settings
    {
        std::atomic<bool> isMenuOpen { false };
        std::atomic<bool> showPlayerInfo { false };
        std::atomic<bool> trainerOverlay { true };
        std::atomic<int>  overlayCorner { static_cast<int>(OverlayCorner::TopLeft) };
        std::atomic<float> uiScale { 0.0f };        // 0 = automatic (window DPI)

        // Active Cheats
        std::atomic<bool> godMode { false };
        std::atomic<bool> noclip { false };
        std::atomic<float> noclipSpeed { 0.5f };
        std::atomic<bool> noStumble { false };
        std::atomic<bool> bhop { false };
        std::atomic<bool> sameWall { false };
        std::atomic<bool> infiniteWallclimbs { false };
        std::atomic<bool> infiniteWallruns { false };
        std::atomic<bool> fastLoads { false };

        // Visuals / World
        std::atomic<bool> freezeTime { false };
        std::atomic<float> frozenTimeValue { 0.0f };
        std::atomic<float> timeScale { 1.0f };
        std::atomic<bool> bloom { false };
        std::atomic<bool> blur { false };
        std::atomic<bool> vignette { false };
    };

    constexpr float kMinNoclipSpeed = 0.1f;
    constexpr float kMaxNoclipSpeed = 10.0f;
    constexpr float kMinUiScale = 0.75f;
    constexpr float kMaxUiScale = 3.0f;

    class ConfigManager
    {
    public:
        static ConfigManager& Get()
        {
            static ConfigManager instance;
            return instance;
        }

        void Load();
        void Save();

        // UTF-8 path for display
        std::string GetConfigPathUtf8() const;
        const std::filesystem::path& GetConfigPath() const { return m_ConfigPath; }
        const std::filesystem::path& GetDirectory() const { return m_Directory; }

        // Serialisable key name (always parses back to the same VK code)
        static std::string GetKeyName(int vkCode);
        // Human-friendly key name for the UI
        static std::string GetKeyDisplayName(int vkCode);
        // Returns 0 when the name is not a valid key
        static int ParseKeyName(const std::string& name);

        Keybinds Keys;
        Settings State;

    private:
        ConfigManager();
        std::filesystem::path m_ConfigPath;
        std::filesystem::path m_Directory;
        std::mutex m_FileMutex;
    };

    std::string WideToUtf8(const std::wstring& wide);
}
