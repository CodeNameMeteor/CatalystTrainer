#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <conio.h>
#include <fcntl.h>
#include <io.h>

// How long to wait for LoadLibraryW in the game before giving up on knowing the result
constexpr DWORD kInjectionTimeoutMs = 30000;
// After this long without a window title match, fall back to the game's largest visible window
constexpr DWORD kTitleMatchFallbackMs = 60000;

// Directory containing this executable (not the current working directory, which the user does not control)
std::filesystem::path GetExecutableDirectory()
{
    std::wstring path(MAX_PATH, L'\0');
    while (true)
    {
        DWORD len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (len == 0) return {};
        if (len < path.size())
        {
            path.resize(len);
            break;
        }
        path.resize(path.size() * 2);
    }
    return std::filesystem::path(path).parent_path();
}

// Find all process IDs with the given executable name
std::vector<DWORD> GetProcessIdsByName(const std::wstring& processName)
{
    std::vector<DWORD> pids;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(processName.c_str(), entry.szExeFile) == 0)
                    pids.push_back(entry.th32ProcessID);
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return pids;
}

bool IsProcessAlive(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return false;

    DWORD exitCode = 0;
    bool alive = GetExitCodeProcess(hProc, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(hProc);
    return alive;
}

// Find main window belonging to PID
struct WindowSearchData
{
    DWORD pid;
    HWND hWnd;          // Window whose title matches the game
    HWND fallback;      // Largest plausible window, used if the title never matches (e.g. localised title)
    int fallbackArea;
};

BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam)
{
    WindowSearchData* data = reinterpret_cast<WindowSearchData*>(lParam);
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hWnd, &windowPid);

    if (windowPid == data->pid && IsWindowVisible(hWnd))
    {
        wchar_t windowTitle[256] = {0};
        GetWindowTextW(hWnd, windowTitle, 256);
        std::wstring wsTitle(windowTitle);

        // Ignore EA activation or dummy windows
        if (wsTitle.empty() ||
            wsTitle.find(L"Activation") != std::wstring::npos ||
            wsTitle.find(L"EA") != std::wstring::npos)
        {
            return TRUE; // Continue looking
        }

        // Check window size to ignore dummy/splash windows
        RECT rect{};
        GetClientRect(hWnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        if (width < 640 || height < 480)
        {
            return TRUE; // Continue looking, this is likely a splash screen or dummy window
        }

        // Specifically look for the game window by title
        if (wsTitle.find(L"Mirror's Edge") != std::wstring::npos ||
            wsTitle.find(L"Catalyst") != std::wstring::npos)
        {
            data->hWnd = hWnd;
            return FALSE; // Found it! Stop enumerating
        }

        if (width * height > data->fallbackArea)
        {
            data->fallback = hWnd;
            data->fallbackArea = width * height;
        }
    }
    return TRUE;
}

WindowSearchData FindGameWindow(DWORD pid)
{
    WindowSearchData data = { pid, nullptr, nullptr, 0 };
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
    return data;
}

