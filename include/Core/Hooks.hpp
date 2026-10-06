#pragma once
#include <Windows.h>

namespace Core
{
    extern HMODULE g_hModule;
    bool InitializeHooks();
    void ShutdownHooks();
    void UnloadTrainer();
    bool IsUnloadRequested();

    // Tears down hooks, ImGui and D3D objects. Must be called from the trainer's own thread after an unload
    // request. Returns true when it is safe to free the module.
    bool TeardownHooks();

    HWND GetGameWindow();
}
