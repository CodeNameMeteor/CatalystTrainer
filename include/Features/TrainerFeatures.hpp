#pragma once
#include "Game/GameState.hpp"
#include "Core/Config.hpp"
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Features
{
    // All game-memory changes made by the cheats happen on the trainer's logic thread (Tick). Hotkeys and
    // menu controls only queue requests.
    class TrainerFeatures
    {
    public:
        static TrainerFeatures& Get();

        // Logic thread. dt is the time since the previous tick in seconds.
        void Tick(float dt);
        void Shutdown();

        // Game window thread
        void HandleInput(int keyCode);

        // Render thread (menu)
        void RequestTimeOfDay(float hours);
        void RequestTimeScale();
        void RequestVisuals();

        // Short status message for the on-screen overlay; empty when there is nothing to show
        std::string GetToast() const;

    private:
        TrainerFeatures() = default;

        enum Action : uint32_t
        {
            ActionToggleGod       = 1u << 0,
            ActionToggleNoclip    = 1u << 1,
            ActionToggleNoStumble = 1u << 2,
            ActionSaveTeleport    = 1u << 3,
            ActionGotoTeleport    = 1u << 4,
            ActionArmBhop         = 1u << 5,
        };

        // Saved copy of some fields of the running/friction tuning block
        struct MovementSnapshot
        {
            bool valid = false;
            std::vector<std::pair<uintptr_t, float>> values;
        };

        void ProcessActions();
        void ProcessWorld();
        void ProcessGodMode();
        void ProcessNoclip(float dt);
        void ProcessNoStumble();
        void ProcessBhop();
        void ProcessWallMovement();
        void ProcessFastLoads();

        void SaveTeleportPosition();
        void TeleportToSaved();
        void ArmBhop();
        void FinishBhop(bool applyBoost);
        void RestoreFastLoads();
        void SyncVisualsFromGame();

        static MovementSnapshot CaptureMovement(std::initializer_list<unsigned int> fields);
        static void RestoreMovement(const MovementSnapshot& snapshot);

        void ShowToast(const std::string& message);

        // Requests from other threads
        std::atomic<uint32_t> m_PendingActions{ 0 };
        std::atomic<bool> m_TimeOfDayRequested{ false };
        std::atomic<float> m_RequestedHours{ 0.0f };
        std::atomic<bool> m_TimeScaleRequested{ false };
        std::atomic<bool> m_VisualsRequested{ false };

        // Teleport state
        bool m_TeleportSaved = false;
        Game::Vector3 m_SavedPos;
        float m_SavedCamSin = 0.0f;
        float m_SavedCamCos = 0.0f;

        // Noclip state
        bool m_NoclipActive = false;
        Game::Vector3 m_NoclipPos;

        // God mode state
        bool m_GodApplied = false;

        // Bhop state
        bool m_BhopTriggered = false;
        float m_BhopSavedVel = 0.0f;
        DWORD m_BhopArmTime = 0;
        DWORD m_BhopTimer = 0;
        MovementSnapshot m_BhopSavedMovement;

        // Fast loads state
        bool m_FastLoadsApplied = false;
        float m_OriginalSimRate = 0.0f;
        float m_OriginalLoadTimeScale = 1.0f;

        // Time scale state
        bool m_TimeScaleChanged = false;
        float m_OriginalTimeScale = 1.0f;

        // Visual flags state
        bool m_VisualsSynced = false;
        uint8_t m_OriginalBloom = 0, m_OriginalBlur = 0, m_OriginalVignette = 0;

        // Overlay message
        mutable std::mutex m_ToastMutex;
        std::string m_Toast;
        DWORD m_ToastTime = 0;
    };
}
