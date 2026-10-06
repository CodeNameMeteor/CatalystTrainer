#pragma once
#include <atomic>

namespace UI
{
    // Keybind currently waiting for a key press (set by the menu, completed by the window procedure)
    extern std::atomic<std::atomic<int>*> g_ActiveBindingKey;

    class Menu
    {
    public:
        static void Render();
        static void RenderOverlay();
        static void DrawHotkey(const char* label, std::atomic<int>* key, bool isDuplicate);

    private:
        static void DrawTrainerTab();
        static void DrawMovementTab();
        static void DrawWorldTab();
        static void DrawSettingsTab();
    };
}
