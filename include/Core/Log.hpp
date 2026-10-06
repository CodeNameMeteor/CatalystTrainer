#pragma once

namespace Core
{
    // Minimal logger: writes to the debugger (OutputDebugString) and to trainer_log.txt next to the DLL.
    void Log(const char* fmt, ...);
}
