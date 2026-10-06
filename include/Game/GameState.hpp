#pragma once
#include <Windows.h>
#include <cstdint>
#include <mutex>

namespace Game
{
    struct Vector3
    {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };

    // Resolved addresses, re-resolved on every update
    struct Addresses
    {
        uintptr_t playerX = 0, playerY = 0, playerZ = 0;
        uintptr_t playerVelocity = 0, playerState = 0;
        uintptr_t immortal = 0, inputEnabled = 0, mouseEnabled = 0;
        uintptr_t lastGroundY = 0, timeOfDay = 0, contentActive = 0, timeScale = 0;
        uintptr_t dashStarted = 0, camSin = 0, camCos = 0, camFwdX = 0;
        uintptr_t wallrunCount = 0, wallclimbCount = 0, wallCount = 0;
        uintptr_t onGroundStatus = 0;
    };

    // Written by the logic thread, read by the render and window threads. All access goes through a mutex
    // and returns copies, so readers never see a half-updated state.
    class GameState
    {
    public:
        static GameState& Get();

        void Update();

        bool HasPlayer() const;
        bool IsLoading() const;
        bool IsLoadingKnown() const;    // false when the loading flag could not be read
        bool IsInMenu() const;

        // Player Info
        Vector3 GetPlayerPos() const;
        float   GetPlayerVelocity() const;
        int     GetPlayerState() const;
        float   GetLastGroundY() const;

        // Camera Forward & Orthogonal Vectors
        Vector3 GetCameraForward() const;
        Vector3 GetCameraLeft() const;
        Vector3 GetCameraRight() const;

        // World Info
        float GetTimeOfDay() const;
        float GetTimeScale() const;
        bool  IsDashStarted() const;
        bool  IsContentActive() const;

        Addresses GetAddrs() const;

    private:
        GameState() = default;

        struct Snapshot
        {
            bool isLoading = true;
            bool loadingKnown = false;
            int  inMenu = 0;

            Vector3 playerPos;
            float   playerVelocity = 0.0f;
            int     playerState = 0;
            float   lastGroundY = 0.0f;
            float   timeOfDay = 0.0f;
            float   timeScale = 1.0f;
            bool    dashStarted = false;
            bool    contentActive = false;

            Vector3 cameraForward;
            Addresses addrs;
        };

        mutable std::mutex m_Mutex;
        Snapshot m_State;
    };
}
