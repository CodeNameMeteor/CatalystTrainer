#include "Core/Hooks.hpp"
#include "Core/Config.hpp"
#include "Core/Memory.hpp"
#include "Game/GameState.hpp"
#include "Features/TrainerFeatures.hpp"
#include "UI/Menu.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <atomic>
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

            Features::TrainerFeatures::Get().HandleInput(static_cast<int>(wParam));
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
        if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
        {
            kiero::bind(8, reinterpret_cast<void**>(&oPresent), HookedPresent);
            kiero::bind(13, reinterpret_cast<void**>(&oResizeBuffers), HookedResizeBuffers);
            return true;
        }
        return false;
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
