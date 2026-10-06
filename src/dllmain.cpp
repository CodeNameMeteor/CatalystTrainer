#include <Windows.h>
#include "Core/Hooks.hpp"
#include "Core/Config.hpp"
#include "Core/Log.hpp"
#include "Core/Memory.hpp"
#include "Game/GameOffsets.hpp"
#include "Game/GameState.hpp"
#include "Features/TrainerFeatures.hpp"
#include "Features/MovementProperties.hpp"

namespace Core
{
    HMODULE g_hModule = nullptr;
}

static void CheckGameBuild()
{
    const auto detected = Game::BuildInfo::Detect();
    switch (Game::BuildInfo::Check(detected))
    {
    case Game::BuildInfo::Status::Supported:
        Core::Log("Game build 0x%08X / 0x%08X is supported", detected.timeDateStamp, detected.sizeOfImage);
        break;
    case Game::BuildInfo::Status::Unverified:
        Core::Log("Game build 0x%08X / 0x%08X has not been verified against the offsets", detected.timeDateStamp, detected.sizeOfImage);
        break;
    case Game::BuildInfo::Status::Unsupported:
        // The hard-coded offsets would point at unrelated memory on this build
        Core::Log("Game build 0x%08X / 0x%08X is not supported: memory writes disabled", detected.timeDateStamp, detected.sizeOfImage);
        Core::Memory::SetWritesEnabled(false);
        break;
    }
}

DWORD WINAPI MainThread(LPVOID)
{
    Core::Log("Trainer loaded");

    // Loads User settings
    Core::ConfigManager::Get().Load();

    // Create the shared singletons here, on one thread, before the hooks can touch them
    Game::GameState::Get();
    Features::TrainerFeatures::Get();
    Features::MovementManager::Get();

    // Wait until the main game module and graphics libraries are loaded in the process
    while (!GetModuleHandleA("MirrorsEdgeCatalyst.exe") ||
           !GetModuleHandleA("d3d11.dll") ||
           !GetModuleHandleA("dxgi.dll"))
    {
        if (Core::IsUnloadRequested()) return 0;
        Sleep(200);
    }

    CheckGameBuild();

    // Keep retrying hook until DirectX 11 device is created
    bool hooked = false;
    for (int attempts = 0; attempts < 100 && !Core::IsUnloadRequested(); ++attempts)
    {
        if (Core::InitializeHooks())
        {
            hooked = true;
            break;
        }
        Sleep(250);
    }

    if (!hooked)
    {
        Core::Log("Could not hook DirectX 11, trainer inactive");
        return 1;
    }

    // Background Game Logic Thread
    LARGE_INTEGER frequency, last, now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&last);

    while (!Core::IsUnloadRequested())
    {
        QueryPerformanceCounter(&now);
        float dt = static_cast<float>(now.QuadPart - last.QuadPart) / static_cast<float>(frequency.QuadPart);
        last = now;

        Game::GameState::Get().Update();
        Features::TrainerFeatures::Get().Tick(dt);
        Sleep(16);
    }

    // Unload: restore the game's state first, then remove the hooks, then free the module from this thread
    // (so nothing of ours is still running when the DLL goes away)
    Core::Log("Unloading");
    Features::TrainerFeatures::Get().Shutdown();

    if (Core::TeardownHooks())
    {
        Core::Log("Unloaded");
        FreeLibraryAndExitThread(Core::g_hModule, 0);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        Core::g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE thread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        Core::ShutdownHooks();
    }
    return TRUE;
}
