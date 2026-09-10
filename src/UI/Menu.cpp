#include "UI/Menu.hpp"
#include "Core/Config.hpp"
#include "Core/Hooks.hpp"
#include "Game/GameState.hpp"
#include "Game/GameOffsets.hpp"
#include "Features/TrainerFeatures.hpp"
#include "Features/MovementProperties.hpp"
#include "imgui/imgui.h"

namespace UI
{
    int* g_ActiveBindingKey = nullptr;

    void Menu::DrawHotkey(const char* label, int* key)
    {
        bool isBinding = (g_ActiveBindingKey == key);

        std::string keyName = Core::ConfigManager::GetKeyName(*key);
        char btnLabel[128];
        if (isBinding)
        {
            sprintf_s(btnLabel, "[Press Key...]##%s", label);
        }
        else
        {
            sprintf_s(btnLabel, "[%s]##%s", keyName.c_str(), label);
        }

        if (isBinding)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.40f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.50f, 0.20f, 1.0f));
        }

        if (ImGui::Button(btnLabel, ImVec2(140, 0)))
        {
            g_ActiveBindingKey = isBinding ? nullptr : key;
        }

        if (isBinding)
        {
            ImGui::PopStyleColor(2);
        }

        ImGui::SameLine();
        ImGui::Text("%s", label);
    }

    void Menu::Render()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        static bool lastMenuState = false;
        if (cfg.State.isMenuOpen != lastMenuState)
        {
            lastMenuState = cfg.State.isMenuOpen;
            if (cfg.State.isMenuOpen)
            {
                ClipCursor(nullptr);
                Core::Memory::SafeWrite<int>(game.Addrs.inputEnabled, 1);
                Core::Memory::SafeWrite<int>(game.Addrs.mouseEnabled, 1);
            }
            else
            {
                if (game.IsInMenu() == 0)
                {
                    Core::Memory::SafeWrite<int>(game.Addrs.inputEnabled, 0);
                    Core::Memory::SafeWrite<int>(game.Addrs.mouseEnabled, 0);
                }
            }
        }

        if (!cfg.State.isMenuOpen)
        {
            ImGui::GetIO().MouseDrawCursor = false;
            return;
        }

        ImGui::GetIO().MouseDrawCursor = true;

        ImGui::SetNextWindowSize(ImVec2(620, 520), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Mirror's Edge Catalyst Trainer", &cfg.State.isMenuOpen, ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::BeginTabBar("TrainerTabs"))
            {
                if (ImGui::BeginTabItem("Trainer"))
                {
                    DrawTrainerTab();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Movement Tuning"))
                {
                    DrawMovementTab();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("World & Visuals"))
                {
                    DrawWorldTab();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Settings"))
                {
                    DrawSettingsTab();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void Menu::DrawMovementTab()
    {
        static int selectedCategory = 0;
        const char* categories[] = {
            "Running", "Wallclimbing", "Wallrunning", "Coil", "Uncontrolled Slide", "Mag Pullup", "Mag Swing"
        };

        ImGui::Combo("Category", &selectedCategory, categories, IM_ARRAYSIZE(categories));
        ImGui::Separator();

        Features::MovementManager::Get().RenderImGuiCategory(static_cast<Features::MovementCategory>(selectedCategory));

        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Reset All to Default"))
        {
            Features::MovementManager::Get().ResetAll();
        }
        ImGui::SameLine();
        if (ImGui::Button("Randomize Movement"))
        {
            Features::MovementManager::Get().RandomizeAll();
        }
    }

    void Menu::DrawWorldTab()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        ImGui::Text("World & Environment Controls");
        ImGui::Separator();

        float timeOfDay = game.GetTimeOfDay();
        int hours = static_cast<int>(timeOfDay / 3600.0f);
        if (ImGui::SliderInt("Time of Day (Hours)", &hours, 0, 24))
        {
            Core::Memory::SafeWrite<float>(game.Addrs.timeOfDay, static_cast<float>(hours * 3600));
        }

        ImGui::Checkbox("Freeze Time of Day", &cfg.State.freezeTime);
        if (ImGui::SliderFloat("Game Time Scale", &cfg.State.timeScale, 0.1f, 10.0f))
        {
            uintptr_t engine = Game::Offsets::EngineSettings();
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(engine, { 0x48 }), cfg.State.timeScale);
        }

        ImGui::Separator();
        ImGui::Text("Post Processing");
        if (ImGui::Checkbox("Bloom", &cfg.State.bloom))
        {
            uintptr_t vis = Game::Offsets::VisualsBase();
            Core::Memory::SafeWrite<int>(Core::Memory::ResolvePtrChain(vis, { 0x2450, 0x8, 0x22b }), cfg.State.bloom ? 1 : 0);
        }
        if (ImGui::Checkbox("Blur", &cfg.State.blur))
        {
            uintptr_t vis = Game::Offsets::VisualsBase();
            Core::Memory::SafeWrite<int>(Core::Memory::ResolvePtrChain(vis, { 0x2450, 0x8, 0x228 }), cfg.State.blur ? 1 : 0);
        }
        if (ImGui::Checkbox("Vignette (Use at own risk)", &cfg.State.vignette))
        {
            uintptr_t vis = Game::Offsets::VisualsBase();
            Core::Memory::SafeWrite<int>(Core::Memory::ResolvePtrChain(vis, { 0x2450, 0x8, 0x239 }), cfg.State.vignette ? 1 : 0);
        }
    }

    void Menu::DrawTrainerTab()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        // 1. Player Telemetry & Info
        ImGui::Text("Player Telemetry");
        ImGui::Checkbox("Show Player Info HUD Overlay", &cfg.State.showPlayerInfo);

        if (game.HasPlayer())
        {
            auto pos = game.GetPlayerPos();
            ImGui::Text("Pos: (%.2f, %.2f, %.2f) | Speed: %.2f m/s", pos.x, pos.y, pos.z, game.GetPlayerVelocity());
            ImGui::Text("State: %d | Last Ground Y: %.2f", game.GetPlayerState(), game.GetLastGroundY());
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Game telemetry unavailable (Loading / Inactive)");
        }

        ImGui::Separator();
        //ImGui::Text("Active Cheats");

        ImGui::Checkbox("God Mode (Immortality) ", &cfg.State.godMode);
        ImGui::Checkbox("Noclip (Fly Through World) ", &cfg.State.noclip);
        ImGui::Checkbox("No-Stumble (No Fall Damage Stumble) ", &cfg.State.noStumble);
        if (cfg.State.noclip)
        {
            ImGui::Indent();
            ImGui::SliderFloat("Noclip Speed", &cfg.State.noclipSpeed, 0.1f, 5.0f, "%.2fx");
            ImGui::Unindent();
        }

        ImGui::Separator();
        ImGui::Checkbox("Same Wall Climbs/Runs", &cfg.State.sameWall);
        ImGui::Checkbox("Infinite Wallclimbs", &cfg.State.infiniteWallclimbs);
        ImGui::Checkbox("Infinite Wallruns", &cfg.State.infiniteWallruns);
        ImGui::Checkbox("Fast Loads", &cfg.State.fastLoads);
    }

    void Menu::DrawSettingsTab()
    {
        auto& cfg = Core::ConfigManager::Get();
        ImGui::Text("Hotkeys (Click any button to remap, ESC to cancel)");
        ImGui::Separator();
        DrawHotkey("Toggle Menu", &cfg.Keys.toggleMenu);
        DrawHotkey("God Mode", &cfg.Keys.godMode);
        DrawHotkey("Noclip", &cfg.Keys.noclip);
        DrawHotkey("Decrease Noclip Speed", &cfg.Keys.decNoclipSpeed);
        DrawHotkey("Increase Noclip Speed", &cfg.Keys.incNoclipSpeed);
        DrawHotkey("No Stumble", &cfg.Keys.noStumble);
        DrawHotkey("Set Teleport", &cfg.Keys.setTeleport);
        DrawHotkey("Goto Teleport", &cfg.Keys.gotoTeleport);
        DrawHotkey("Bunnyhop", &cfg.Keys.bhop);

        ImGui::Separator();
        ImGui::Spacing();

        static std::string statusMsg = "";
        static DWORD statusTimer = 0;

        if (ImGui::Button("Save to config.ini"))
        {
            cfg.Save();
            statusMsg = "Saved: " + cfg.GetConfigPath();
            statusTimer = GetTickCount();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload from config.ini"))
        {
            cfg.Load();
            statusMsg = "Reloaded: " + cfg.GetConfigPath();
            statusTimer = GetTickCount();
        }

        if (!statusMsg.empty() && (GetTickCount() - statusTimer < 5000))
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", statusMsg.c_str());
        }
        else
        {
            ImGui::Spacing();
            ImGui::TextDisabled("File: %s", cfg.GetConfigPath().c_str());
        }

        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.60f, 0.10f, 0.10f, 1.0f));
        if (ImGui::Button("Unload / Eject Trainer", ImVec2(220, 32)))
        {
            Core::UnloadTrainer();
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        ImGui::TextDisabled("(Safely restores game hooks & unloads DLL)");
    }

    void Menu::RenderOverlay()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        // 1. Player Info HUD (spawns in bottom-right corner by default, fully movable)
        if (cfg.State.showPlayerInfo && game.HasPlayer())
        {
            ImGuiIO& io = ImGui::GetIO();
            // Set pivot to bottom-right (1.0f, 1.0f) to make it perfectly flush with the corner
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_FirstUseEver, ImVec2(1.0f, 1.0f));
            ImGui::Begin("HUD_PlayerInfo", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
            auto pos = game.GetPlayerPos();
            ImGui::Text("Pos: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
            ImGui::Text("Speed: %.2f m/s", game.GetPlayerVelocity());
            ImGui::Text("State: %d", game.GetPlayerState());
            ImGui::End();
        }

        // 2. Active Cheats HUD Overlay (God, Noclip, Nostumble, Bhop)
        if (cfg.State.trainerOverlay)
        {
            std::vector<const char*> tags;
            if (cfg.State.godMode) tags.push_back("God");
            if (cfg.State.noclip) tags.push_back("Noclip");
            if (cfg.State.noStumble) tags.push_back("Nostumble");
            if (cfg.State.bhop) tags.push_back("Bhop");

            if (!tags.empty())
            {
                std::string activeCheats;
                for (size_t i = 0; i < tags.size(); ++i)
                {
                    if (i > 0) activeCheats += " ";
                    activeCheats += tags[i];
                }

                ImDrawList* drawList = ImGui::GetBackgroundDrawList();
                ImVec2 textSize = ImGui::CalcTextSize(activeCheats.c_str());
                ImVec2 pos = ImVec2(10.0f, 10.0f);

                // Draw dark background pill
                drawList->AddRectFilled(
                    ImVec2(pos.x - 6.0f, pos.y - 4.0f),
                    ImVec2(pos.x + textSize.x + 6.0f, pos.y + textSize.y + 4.0f),
                    IM_COL32(0, 0, 0, 180),
                    4.0f
                );

                // Draw bright neon green text
                drawList->AddText(pos, IM_COL32(50, 255, 120, 255), activeCheats.c_str());
            }
        }
    }
}
