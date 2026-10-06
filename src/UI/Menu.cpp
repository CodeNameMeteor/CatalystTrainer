#include "UI/Menu.hpp"
#include "Core/Config.hpp"
#include "Core/Hooks.hpp"
#include "Core/Memory.hpp"
#include "Game/GameState.hpp"
#include "Game/GameOffsets.hpp"
#include "Features/TrainerFeatures.hpp"
#include "Features/MovementProperties.hpp"
#include "imgui/imgui.h"
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace UI
{
    std::atomic<std::atomic<int>*> g_ActiveBindingKey{ nullptr };

    static bool AtomicCheckbox(const char* label, std::atomic<bool>& value)
    {
        bool local = value.load();
        if (ImGui::Checkbox(label, &local))
        {
            value.store(local);
            return true;
        }
        return false;
    }

    static bool AtomicSliderFloat(const char* label, std::atomic<float>& value, float minValue, float maxValue, const char* format)
    {
        float local = value.load();
        if (ImGui::SliderFloat(label, &local, minValue, maxValue, format))
        {
            value.store(local);
            return true;
        }
        return false;
    }

    // Tooltip on the previous item (also reachable with keyboard/gamepad navigation)
    static void Tooltip(const char* text)
    {
        if (ImGui::IsItemHovered() || ImGui::IsItemFocused())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void Menu::DrawHotkey(const char* label, std::atomic<int>* key, bool isDuplicate)
    {
        const bool isBinding = (g_ActiveBindingKey.load() == key);

        std::string btnLabel = isBinding
            ? std::string("[Press Key...]##") + label
            : "[" + Core::ConfigManager::GetKeyDisplayName(key->load()) + "]##" + label;

        if (isBinding)
        {
            // Dark enough for white text to stay readable (contrast above 4.5:1)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.30f, 0.05f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.74f, 0.33f, 0.06f, 1.0f));
        }

        float scale = ImGui::GetIO().FontGlobalScale;
        if (ImGui::Button(btnLabel.c_str(), ImVec2(140.0f * scale, 0)))
        {
            g_ActiveBindingKey.store(isBinding ? nullptr : key);
        }

        if (isBinding)
        {
            ImGui::PopStyleColor(2);
        }

        ImGui::SameLine();
        ImGui::TextUnformatted(label);

        if (isDuplicate)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "(shared with another action)");
        }
    }

    void Menu::Render()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        static bool lastMenuState = false;
        const bool menuOpen = cfg.State.isMenuOpen;
        if (menuOpen != lastMenuState)
        {
            lastMenuState = menuOpen;
            auto addrs = game.GetAddrs();
            if (menuOpen)
            {
                ClipCursor(nullptr);
                Core::Memory::SafeWrite<int>(addrs.inputEnabled, 1);
                Core::Memory::SafeWrite<int>(addrs.mouseEnabled, 1);
            }
            else
            {
                g_ActiveBindingKey.store(nullptr);
                if (!game.IsInMenu())
                {
                    Core::Memory::SafeWrite<int>(addrs.inputEnabled, 0);
                    Core::Memory::SafeWrite<int>(addrs.mouseEnabled, 0);
                }
            }
        }

        if (!menuOpen)
        {
            ImGui::GetIO().MouseDrawCursor = false;
            return;
        }

        ImGui::GetIO().MouseDrawCursor = true;

        float scale = ImGui::GetIO().FontGlobalScale;
        ImGui::SetNextWindowSize(ImVec2(620.0f * scale, 520.0f * scale), ImGuiCond_FirstUseEver);
        bool open = true;
        if (ImGui::Begin("Mirror's Edge Catalyst Trainer", &open, ImGuiWindowFlags_NoCollapse))
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

        if (!open)
            cfg.State.isMenuOpen = false;
    }

    void Menu::DrawMovementTab()
    {
        static int selectedCategory = 0;
        const char* categories[] = {
            "Running", "Wallclimbing", "Wallrunning", "Coil", "Uncontrolled Slide", "Mag Pullup", "Mag Swing"
        };

        ImGui::Combo("Category", &selectedCategory, categories, IM_ARRAYSIZE(categories));
        ImGui::TextDisabled("Values come from the game. Edits are applied when you press Enter or leave the field.");
        ImGui::Separator();

        Features::MovementManager::Get().RenderImGuiCategory(static_cast<Features::MovementCategory>(selectedCategory));

        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Reset All to Game Values"))
        {
            Features::MovementManager::Get().ResetAll();
        }
        Tooltip("Puts every value you changed back to what the game had before.");
        ImGui::SameLine();
        if (ImGui::Button("Randomize Movement"))
        {
            Features::MovementManager::Get().RandomizeAll();
        }
        Tooltip("Sets most movement values to random amounts between half and 2.5 times their usual value.");
    }

    void Menu::DrawWorldTab()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();
        auto& features = Features::TrainerFeatures::Get();

        ImGui::Text("World & Environment Controls");
        ImGui::Separator();

        const float currentRawTime = cfg.State.freezeTime ? cfg.State.frozenTimeValue.load() : game.GetTimeOfDay();
        float hours = Game::HourOfDay(currentRawTime);

        if (ImGui::SliderFloat("Time of Day (Hours)", &hours, 0.0f, 23.99f, "%.2f hrs"))
        {
            features.RequestTimeOfDay(hours);
        }

        if (AtomicCheckbox("Freeze Time of Day", cfg.State.freezeTime) && cfg.State.freezeTime)
        {
            // Capture the current time at the exact instant freeze is turned on
            cfg.State.frozenTimeValue = game.GetTimeOfDay();
        }

        if (AtomicSliderFloat("Game Time Scale", cfg.State.timeScale, 0.1f, 10.0f, "%.2fx"))
        {
            features.RequestTimeScale();
        }
        Tooltip("Speeds up or slows down the whole game. Restored when the trainer is unloaded.");

        ImGui::Separator();
        ImGui::Text("Post Processing");
        ImGui::TextDisabled("Checkboxes show the game's current settings once in game.");

        if (AtomicCheckbox("Bloom", cfg.State.bloom)) features.RequestVisuals();
        if (AtomicCheckbox("Blur", cfg.State.blur)) features.RequestVisuals();
        if (AtomicCheckbox("Vignette", cfg.State.vignette)) features.RequestVisuals();
        Tooltip("Experimental: this flag was found by hand and may not behave the same in every scene.");
    }

    void Menu::DrawTrainerTab()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        const auto buildStatus = Game::BuildInfo::Check(Game::BuildInfo::Detect());
        if (buildStatus == Game::BuildInfo::Status::Unsupported)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Unsupported game version: cheats are disabled (see Settings).");
            ImGui::Separator();
        }

        // 1. Player Telemetry & Info
        ImGui::Text("Player Telemetry");
        AtomicCheckbox("Show Player Info HUD Overlay", cfg.State.showPlayerInfo);

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

        AtomicCheckbox("God Mode (Immortality)", cfg.State.godMode);
        Tooltip("You can't die. Re-applied automatically after respawns and level loads.");
        AtomicCheckbox("Noclip (Fly Through World)", cfg.State.noclip);
        Tooltip("Fly freely through walls. Movement keys can be changed in Settings. Includes God Mode while active.");
        AtomicCheckbox("No-Stumble (No Fall Damage Stumble)", cfg.State.noStumble);
        Tooltip("Removes the stumble after landing from a big drop.");
        if (cfg.State.noclip)
        {
            ImGui::Indent();
            AtomicSliderFloat("Noclip Speed", cfg.State.noclipSpeed, Core::kMinNoclipSpeed, Core::kMaxNoclipSpeed, "%.2fx");
            ImGui::Unindent();
        }

        ImGui::Separator();
        AtomicCheckbox("Same Wall Climbs/Runs", cfg.State.sameWall);
        Tooltip("Lets you wallrun or wallclimb the same wall again without touching the ground.");
        AtomicCheckbox("Infinite Wallclimbs", cfg.State.infiniteWallclimbs);
        Tooltip("No limit on how many wallclimbs you can chain.");
        AtomicCheckbox("Infinite Wallruns", cfg.State.infiniteWallruns);
        Tooltip("No limit on how many wallruns you can chain.");
        AtomicCheckbox("Fast Loads", cfg.State.fastLoads);
        Tooltip("Runs the engine much faster while a loading screen is showing. Has no effect during normal play.");

        ImGui::Spacing();
        ImGui::TextDisabled("Please don't submit leaderboard or time trial results while using the trainer.");
    }

    void Menu::DrawSettingsTab()
    {
        auto& cfg = Core::ConfigManager::Get();
        ImGui::Text("Hotkeys (Click any button to remap, ESC to cancel)");
        ImGui::Separator();

        const auto& table = Core::GetKeybindTable();
        std::map<int, int> keyUses;
        for (const auto& entry : table)
            keyUses[(cfg.Keys.*(entry.member)).load()]++;

        for (const auto& entry : table)
        {
            std::atomic<int>* key = &(cfg.Keys.*(entry.member));
            DrawHotkey(entry.label, key, keyUses[key->load()] > 1);
        }

        ImGui::Separator();
        ImGui::Text("Display");

        bool autoScale = cfg.State.uiScale <= 0.0f;
        if (ImGui::Checkbox("Automatic UI scale", &autoScale))
        {
            cfg.State.uiScale = autoScale ? 0.0f : ImGui::GetIO().FontGlobalScale;
            cfg.Save();
        }
        if (!autoScale)
        {
            float uiScale = cfg.State.uiScale;
            ImGui::SliderFloat("UI Scale", &uiScale, Core::kMinUiScale, Core::kMaxUiScale, "%.2fx");
            // Applied once the slider is released so the window doesn't resize under the mouse
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                cfg.State.uiScale = uiScale;
                cfg.Save();
            }
        }

        AtomicCheckbox("Show active cheats overlay", cfg.State.trainerOverlay);
        const char* corners[] = { "Top Left", "Top Right", "Bottom Left", "Bottom Right" };
        int corner = cfg.State.overlayCorner;
        if (ImGui::Combo("Overlay position", &corner, corners, IM_ARRAYSIZE(corners)))
            cfg.State.overlayCorner = corner;

        ImGui::Separator();
        ImGui::Spacing();

        static std::string statusMsg = "";
        static DWORD statusTimer = 0;

        if (ImGui::Button("Save Config"))
        {
            cfg.Save();
            statusMsg = "Saved: " + cfg.GetConfigPathUtf8();
            statusTimer = GetTickCount();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload Config"))
        {
            cfg.Load();
            statusMsg = "Reloaded: " + cfg.GetConfigPathUtf8();
            statusTimer = GetTickCount();
        }

        ImGui::Spacing();
        if (!statusMsg.empty() && (GetTickCount() - statusTimer < 5000))
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", statusMsg.c_str());
        else
            ImGui::TextDisabled("File: %s", cfg.GetConfigPathUtf8().c_str());

        ImGui::Separator();
        const auto detected = Game::BuildInfo::Detect();
        const auto status = Game::BuildInfo::Check(detected);
        const char* statusText = status == Game::BuildInfo::Status::Supported ? "supported"
                               : status == Game::BuildInfo::Status::Unverified ? "not verified"
                               : "UNSUPPORTED - memory writes disabled";
        ImGui::TextDisabled("Game build: timestamp 0x%08X, image size 0x%08X (%s)",
            detected.timeDateStamp, detected.sizeOfImage, statusText);

        ImGui::Separator();
        ImGui::Spacing();
        float scale = ImGui::GetIO().FontGlobalScale;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.60f, 0.10f, 0.10f, 1.0f));
        if (ImGui::Button("Unload / Eject Trainer", ImVec2(220.0f * scale, 32.0f * scale)))
        {
            Core::UnloadTrainer();
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        ImGui::TextDisabled("(Restores the game's settings & unloads the DLL)");
    }

    void Menu::RenderOverlay()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();
        ImGuiIO& io = ImGui::GetIO();
        const float scale = io.FontGlobalScale;

        // 1. Player Info HUD (spawns in bottom-right corner by default, fully movable)
        if (cfg.State.showPlayerInfo && game.HasPlayer())
        {
            // Set pivot to bottom-right (1.0f, 1.0f) to make it perfectly flush with the corner
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_FirstUseEver, ImVec2(1.0f, 1.0f));
            ImGui::Begin("HUD_PlayerInfo", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
            auto pos = game.GetPlayerPos();
            ImGui::Text("Pos: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
            ImGui::Text("Speed: %.2f m/s", game.GetPlayerVelocity());
            ImGui::Text("State: %d", game.GetPlayerState());
            ImGui::End();
        }

        // 2. Active Cheats HUD Overlay (God, Noclip, Nostumble, Bhop) and short status messages
        std::vector<std::string> lines;
        if (cfg.State.trainerOverlay)
        {
            std::string activeCheats;
            auto Add = [&](bool enabled, const char* tag) {
                if (!enabled) return;
                if (!activeCheats.empty()) activeCheats += " ";
                activeCheats += tag;
            };
            Add(cfg.State.godMode, "God");
            Add(cfg.State.noclip, "Noclip");
            Add(cfg.State.noStumble, "Nostumble");
            Add(cfg.State.bhop, "Bhop");
            if (!activeCheats.empty())
                lines.push_back(activeCheats);
        }

        std::string toast = Features::TrainerFeatures::Get().GetToast();
        if (!toast.empty())
            lines.push_back(toast);

        if (lines.empty())
            return;

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();
        const float margin = 10.0f * scale;
        const float padX = 6.0f * scale;
        const float padY = 4.0f * scale;
        const float lineGap = fontSize + 2.0f * padY + 4.0f * scale;

        const auto corner = static_cast<Core::OverlayCorner>(cfg.State.overlayCorner.load());
        const bool right = corner == Core::OverlayCorner::TopRight || corner == Core::OverlayCorner::BottomRight;
        const bool bottom = corner == Core::OverlayCorner::BottomLeft || corner == Core::OverlayCorner::BottomRight;

        for (size_t i = 0; i < lines.size(); ++i)
        {
            const char* text = lines[i].c_str();
            ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);

            float x = right ? io.DisplaySize.x - margin - padX - textSize.x : margin + padX;
            float row = static_cast<float>(bottom ? (lines.size() - 1 - i) : i);
            float y = bottom ? io.DisplaySize.y - margin - padY - textSize.y - row * lineGap
                             : margin + padY + row * lineGap;

            // Dark background pill with bright green text
            drawList->AddRectFilled(ImVec2(x - padX, y - padY), ImVec2(x + textSize.x + padX, y + textSize.y + padY),
                IM_COL32(0, 0, 0, 180), 4.0f * scale);
            drawList->AddText(font, fontSize, ImVec2(x, y), IM_COL32(50, 255, 120, 255), text);
        }
    }
}
