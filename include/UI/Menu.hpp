#pragma once
#include <d3d11.h>

namespace UI
{
    extern int* g_ActiveBindingKey;

    class Menu
    {
    public:
        static void Render();
        static void RenderOverlay();
        static void DrawHotkey(const char* label, int* key);

    private:
        static void DrawTrainerTab();
        static void DrawMovementTab();
        static void DrawWorldTab();
        static void DrawSettingsTab();
    };
}
