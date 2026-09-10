#pragma once
#include "Game/GameState.hpp"
#include "Core/Config.hpp"

namespace Features
{
    class TrainerFeatures
    {
    public:
        static TrainerFeatures& Get();

        void Tick();
        void HandleInput(int keyCode);

        void SaveTeleportPosition();
        void TeleportToSaved();

    private:
        TrainerFeatures() = default;

        void ProcessGodMode();
        void ProcessNoclip();
        void ProcessNoStumble();
        void ProcessBhop();
        void ProcessWallMovement();
        void ProcessFastLoads();

        // Teleport state
        bool m_TeleportSaved = false;
        Game::Vector3 m_SavedPos;
        Game::Vector3 m_SavedCamPos;
        float m_SavedVelocity = 0.0f;
        float m_SavedCamSin = 0.0f;
        float m_SavedCamCos = 0.0f;

        // Noclip state
        bool m_NoclipActive = false;
        Game::Vector3 m_NoclipPos;

        // Bhop state
        bool m_BhopTriggered = false;
        float m_BhopSavedVel = 0.0f;
        DWORD m_BhopTimer = 0;
    };
}
