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

    // The DLL is already running inside the target process, so use the process
    // image handle instead of depending on one particular executable filename.
    // This also keeps the loader compatible with renamed/launcher variants.
    while (!GetModuleHandleA(nullptr) ||
           !GetModuleHandleA("d3d11.dll") ||
           !GetModuleHandleA("dxgi.dll"))
    {
        Sleep(200);
    }

    // Keep retrying hook until DirectX 11 device is created
    bool hooked = false;
    for (int attempts = 0; attempts < 240; ++attempts)
    {
        if (Core::InitializeHooks())
        {
            hooked = true;
            break;
        }
        Sleep(250);
    }

    if (!hooked)
        OutputDebugStringA("MEC Trainer: D3D11 hooks could not be installed after 60 seconds.\n");
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