// Returns the base address of the module with the given path in the target process, or 0
uintptr_t FindRemoteModule(DWORD pid, const std::filesystem::path& modulePath)
{
    uintptr_t base = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry))
    {
        do
        {
            std::error_code ec;
            if (std::filesystem::equivalent(entry.szExePath, modulePath, ec))
            {
                base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return base;
}

bool InjectDLL(DWORD pid, const std::filesystem::path& dllPath)
{
    std::wcout << L"[*] Opening process (PID: " << pid << L")...\n";

    // Only the rights needed to allocate, write and start a thread in the game
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_LIMITED_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess)
    {
        DWORD err = GetLastError();
        std::wcout << L"[-] Failed to open game process. Error code: " << err << L"\n";
        if (err == ERROR_ACCESS_DENIED)
            std::wcout << L"    -> Access denied: the game is probably running as administrator. Start the game normally (not as administrator) and try again.\n";
        return false;
    }

    const std::wstring path = dllPath.wstring();
    size_t pathSize = (path.size() + 1) * sizeof(wchar_t);
    LPVOID pRemoteBuf = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteBuf)
    {
        std::wcout << L"[-] VirtualAllocEx failed in target process. Error: " << GetLastError() << L"\n";
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pRemoteBuf, path.c_str(), pathSize, nullptr))
    {
        std::wcout << L"[-] WriteProcessMemory failed. Error: " << GetLastError() << L"\n";
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPTHREAD_START_ROUTINE pLoadLibraryW = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")));

    if (!pLoadLibraryW)
    {
        std::wcout << L"[-] Failed to find LoadLibraryW.\n";
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    std::wcout << L"[*] Spawning injection thread in game process...\n";
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, pLoadLibraryW, pRemoteBuf, 0, nullptr);
    if (!hThread)
    {
        std::wcout << L"[-] CreateRemoteThread failed. Error: " << GetLastError() << L"\n";
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    std::wcout << L"[*] Waiting for DLL initialization...\n";
    DWORD waitResult = WaitForSingleObject(hThread, kInjectionTimeoutMs);
    CloseHandle(hThread);

    if (waitResult != WAIT_OBJECT_0)
    {
        // LoadLibraryW may still be reading the path, so the buffer is deliberately left allocated
        std::wcout << L"[-] The game did not finish loading the trainer within " << (kInjectionTimeoutMs / 1000)
                   << L" seconds. It may still load; check the game in a moment.\n";
        CloseHandle(hProcess);
        return false;
    }

    VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    // The thread exit code only holds the low 32 bits of the module handle, so look the module up instead
    uintptr_t moduleBase = FindRemoteModule(pid, dllPath);
    if (!moduleBase)
    {
        std::wcout << L"[-] LoadLibraryW in target process failed (module not loaded)!\n";
        return false;
    }

    std::wcout << L"[+] Trainer module loaded at: 0x" << std::hex << moduleBase << std::dec << L"\n";
    return true;
}

int Finish(bool success)
{
    if (success)
    {
        std::wcout << L"\n====================================================\n";
        std::wcout << L"[+] INJECTION SUCCESSFUL!\n";
        std::wcout << L"====================================================\n";
        std::wcout << L"\nClosing in 5 seconds...\n";
        Sleep(5000);
        return 0;
    }

    // Leave error messages on screen until the user has read them
    std::wcout << L"\n[-] INJECTION FAILED. Check error messages above.\n";
    std::wcout << L"Press any key to exit...\n";
    _getch();
    return 1;
}

int main()
{
    // Without this, printing a non-ASCII path (e.g. a user name with accents) breaks all further output
    _setmode(_fileno(stdout), _O_U16TEXT);

    SetConsoleTitleW(L"Mirror's Edge Catalyst Trainer Injector");
    std::wcout << L"====================================================\n";
    std::wcout << L"   Mirror's Edge Catalyst - Trainer Injector          \n";
    std::wcout << L"====================================================\n\n";

    const std::wstring targetProcess = L"MirrorsEdgeCatalyst.exe";
    const std::wstring dllName = L"MEC_Trainer.dll";

    // Only ever load the trainer that sits next to the injector
    const std::filesystem::path exeDir = GetExecutableDirectory();
    const std::filesystem::path dllPath = exeDir / dllName;

    std::error_code ec;
    if (exeDir.empty() || !std::filesystem::is_regular_file(dllPath, ec))
    {
        std::wcout << L"[-] Error: Trainer DLL was not found next to the injector!\n";
        std::wcout << L"    Expected: " << dllPath.wstring() << L"\n";
        return Finish(false);
    }

    std::wcout << L"[+] Found DLL: " << dllPath.wstring() << L"\n";
    DWORD pid = 0;
    HWND hGameWindow = nullptr;

    while (true)
    {
        std::wcout << L"[*] Looking for " << targetProcess << L"...\n";
        std::vector<DWORD> pids;
        while ((pids = GetProcessIdsByName(targetProcess)).empty())
        {
            Sleep(500);
        }
        if (pids.size() > 1)
            std::wcout << L"[!] " << pids.size() << L" game processes found, using the one with a game window.\n";
        std::wcout << L"[+] Target process found!\n";

        // Wait for the window to be created
        std::wcout << L"[*] Waiting for game window to be ready...\n";
        bool processesDied = false;
        DWORD waitStart = GetTickCount();
        DWORD lastNotice = waitStart;
        pid = 0;
        hGameWindow = nullptr;

        while (!hGameWindow)
        {
            bool anyAlive = false;
            for (DWORD candidate : pids)
            {
                if (!IsProcessAlive(candidate)) continue;
                anyAlive = true;

                WindowSearchData found = FindGameWindow(candidate);
                HWND window = found.hWnd;
                if (!window && found.fallback && GetTickCount() - waitStart > kTitleMatchFallbackMs)
                {
                    std::wcout << L"[!] No window titled \"Mirror's Edge\" found; using the game's main window instead.\n";
                    window = found.fallback;
                }

                if (window)
                {
                    pid = candidate;
                    hGameWindow = window;
                    break;
                }
            }

            if (!anyAlive)
            {
                processesDied = true;
                std::wcout << L"[-] Process closed before window was ready. Restarting search...\n\n";
                break;
            }

            if (!hGameWindow)
            {
                if (GetTickCount() - lastNotice > 15000)
                {
                    std::wcout << L"[*] Still waiting for the game window (is the game past the EA app splash screen?)...\n";
                    lastNotice = GetTickCount();
                }
                Sleep(500);
            }
        }

        if (processesDied)
        {
            continue; // Loop back and search for a new PID
        }

        std::wcout << L"[+] Game window detected (PID: " << pid << L"). Verifying responsiveness...\n";
        DWORD_PTR result;
        int checkCount = 0;
        bool processDied = false;
        bool responsive = false;
        while (checkCount < 10)
        {
            if (SendMessageTimeoutW(hGameWindow, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 1000, &result))
            {
                responsive = true;
                break;
            }

            // Verify process didn't die while we were checking window responsiveness
            if (!IsProcessAlive(pid))
            {
                processDied = true;
                std::wcout << L"[-] Process died during responsiveness check. Restarting search...\n\n";
                break;
            }

            std::wcout << L"[*] Game is busy initializing... waiting...\n";
            Sleep(1000);
            checkCount++;
        }

        if (processDied)
        {
            continue; // Loop back and search for a new PID
        }

        if (!responsive)
        {
            std::wcout << L"[!] The game window is still not responding; injecting anyway.\n";
        }

        // If we got here, the process is alive and the window is found.
        break;
    }

    std::wcout << L"[+] Injecting " << dllName << L" into process...\n";
    return Finish(InjectDLL(pid, dllPath));
}
