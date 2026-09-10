#pragma once
#include <Windows.h>
#include <vector>
#include <cstdint>

namespace Game
{
    struct Vector3
    {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };

    class GameState
    {
    public:
        static GameState& Get();

        void Update();

        bool HasPlayer() const        { return Addrs.playerX != 0; }
        bool IsLoading() const        { return m_IsLoading; }
        bool IsInMenu() const         { return m_InMenu != 0; }

        // Player Info
        Vector3 GetPlayerPos() const      { return m_PlayerPos; }
        float   GetPlayerVelocity() const { return m_PlayerVelocity; }
        int     GetPlayerState() const    { return m_PlayerState; }
        float   GetLastGroundY() const    { return m_LastGroundY; }

        // Camera Forward & Orthogonal Vectors
        Vector3 GetCameraForward() const { return m_CameraForward; }
        Vector3 GetCameraLeft() const    { return { m_CameraForward.z, 0.0f, -m_CameraForward.x }; }
        Vector3 GetCameraRight() const   { return { -m_CameraForward.z, 0.0f, m_CameraForward.x }; }

        // World Info
        float GetTimeOfDay() const     { return m_TimeOfDay; }
        float GetTimeScale() const     { return m_TimeScale; }
        bool  IsDashStarted() const    { return m_DashStarted; }
        bool  IsContentActive() const  { return m_ContentActive; }

        // Resolved Cached Addresses
        struct Addresses
        {
            uintptr_t playerX = 0, playerY = 0, playerZ = 0;
            uintptr_t playerVelocity = 0, playerState = 0;
            uintptr_t immortal = 0, inputEnabled = 0, mouseEnabled = 0;
            uintptr_t lastGroundY = 0, timeOfDay = 0, contentActive = 0, timeScale = 0;
            uintptr_t dashStarted = 0, camSin = 0, camCos = 0;
            uintptr_t wallrunCount = 0, wallclimbCount = 0, wallCount = 0;
            uintptr_t onGroundStatus = 0;
        } Addrs;

    private:
        GameState() = default;

        bool m_IsLoading = true;
        int  m_InMenu = 0;

        Vector3 m_PlayerPos;
        float   m_PlayerVelocity = 0.0f;
        int     m_PlayerState = 0;
        float   m_LastGroundY = 0.0f;
        float   m_TimeOfDay = 0.0f;
        float   m_TimeScale = 1.0f;
        bool    m_DashStarted = false;
        bool    m_ContentActive = false;

        Vector3 m_CameraForward;
        DWORD   m_LastUpdateTick = 0;
    };
}
