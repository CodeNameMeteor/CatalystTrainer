#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <conio.h>

// Enable SeDebugPrivilege for administrator access
bool EnableDebugPrivilege()
{
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tkp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;

    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
    {
        CloseHandle(hToken);
        return false;
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Luid = luid;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL status = AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL);
    CloseHandle(hToken);
    return status != FALSE;
}

// Find process ID by executable name
DWORD GetProcessIdByName(const std::wstring& processName)
{
    DWORD pid = 0;
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
                {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

// Find main window belonging to PID
struct WindowSearchData
{
    DWORD pid;
    HWND hWnd;
};

BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam)
{
    WindowSearchData* data = reinterpret_cast<WindowSearchData*>(lParam);
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hWnd, &windowPid);

    if (windowPid == data->pid && IsWindowVisible(hWnd))
    {
        wchar_t windowTitle[256];
        GetWindowTextW(hWnd, windowTitle, 256);
        std::wstring wsTitle(windowTitle);

        wchar_t className[256];
        GetClassNameW(hWnd, className, 256);
        std::wstring wsClass(className);

        // Ignore EA Activation or dummy windows
        if (wsTitle.find(L"Activation") != std::wstring::npos ||
            wsTitle.find(L"EA") != std::wstring::npos ||
            wsTitle.empty())
        {
            return TRUE; // Continue looking
        }

        // Check window size to ignore dummy/splash windows
        RECT rect;
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
    }
    return TRUE;
}

HWND GetGameWindow(DWORD pid)
{
    WindowSearchData data = { pid, nullptr };
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
    return data.hWnd;
}

bool InjectDLL(DWORD pid, const std::wstring& dllPath)
{
    std::wcout << L"[*] Opening process (PID: " << pid << L")...\n";
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess)
    {
        DWORD err = GetLastError();
        std::wcout << L"[-] Failed to open game process. Error code: " << err << L"\n";
        if (err == 5)
            std::wcout << L"    -> Access Denied: Please right-click and 'Run as administrator'!\n";
        return false;
    }

    size_t pathSize = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID pRemoteBuf = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteBuf)
    {
        std::wcout << L"[-] VirtualAllocEx failed in target process. Error: " << GetLastError() << L"\n";
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pRemoteBuf, dllPath.c_str(), pathSize, nullptr))
    {
        std::wcout << L"[-] WriteProcessMemory failed. Error: " << GetLastError() << L"\n";
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPTHREAD_START_ROUTINE pLoadLibraryW = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW"));

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
    WaitForSingleObject(hThread, 8000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    if (exitCode == 0)
    {
        std::wcout << L"[-] LoadLibraryW in target process returned NULL (Module failed to load)!\n";
        return false;
    }

    std::wcout << L"[+] Remote module handle loaded at: 0x" << std::hex << exitCode << std::dec << L"\n";
    return true;
}

int main()
{
    SetConsoleTitleW(L"Mirror's Edge Catalyst Trainer Injector");
    std::wcout << L"====================================================\n";
    std::wcout << L"   Mirror's Edge Catalyst - Trainer Injector          \n";
    std::wcout << L"====================================================\n\n";

    EnableDebugPrivilege();

    std::wstring targetProcess = L"MirrorsEdgeCatalyst.exe";
    std::wstring dllName = L"MEC_Trainer.dll";

    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, currentDir);
    std::wstring fullDllPath = std::wstring(currentDir) + L"\\" + dllName;

    // Check if dll exists in current directory or in Release folder
    if (!std::filesystem::exists(fullDllPath))
    {
        std::wstring altPath = std::wstring(currentDir) + L"\\Release\\" + dllName;
        if (std::filesystem::exists(altPath))
        {
            fullDllPath = altPath;
        }
        else
        {
            std::wcout << L"[-] Error: Trainer DLL was not found in the injector directory!\n";
            std::wcout << L"    Expected: " << fullDllPath << L"\n\n";
            std::wcout << L"Press any key to exit...\n";
            _getch();
            return 1;
        }
    }

    std::wcout << L"[+] Found DLL: " << fullDllPath << L"\n";
    DWORD pid = 0;
    HWND hGameWindow = nullptr;

    while (true)
    {
        std::wcout << L"[*] Looking for " << targetProcess << L"...\n";
        while ((pid = GetProcessIdByName(targetProcess)) == 0)
        {
            Sleep(500);
        }
        std::wcout << L"[+] Target process found! (PID: " << pid << L")\n";

        // Wait for the window to be created
        std::wcout << L"[*] Waiting for game window to be ready...\n";
        bool processDied = false;
        
        while (!(hGameWindow = GetGameWindow(pid)))
        {
            Sleep(500);
            
            // Check if process still exists
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc)
            {
                DWORD exitCode = 0;
                if (GetExitCodeProcess(hProc, &exitCode) && exitCode != STILL_ACTIVE)
                {
                    processDied = true;
                }
                CloseHandle(hProc);
            }
            else
            {
                processDied = true;
            }

            if (processDied)
            {
                std::wcout << L"[-] Process closed before window was ready. Restarting search...\n\n";
                break;
            }
        }

        if (processDied) 
        {
            continue; // Loop back and search for a new PID
        }

        if (hGameWindow)
        {
            std::wcout << L"[+] Game window detected. Verifying responsiveness...\n";
            DWORD_PTR result;
            int checkCount = 0;
            while (!SendMessageTimeoutW(hGameWindow, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 1000, &result) && checkCount < 10)
            {
                // Verify process didn't die while we were checking window responsiveness
                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                if (hProc)
                {
                    DWORD exitCode = 0;
                    if (GetExitCodeProcess(hProc, &exitCode) && exitCode != STILL_ACTIVE)
                        processDied = true;
                    CloseHandle(hProc);
                }
                else
                {
                    processDied = true;
                }
                
                if (processDied)
                {
                    std::wcout << L"[-] Process died during responsiveness check. Restarting search...\n\n";
                    break;
                }
                
                std::wcout << L"[*] Game is busy initializing... waiting...\n";
                Sleep(1000);
                checkCount++;
            }
        }
        
        if (processDied)
        {
            continue; // Loop back and search for a new PID
        }

        // If we got here, the process is alive, window is found, and it's responsive.
        break;
    }

    std::wcout << L"[+] Injecting " << dllName << L" into process...\n";

    if (InjectDLL(pid, fullDllPath))
    {
        std::wcout << L"\n====================================================\n";
        std::wcout << L"[+] INJECTION SUCCESSFUL!\n";
        std::wcout << L"====================================================\n";
    }
    else
    {
        std::wcout << L"\n[-] INJECTION FAILED. Check error messages above.\n";
    }

    std::wcout << L"\nClosing in 5 seconds...\n";
    Sleep(5000);
    return 0;
}
