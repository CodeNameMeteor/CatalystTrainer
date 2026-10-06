#include "Core/Log.hpp"
#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace Core
{
    static std::wstring GetLogPath()
    {
        HMODULE hDll = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetLogPath), &hDll);

        std::wstring path(MAX_PATH, L'\0');
        DWORD len = 0;
        while (hDll)
        {
            len = GetModuleFileNameW(hDll, path.data(), static_cast<DWORD>(path.size()));
            if (len == 0 || len < path.size()) break;
            path.resize(path.size() * 2);
        }
        path.resize(len);

        size_t lastSlash = path.find_last_of(L"\\/");
        if (lastSlash == std::wstring::npos) return L"trainer_log.txt";
        return path.substr(0, lastSlash) + L"\\trainer_log.txt";
    }

    void Log(const char* fmt, ...)
    {
        static std::mutex s_Mutex;
        static FILE* s_File = nullptr;
        static bool s_Opened = false;

        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        std::lock_guard<std::mutex> lock(s_Mutex);

        std::string line = "[MEC_Trainer] ";
        line += buffer;
        line += "\n";
        OutputDebugStringA(line.c_str());

        if (!s_Opened)
        {
            s_Opened = true;
            if (_wfopen_s(&s_File, GetLogPath().c_str(), L"w") != 0)
                s_File = nullptr;
        }
        if (s_File)
        {
            fputs(line.c_str(), s_File);
            fflush(s_File);
        }
    }
}
