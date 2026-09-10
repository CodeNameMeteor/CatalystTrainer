#include <Windows.h>
#include "Core/Hooks.hpp"
#include "Core/Config.hpp"
#include "Core/Memory.hpp"
#include "Game/GameState.hpp"
#include <iostream>

DWORD WINAPI MainThread(LPVOID lpParam)
{
    // Loads User settings
    Core::ConfigManager::Get().Load();

    // Wait until the main game module and graphics libraries are loaded in the process
    while (!GetModuleHandleA("MirrorsEdgeCatalyst.exe") || 
           !GetModuleHandleA("d3d11.dll") || 
           !GetModuleHandleA("dxgi.dll"))
    {
        Sleep(200);
    }

    // Keep retrying hook until DirectX 11 device is created
    bool hooked = false;
    for (int attempts = 0; attempts < 100; ++attempts)
    {
        if (Core::InitializeHooks())
        {
            hooked = true;
            break;
        }
        Sleep(250);
    }

    return hooked ? 0 : 1;
}

namespace Core
{
    HMODULE g_hModule = nullptr;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        Core::g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        Core::ShutdownHooks();
    }
    return TRUE;
}
