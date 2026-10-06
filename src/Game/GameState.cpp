#include "Game/GameState.hpp"
#include "Game/GameOffsets.hpp"
#include "Core/Memory.hpp"

namespace Game
{
    using Core::Memory;

    GameState& GameState::Get()
    {
        static GameState instance;
        return instance;
    }

    void GameState::Update()
    {
        Snapshot s;
        Addresses& a = s.addrs;

        uintptr_t loadPtr = Memory::ResolvePtrChain(Offsets::LoadingState(), { 0x4C1 });
        s.loadingKnown = loadPtr != 0;
        s.isLoading = Memory::SafeRead<bool>(loadPtr, true);

        uintptr_t pX = Memory::ResolvePtrChain(Offsets::PlayerEntity(), { 0xD0, 0x128, 0x30, 0x50 });
        a.playerX = pX;
        a.playerY = pX ? (pX + 0x4) : 0;
        a.playerZ = pX ? (pX + 0x8) : 0;
        a.playerVelocity = Memory::ResolvePtrChain(Offsets::PlayerVelocity(), { 0x2378, 0x10, 0x438 });
        a.playerState = Memory::ResolvePtrChain(Offsets::PlayerEntity(), { 0xB0, 0x48, 0x0, 0x28, 0x90 });
        a.immortal = Memory::ResolvePtrChain(Offsets::ImmortalBase(), { 0x70, 0xC0, 0x0, 0x0 });
        a.inputEnabled = Memory::ResolvePtrChain(Offsets::InputBase(), { 0x8D4 });
        a.mouseEnabled = Memory::ResolvePtrChain(Offsets::InputBase(), { 0x8D8 });
        a.lastGroundY = Memory::ResolvePtrChain(Offsets::GroundStatusBase(), { 0x20, 0x20, 0x40, 0x20, 0x4 });
        a.wallrunCount = Memory::ResolvePtrChain(Offsets::PlayerEntity(), { 0xB0, 0x48, 0x0, 0x28, 0x3C });
        a.wallclimbCount = Memory::ResolvePtrChain(Offsets::PlayerEntity(), { 0xB0, 0x48, 0x0, 0x28, 0x38 });
        a.wallCount = Memory::ResolvePtrChain(Offsets::PlayerEntity(), { 0xB0, 0x48, 0x0, 0x28, 0x18 });
        a.timeOfDay = Memory::ResolvePtrChain(Offsets::TimeOfDayBase(), { 0x8, 0x28, 0x30 });
        a.timeScale = Offsets::EngineField(Offsets::Engine::TimeScale);
        a.contentActive = Memory::ResolvePtrChain(Offsets::ContentActiveBase(), { 0x28, 0x280 });
        a.dashStarted = Memory::ResolvePtrChain(Offsets::DashStartedBase(), { 0xE0 });
        a.camSin = Memory::ResolvePtrChain(Offsets::CameraAnglesBase(), { 0x70, 0x98, 0x238, 0x18, 0x22C4 });
        a.camCos = Memory::ResolvePtrChain(Offsets::CameraAnglesBase(), { 0x70, 0x98, 0x238, 0x18, 0x22CC });
        a.onGroundStatus = Memory::ResolvePtrChain(Offsets::GroundStatusBase(), { 0x20, 0x20, 0x40, 0x20, 0x17 });
        a.camFwdX = Memory::ResolvePtrChain(Offsets::CameraMatrixBase(), { 0x68, 0x568, 0x14a0, 0x250, 0x70 });

        s.playerPos.x = Memory::SafeRead<float>(a.playerX);
        s.playerPos.y = Memory::SafeRead<float>(a.playerY);
        s.playerPos.z = Memory::SafeRead<float>(a.playerZ);
        s.playerVelocity = Memory::SafeRead<float>(a.playerVelocity);
        s.playerState = Memory::SafeRead<int>(a.playerState);
        s.lastGroundY = Memory::SafeRead<float>(a.lastGroundY);
        s.timeOfDay = Memory::SafeRead<float>(a.timeOfDay);
        s.timeScale = Memory::SafeRead<float>(a.timeScale, 1.0f);
        s.contentActive = Memory::SafeRead<int>(a.contentActive) == 1;
        s.dashStarted = Memory::SafeRead<bool>(a.dashStarted);
        s.inMenu = Memory::SafeRead<int>(Offsets::InMenuAddress());

        if (a.camFwdX)
        {
            s.cameraForward.x = Memory::SafeRead<float>(a.camFwdX);
            s.cameraForward.y = Memory::SafeRead<float>(a.camFwdX + 0x4);
            s.cameraForward.z = Memory::SafeRead<float>(a.camFwdX + 0x8);
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            s.cameraForward = m_State.cameraForward;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State = s;
    }

    bool GameState::HasPlayer() const        { std::lock_guard<std::mutex> l(m_Mutex); return m_State.addrs.playerX != 0; }
    bool GameState::IsLoading() const        { std::lock_guard<std::mutex> l(m_Mutex); return m_State.isLoading; }
    bool GameState::IsLoadingKnown() const   { std::lock_guard<std::mutex> l(m_Mutex); return m_State.loadingKnown; }
    bool GameState::IsInMenu() const         { std::lock_guard<std::mutex> l(m_Mutex); return m_State.inMenu != 0; }

    Vector3 GameState::GetPlayerPos() const      { std::lock_guard<std::mutex> l(m_Mutex); return m_State.playerPos; }
    float   GameState::GetPlayerVelocity() const { std::lock_guard<std::mutex> l(m_Mutex); return m_State.playerVelocity; }
    int     GameState::GetPlayerState() const    { std::lock_guard<std::mutex> l(m_Mutex); return m_State.playerState; }
    float   GameState::GetLastGroundY() const    { std::lock_guard<std::mutex> l(m_Mutex); return m_State.lastGroundY; }

    Vector3 GameState::GetCameraForward() const { std::lock_guard<std::mutex> l(m_Mutex); return m_State.cameraForward; }

    Vector3 GameState::GetCameraLeft() const
    {
        Vector3 f = GetCameraForward();
        return { f.z, 0.0f, -f.x };
    }

    Vector3 GameState::GetCameraRight() const
    {
        Vector3 f = GetCameraForward();
        return { -f.z, 0.0f, f.x };
    }

    float GameState::GetTimeOfDay() const   { std::lock_guard<std::mutex> l(m_Mutex); return m_State.timeOfDay; }
    float GameState::GetTimeScale() const   { std::lock_guard<std::mutex> l(m_Mutex); return m_State.timeScale; }
    bool  GameState::IsDashStarted() const  { std::lock_guard<std::mutex> l(m_Mutex); return m_State.dashStarted; }
    bool  GameState::IsContentActive() const { std::lock_guard<std::mutex> l(m_Mutex); return m_State.contentActive; }

    Addresses GameState::GetAddrs() const   { std::lock_guard<std::mutex> l(m_Mutex); return m_State.addrs; }
}
