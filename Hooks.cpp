#include "Core/Hooks.hpp"
#include "Core/Config.hpp"
#include "Core/Memory.hpp"
#include "Game/GameState.hpp"
#include "Features/TrainerFeatures.hpp"
#include "UI/Menu.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <atomic>
#include <fstream>
#include <string>
#include "kiero/kiero.h"
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
    static WNDPROC oWndProc = nullptr;

    static ID3D11Device* pDevice = nullptr;
    static ID3D11DeviceContext* pContext = nullptr;
    static ID3D11RenderTargetView* pRenderTarget = nullptr;
    static HWND hGameWindow = nullptr;
    static bool bInitialized = false;
    static std::atomic<bool> g_UnloadRequested{ false };
    static std::atomic<bool> g_HooksInstalled{ false };

    static void WriteHookLog(const char* message)
    {
        // Keep diagnostics beside the DLL so failures are still visible when the
        // injector has already exited.
        char modulePath[MAX_PATH] = {};
        if (g_hModule && GetModuleFileNameA(g_hModule, modulePath, MAX_PATH))
        {
            std::string path(modulePath);
            const size_t slash = path.find_last_of("\\/");
            path = (slash == std::string::npos ? std::string{} : path.substr(0, slash)) + "\\MEC_Trainer.log";
            std::ofstream log(path, std::ios::app);
            if (log) log << message << '\n';
        }
    }

    static void PollHotkeys()
    {
        // Some game/input configurations do not deliver keyboard messages to the
        // game window. Polling the desktop key state gives the trainer a reliable
        // fallback while the WndProc hook remains useful for ImGui/remapping.
        static bool previousKeys[7] = {};
        if (UI::g_ActiveBindingKey != nullptr) return;
        auto& keys = ConfigManager::Get().Keys;
        int configuredKeys[] = {
            keys.toggleMenu, keys.godMode, keys.noclip, keys.noStumble,
            keys.setTeleport, keys.gotoTeleport, keys.bhop
        };

        for (size_t i = 0; i < sizeof(configuredKeys) / sizeof(configuredKeys[0]); ++i)
        {
            const int key = configuredKeys[i];
            if (key <= 0 || key >= 256) continue;
            const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
            if (down && !previousKeys[i])
                Features::TrainerFeatures::Get().HandleInput(key);
            previousKeys[i] = down;
        }
    }

    LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)
        {
            if (UI::g_ActiveBindingKey != nullptr)
            {
                int pressedKey = static_cast<int>(wParam);
                if (pressedKey != VK_ESCAPE)
                {
                    *UI::g_ActiveBindingKey = pressedKey;
                    Core::ConfigManager::Get().Save();
                }
                UI::g_ActiveBindingKey = nullptr;
                return true;
            }

            // Hotkeys are polled from HookedPresent. Handling them here as well
            // would toggle twice on games that dispatch both paths.
        }

        if (ConfigManager::Get().State.isMenuOpen)
        {
            ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

            // Block game from capturing mouse or processing input while trainer menu is open
            if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST) return true;
            if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) return true;
            if (uMsg == WM_INPUT || uMsg == WM_SETCURSOR) return true;
        }

        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    HRESULT __stdcall HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        if (g_UnloadRequested.load())
        {
            // 1. Restore game inputs and cursor
            auto& cfg = ConfigManager::Get();
            auto& game = Game::GameState::Get();
            cfg.State.isMenuOpen = false;
            Core::Memory::SafeWrite<int>(game.Addrs.inputEnabled, 0);
            Core::Memory::SafeWrite<int>(game.Addrs.mouseEnabled, 0);
            ImGui::GetIO().MouseDrawCursor = false;

            // Restore original WndProc
            if (hGameWindow && oWndProc)
            {
                SetWindowLongPtr(hGameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
                oWndProc = nullptr;
            }

            // 3. Clean DirectX & ImGui resources
            if (pRenderTarget)
            {
                pRenderTarget->Release();
                pRenderTarget = nullptr;
            }
            // add reset movement properties to default 
            if (bInitialized)
            {
                ImGui_ImplDX11_Shutdown();
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                bInitialized = false;
            }

            kiero::shutdown();

            // 4. Eject module safely
            HMODULE mod = g_hModule;
            CreateThread(nullptr, 0, [](LPVOID param) -> DWORD {
                Sleep(150);
                FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(param), 0);
                return 0;
            }, mod, 0, nullptr);

            return oPresent(pSwapChain, SyncInterval, Flags);
        }

        if (!bInitialized)
        {
            if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice))))
            {
                pDevice->GetImmediateContext(&pContext);
                DXGI_SWAP_CHAIN_DESC desc;
                pSwapChain->GetDesc(&desc);
                hGameWindow = desc.OutputWindow;

                ID3D11Texture2D* pBackBuffer = nullptr;
                pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
                pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pRenderTarget);
                pBackBuffer->Release();

                oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hGameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));

                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

                // Load custom styling
                ImGuiStyle& style = ImGui::GetStyle();
                style.WindowRounding = 4.0f;
                style.FrameRounding = 3.0f;
                style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.12f, 0.95f);
                style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.22f, 0.35f, 1.0f);
                style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.35f, 0.65f, 0.8f);
                style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.45f, 0.85f, 1.0f);

                ImGui_ImplWin32_Init(hGameWindow);
                ImGui_ImplDX11_Init(pDevice, pContext);

                bInitialized = true;
            }
            else
            {
                return oPresent(pSwapChain, SyncInterval, Flags);
            }
        }

        // Update Game State & Execute Active Cheats
        Game::GameState::Get().Update();
        PollHotkeys();
        Features::TrainerFeatures::Get().Tick();

        // Render UI
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        UI::Menu::Render();
        UI::Menu::RenderOverlay();

        ImGui::Render();
        pContext->OMSetRenderTargets(1, &pRenderTarget, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    HRESULT __stdcall HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT Flags)
    {
        if (pRenderTarget)
        {
            pContext->OMSetRenderTargets(0, nullptr, nullptr);
            pRenderTarget->Release();
            pRenderTarget = nullptr;
        }

        HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, Flags);

        ID3D11Texture2D* pBuffer = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBuffer));
        if (pBuffer)
        {
            pDevice->CreateRenderTargetView(pBuffer, nullptr, &pRenderTarget);
            pBuffer->Release();
        }

        return hr;
    }

    bool InitializeHooks()
    {
        if (g_HooksInstalled.load()) return true;

        const auto initStatus = kiero::init(kiero::RenderType::D3D11);
        if (initStatus != kiero::Status::Success && initStatus != kiero::Status::AlreadyInitializedError)
        {
            WriteHookLog("kiero::init(D3D11) failed");
            return false;
        }

        const auto presentStatus = kiero::bind(8, reinterpret_cast<void**>(&oPresent), HookedPresent);
        const auto resizeStatus = kiero::bind(13, reinterpret_cast<void**>(&oResizeBuffers), HookedResizeBuffers);
        if (presentStatus != kiero::Status::Success || resizeStatus != kiero::Status::Success || !oPresent)
        {
            WriteHookLog("D3D11 hook bind failed (Present/ResizeBuffers)");
            if (presentStatus == kiero::Status::Success) kiero::unbind(8);
            if (resizeStatus == kiero::Status::Success) kiero::unbind(13);
            kiero::shutdown();
            oPresent = nullptr;
            oResizeBuffers = nullptr;
            return false;
        }

        g_HooksInstalled.store(true);
        WriteHookLog("D3D11 hooks installed");
        return true;
    }

    void ShutdownHooks()
    {
        g_UnloadRequested.store(true);
    }

    void UnloadTrainer()
    {
        g_UnloadRequested.store(true);
    }
}
