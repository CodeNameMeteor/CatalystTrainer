#pragma once
#include <Windows.h>

namespace Core
{
    extern HMODULE g_hModule;
    bool InitializeHooks();
    void ShutdownHooks();
    void UnloadTrainer();
}
