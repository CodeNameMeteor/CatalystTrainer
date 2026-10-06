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

    static PresentFn oPresent = nullptr;

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

    static void ShutdownDx11Backend()
    {
        if (bDx11Ready)
        {
            ImGui_ImplDX11_Shutdown();
            bDx11Ready = false;
        }
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

    static bool IsWindowDrawable(HWND window)
    {
        RECT rect{};
        return window && !IsIconic(window) && GetClientRect(window, &rect) &&
               rect.right > rect.left && rect.bottom > rect.top;
    }

    // Makes sure ImGui and its DX11 backend match the device and window being presented to.
    // Returns false to skip drawing on this Present call.
    static bool EnsureInitialized(IDXGISwapChain* pSwapChain)
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(pSwapChain->GetDesc(&desc)) || !desc.OutputWindow)
            return false;

        // If the game presents to a second window while the one we're attached to is still alive and shown,
        // ignore it rather than bouncing the window hook back and forth
        HWND current = hGameWindow.load();
        if (current && current != desc.OutputWindow && IsWindow(current) && IsWindowVisible(current))
            return false;

        ID3D11Device* device = nullptr;
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) || !device)
            return false;

        std::lock_guard<std::mutex> lock(g_ImGuiMutex);

        // The game recreated its device (e.g. after a display mode change): rebuild only the DX11 backend.
        // The ImGui context and the window procedure hook stay in place.
        if (bDx11Ready && device != pDevice)
        {
            Log("Present: D3D11 device changed, re-initialising renderer");
            ShutdownDx11Backend();
        }

        // The game recreated its window: move the window hook and ImGui's Win32 backend to the new one
        if (current != desc.OutputWindow)
        {
            if (current)
                Log("Present: game window changed, re-attaching");
            if (bImGuiContext)
            {
                ImGui_ImplWin32_Shutdown();
                ImGui_ImplWin32_Init(desc.OutputWindow);
            }
            InstallWndProc(desc.OutputWindow);
        }

        if (!bImGuiContext)
            CreateImGuiContext(desc.OutputWindow);

        if (bDx11Ready)
        {
            device->Release();
            return true;
        }

        pDevice = device; // Keeps the reference from GetDevice
        pDevice->GetImmediateContext(&pContext);

        if (!ImGui_ImplDX11_Init(pDevice, pContext))
        {
            Log("Present: ImGui DX11 backend failed to initialise");
            ShutdownDx11Backend();
            return false;
        }

        bDx11Ready = true;
        return true;
    }

    static void RenderFrame(IDXGISwapChain* pSwapChain)
    {
        std::lock_guard<std::mutex> lock(g_ImGuiMutex);

        // The back buffer is only referenced while drawing. Holding a view on it between frames stops the game
        // from resizing or releasing its swap chain; in exclusive fullscreen the old swap chain then keeps the
        // display and the game's new swap chain fails to go fullscreen, which crashes the game.
        ID3D11Texture2D* backBuffer = nullptr;
        if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer))) || !backBuffer)
            return;

        ID3D11RenderTargetView* renderTarget = nullptr;
        HRESULT hr = pDevice->CreateRenderTargetView(backBuffer, nullptr, &renderTarget);
        backBuffer->Release();
        if (FAILED(hr) || !renderTarget)
        {
            static bool s_Logged = false;
            if (!s_Logged)
            {
                Log("Present: could not create a render target view (0x%08lX)", static_cast<unsigned long>(hr));
                s_Logged = true;
            }
            return;
        }

        ApplyUiScale();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        UI::Menu::Render();
        UI::Menu::RenderOverlay();

        ImGui::Render();

        // ImGui's DX11 backend restores most pipeline state but not the render targets, so put the game's back
        ID3D11RenderTargetView* savedTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
        ID3D11DepthStencilView* savedDepth = nullptr;
        pContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, savedTargets, &savedDepth);

        pContext->OMSetRenderTargets(1, &renderTarget, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        pContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, savedTargets, savedDepth);
        for (ID3D11RenderTargetView* target : savedTargets)
        {
            if (target) target->Release();
        }
        if (savedDepth) savedDepth->Release();
        renderTarget->Release();
    }

    HRESULT __stdcall HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        InFlightGuard guard;

        // After an unload request the trainer's own thread tears everything down; just pass through.
        // DXGI_PRESENT_TEST only checks whether the window is occluded and shows nothing, so don't draw for it,
        // and don't draw while the window is minimised (e.g. after alt-tabbing out of fullscreen).
        if (!g_UnloadRequested.load() && !(Flags & DXGI_PRESENT_TEST) &&
            EnsureInitialized(pSwapChain) && IsWindowDrawable(hGameWindow.load()))
        {
            RenderFrame(pSwapChain);
        }

        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    bool InitializeHooks()
    {
        if (kiero::init(kiero::RenderType::D3D11) != kiero::Status::Success)
            return false;

        // Only Present is hooked: the trainer holds no swap chain resources between frames, so it doesn't need
        // to react to ResizeBuffers
        if (kiero::bind(8, reinterpret_cast<void**>(&oPresent), reinterpret_cast<void*>(HookedPresent)) != kiero::Status::Success)
        {
            Log("Hooks: failed to hook Present");
            kiero::shutdown();
            MH_Uninitialize();
            return false;
        }

        Log("Hooks: Present hooked");
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
