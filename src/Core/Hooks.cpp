#include "Core/Hooks.hpp"
#include "Core/Config.hpp"
#include "Core/Log.hpp"
#include "Core/Memory.hpp"
#include "Game/GameState.hpp"
#include "Features/TrainerFeatures.hpp"
#include "UI/Menu.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <string>
#include "kiero/kiero.h"
#include "MinHook.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Core
{
    typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain*, UINT, UINT);
    typedef HRESULT(__stdcall* ResizeBuffersFn)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    static PresentFn oPresent = nullptr;
    static ResizeBuffersFn oResizeBuffers = nullptr;

    // Shared with the game's window thread
    static std::atomic<WNDPROC> oWndProc{ nullptr };
    static std::atomic<HWND> hGameWindow{ nullptr };
    static std::atomic<bool> g_UnloadRequested{ false };

    // Number of threads currently executing one of our hook functions; the module is only freed at zero
    static std::atomic<int> g_HooksInFlight{ 0 };

    // Dear ImGui is not thread-safe: the window thread feeds it input while the render thread builds frames
    static std::mutex g_ImGuiMutex;

    // Render thread state (guarded by g_ImGuiMutex where the window thread or teardown can also touch it)
    static ID3D11Device* pDevice = nullptr;
    static ID3D11DeviceContext* pContext = nullptr;
    static ID3D11RenderTargetView* pRenderTarget = nullptr;
    static IDXGISwapChain* pLastSwapChain = nullptr;
    static bool bImGuiContext = false;   // ImGui context + Win32 backend created
    static bool bDx11Ready = false;      // DX11 backend initialised for pDevice
    static std::string g_ImGuiIniPath;
    static ImGuiStyle g_BaseStyle;
    static float g_AppliedUiScale = -1.0f;

    struct InFlightGuard
    {
        InFlightGuard() { g_HooksInFlight.fetch_add(1); }
        ~InFlightGuard() { g_HooksInFlight.fetch_sub(1); }
    };

    HWND GetGameWindow()
    {
        return hGameWindow.load();
    }

    static bool IsKeyPressMessage(UINT uMsg)
    {
        return uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN || uMsg == WM_CHAR || uMsg == WM_SYSCHAR ||
               uMsg == WM_DEADCHAR || uMsg == WM_SYSDEADCHAR;
    }

    static bool IsMouseButtonUpMessage(UINT uMsg)
    {
        return uMsg == WM_LBUTTONUP || uMsg == WM_RBUTTONUP || uMsg == WM_MBUTTONUP || uMsg == WM_XBUTTONUP;
    }

    LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        InFlightGuard guard;
        WNDPROC original = oWndProc.load();

        if (g_UnloadRequested.load())
            return CallWindowProcW(original, hWnd, uMsg, wParam, lParam);

        if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)
        {
            // Bit 30 is set for auto-repeat messages while a key is held; only react to the first press
            const bool isRepeat = (lParam & (1 << 30)) != 0;
            const int pressedKey = static_cast<int>(wParam);

            if (std::atomic<int>* binding = UI::g_ActiveBindingKey.load())
            {
                if (!isRepeat)
                {
                    if (pressedKey != VK_ESCAPE)
                    {
                        binding->store(pressedKey);
                        Core::ConfigManager::Get().Save();
                    }
                    UI::g_ActiveBindingKey.store(nullptr);
                }
                return 0;
            }

            if (!isRepeat)
                Features::TrainerFeatures::Get().HandleInput(pressedKey);
        }

        if (ConfigManager::Get().State.isMenuOpen)
        {
            {
                std::lock_guard<std::mutex> lock(g_ImGuiMutex);
                if (bImGuiContext)
                    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
            }

            // Keep Alt+F4 working while the menu is open
            if (uMsg == WM_SYSKEYDOWN && wParam == VK_F4)
                return CallWindowProcW(original, hWnd, uMsg, wParam, lParam);

            // Block key presses and mouse input from reaching the game while the menu is open. Key-up and
            // button-up messages still go through so keys held when the menu opened don't get stuck.
            if (IsKeyPressMessage(uMsg)) return 0;
            if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST && !IsMouseButtonUpMessage(uMsg)) return 0;
            if (uMsg == WM_SETCURSOR) return TRUE;

            // Raw input must still reach DefWindowProc so the system can clean up its buffers
            if (uMsg == WM_INPUT) return DefWindowProcW(hWnd, uMsg, wParam, lParam);
        }

        return CallWindowProcW(original, hWnd, uMsg, wParam, lParam);
    }

    static void InstallWndProc(HWND window)
    {
        HWND current = hGameWindow.load();
        if (current == window && oWndProc.load())
            return;

        // The swap chain moved to another window: give the old one its window procedure back first
        if (current && oWndProc.load() &&
            GetWindowLongPtrW(current, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(HookedWndProc))
        {
            SetWindowLongPtrW(current, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc.load()));
        }

        // Publish the original procedure before installing ours, so HookedWndProc never sees nullptr
        oWndProc.store(reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC)));
        hGameWindow.store(window);
        LONG_PTR previous = SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc));
        if (previous && previous != reinterpret_cast<LONG_PTR>(HookedWndProc))
            oWndProc.store(reinterpret_cast<WNDPROC>(previous));
    }

    static void ReleaseRenderTarget()
    {
        if (pRenderTarget)
        {
            pRenderTarget->Release();
            pRenderTarget = nullptr;
        }
    }

    static void CreateRenderTarget(IDXGISwapChain* pSwapChain)
    {
        ID3D11Texture2D* pBackBuffer = nullptr;
        if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer))) || !pBackBuffer)
        {
            Log("Present: could not get the back buffer");
            return;
        }

        if (FAILED(pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pRenderTarget)))
        {
            Log("Present: could not create a render target view");
            pRenderTarget = nullptr;
        }
        pBackBuffer->Release();
    }

    static void ShutdownDx11Backend()
    {
        if (bDx11Ready)
        {
            ImGui_ImplDX11_Shutdown();
            bDx11Ready = false;
        }
        ReleaseRenderTarget();
        if (pContext)
        {
            pContext->Release();
            pContext = nullptr;
        }
        if (pDevice)
        {
            pDevice->Release();
            pDevice = nullptr;
        }
        pLastSwapChain = nullptr;
    }

    static void ApplyStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.12f, 0.95f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.22f, 0.35f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.35f, 0.65f, 0.8f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.45f, 0.85f, 1.0f);
        // Brighter than the default grey so hint text stays readable on the dark background
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.66f, 0.66f, 0.70f, 1.0f);
        g_BaseStyle = style;
        g_AppliedUiScale = -1.0f;
    }

    // Automatic UI scale: Windows display scaling, or the window height relative to 1080p, whichever is larger
    static float GetAutoUiScale(HWND window)
    {
        float scale = 1.0f;

        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        static auto pGetDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
            reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow")));
        if (pGetDpiForWindow && window)
        {
            UINT dpi = pGetDpiForWindow(window);
            if (dpi > 0) scale = (std::max)(scale, dpi / 96.0f);
        }

        RECT rect{};
        if (window && GetClientRect(window, &rect) && rect.bottom > 0)
            scale = (std::max)(scale, static_cast<float>(rect.bottom) / 1080.0f);

        return (std::clamp)(scale, 1.0f, kMaxUiScale);
    }

    static void ApplyUiScale()
    {
        float requested = ConfigManager::Get().State.uiScale;
        float scale = requested > 0.0f ? (std::clamp)(requested, kMinUiScale, kMaxUiScale) : GetAutoUiScale(hGameWindow.load());
        if (std::abs(scale - g_AppliedUiScale) < 0.01f)
            return;

        ImGuiStyle& style = ImGui::GetStyle();
        style = g_BaseStyle;
        style.ScaleAllSizes(scale);
        ImGui::GetIO().FontGlobalScale = scale;
        g_AppliedUiScale = scale;
    }

    // Called with g_ImGuiMutex held
    static void CreateImGuiContext(HWND window)
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

        // Keep ImGui's window layout next to the trainer config instead of the game's working directory
        const auto& dir = ConfigManager::Get().GetDirectory();
        g_ImGuiIniPath = dir.empty() ? std::string("trainer_imgui.ini") : WideToUtf8((dir / L"trainer_imgui.ini").wstring());
        io.IniFilename = g_ImGuiIniPath.c_str();

        ApplyStyle();
        ImGui_ImplWin32_Init(window);
        bImGuiContext = true;
    }

    // Makes sure ImGui and its DX11 backend match the swap chain being presented. Returns false to skip this frame.
    static bool EnsureInitialized(IDXGISwapChain* pSwapChain)
    {
        ID3D11Device* device = nullptr;
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) || !device)
            return false;

        std::lock_guard<std::mutex> lock(g_ImGuiMutex);

        // Device or swap chain changed (e.g. on a full screen toggle): only the DX11 backend is recreated.
        // The ImGui context and the window procedure hook stay in place.
        if (bDx11Ready && (pSwapChain != pLastSwapChain || device != pDevice))
        {
            Log("Present: swap chain or device changed, re-initialising renderer");
            ShutdownDx11Backend();
        }

        if (bDx11Ready)
        {
            device->Release();
            return true;
        }

        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(pSwapChain->GetDesc(&desc)) || !desc.OutputWindow)
        {
            device->Release();
            return false;
        }

        if (bImGuiContext && hGameWindow.load() != desc.OutputWindow)
        {
            ImGui_ImplWin32_Shutdown();
            ImGui_ImplWin32_Init(desc.OutputWindow);
        }
        InstallWndProc(desc.OutputWindow);

        pDevice = device; // Keeps the reference from GetDevice
        pDevice->GetImmediateContext(&pContext);
        CreateRenderTarget(pSwapChain);

        if (!bImGuiContext)
            CreateImGuiContext(desc.OutputWindow);

        if (!ImGui_ImplDX11_Init(pDevice, pContext))
        {
            Log("Present: ImGui DX11 backend failed to initialise");
            ShutdownDx11Backend();
            return false;
        }

        bDx11Ready = true;
        pLastSwapChain = pSwapChain;
        return true;
    }

    static void RenderFrame()
    {
        std::lock_guard<std::mutex> lock(g_ImGuiMutex);

        ApplyUiScale();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        UI::Menu::Render();
        UI::Menu::RenderOverlay();

        ImGui::Render();
        if (pRenderTarget)
        {
            pContext->OMSetRenderTargets(1, &pRenderTarget, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
    }

    HRESULT __stdcall HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        InFlightGuard guard;

        // After an unload request the trainer's own thread tears everything down; just pass through
        if (!g_UnloadRequested.load() && EnsureInitialized(pSwapChain))
            RenderFrame();

        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    HRESULT __stdcall HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT Flags)
    {
        InFlightGuard guard;

        if (g_UnloadRequested.load())
            return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, Flags);

        bool ours = false;
        {
            // Our render target holds a back-buffer reference, which must be released before resizing
            std::lock_guard<std::mutex> lock(g_ImGuiMutex);
            ours = (pSwapChain == pLastSwapChain);
            if (ours && pRenderTarget)
            {
                if (pContext)
                    pContext->OMSetRenderTargets(0, nullptr, nullptr);
                ReleaseRenderTarget();
            }
        }

        HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, Flags);

        if (ours)
        {
            std::lock_guard<std::mutex> lock(g_ImGuiMutex);
            if (pDevice && pSwapChain == pLastSwapChain && !pRenderTarget)
                CreateRenderTarget(pSwapChain);
        }

        return hr;
    }

    bool InitializeHooks()
    {
        if (kiero::init(kiero::RenderType::D3D11) != kiero::Status::Success)
            return false;

        if (kiero::bind(8, reinterpret_cast<void**>(&oPresent), reinterpret_cast<void*>(HookedPresent)) != kiero::Status::Success ||
            kiero::bind(13, reinterpret_cast<void**>(&oResizeBuffers), reinterpret_cast<void*>(HookedResizeBuffers)) != kiero::Status::Success)
        {
            Log("Hooks: failed to hook Present/ResizeBuffers");
            kiero::shutdown();
            MH_Uninitialize();
            return false;
        }

        Log("Hooks: Present and ResizeBuffers hooked");
        return true;
    }

    bool TeardownHooks()
    {
        bool canFreeModule = true;

        // 1. Our hooks already pass straight through (unload flag). Stop new calls from entering them.
        kiero::shutdown();

        // 2. Give the game its window procedure back, unless something else subclassed the window after us.
        HWND window = hGameWindow.load();
        WNDPROC original = oWndProc.load();
        if (window && original)
        {
            if (GetWindowLongPtrW(window, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(HookedWndProc))
            {
                SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
            }
            else
            {
                // Removing ourselves would break the other program's chain; stay loaded but inactive instead
                Log("Unload: window procedure was subclassed by another program, keeping the trainer loaded (inactive)");
                canFreeModule = false;
            }
        }

        // 3. Wait for any thread still running inside a hook function
        Sleep(100);
        for (int i = 0; i < 2000 && g_HooksInFlight.load() > 0; ++i)
            Sleep(1);

        if (g_HooksInFlight.load() > 0)
        {
            Log("Unload: hook functions still busy, keeping the trainer loaded (inactive)");
            return false;
        }

        // 4. Release ImGui and D3D objects
        {
            std::lock_guard<std::mutex> lock(g_ImGuiMutex);
            ShutdownDx11Backend();
            if (bImGuiContext)
            {
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                bImGuiContext = false;
            }
        }

        // 5. Remove the hooks and free MinHook's trampolines
        if (canFreeModule)
            MH_Uninitialize();

        return canFreeModule;
    }

    void ShutdownHooks()
    {
        g_UnloadRequested.store(true);
    }

    void UnloadTrainer()
    {
        g_UnloadRequested.store(true);
    }

    bool IsUnloadRequested()
    {
        return g_UnloadRequested.load();
    }
}
