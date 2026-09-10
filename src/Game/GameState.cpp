#include "Game/GameState.hpp"
#include "Game/GameOffsets.hpp"
#include "Core/Memory.hpp"
#include <cmath>

namespace Game
{
    GameState& GameState::Get()
    {
        static GameState instance;
        return instance;
    }

    void GameState::Update()
    {
        DWORD currentTick = GetTickCount();
        if (currentTick - m_LastUpdateTick < 33) // ~30Hz polling rate
            return;
        m_LastUpdateTick = currentTick;

      
        uintptr_t loadPtr = Core::Memory::ResolvePtrChain(Offsets::LoadingState(), { 0x4C1 });
        m_IsLoading = Core::Memory::SafeRead<bool>(loadPtr, true);


        uintptr_t pX = Core::Memory::ResolvePtrChain(Offsets::PlayerEntity(), { 0xD0, 0x128, 0x30, 0x50 });
        Addrs.playerX = pX;
        Addrs.playerY = pX ? (pX + 0x4) : 0;
        Addrs.playerZ = pX ? (pX + 0x8) : 0;
        Addrs.playerVelocity = Core::Memory::ResolvePtrChain(Offsets::PlayerVelocity(), { 0x2378, 0x10, 0x438 });
        Addrs.playerState = Core::Memory::ResolvePtrChain(Offsets::PlayerEntity(), { 0xB0, 0x48, 0x0, 0x28, 0x90 });
        Addrs.immortal = Core::Memory::ResolvePtrChain(Offsets::ImmortalBase(), { 0x70, 0xC0, 0x0, 0x0 });
        Addrs.inputEnabled = Core::Memory::ResolvePtrChain(Offsets::InputBase(), { 0x8D4 });
        Addrs.mouseEnabled = Core::Memory::ResolvePtrChain(Offsets::InputBase(), { 0x8D8 });
        Addrs.lastGroundY = Core::Memory::ResolvePtrChain(Offsets::GroundStatusBase(), { 0x20, 0x20, 0x40, 0x20, 0x4 });
        Addrs.wallrunCount = Core::Memory::ResolvePtrChain(Offsets::PlayerEntity(), { 0xB0, 0x48, 0x0, 0x28, 0x3C });
        Addrs.wallclimbCount = Core::Memory::ResolvePtrChain(Offsets::PlayerEntity(), { 0xB0, 0x48, 0x0, 0x28, 0x38 });
        Addrs.wallCount = Core::Memory::ResolvePtrChain(Offsets::PlayerEntity(), { 0xB0, 0x48, 0x0, 0x28, 0x18 });
        Addrs.timeOfDay = Core::Memory::ResolvePtrChain(Offsets::TimeOfDayBase(), { 0x8, 0x28, 0x30 });
        Addrs.timeScale = Core::Memory::ResolvePtrChain(Offsets::EngineSettings(), { 0x48 });
        Addrs.contentActive = Core::Memory::ResolvePtrChain(Offsets::ContentActiveBase(), { 0x28, 0x280 });
        Addrs.dashStarted = Core::Memory::ResolvePtrChain(Offsets::DashStartedBase(), { 0xE0 });
        Addrs.camSin = Core::Memory::ResolvePtrChain(Offsets::CameraAnglesBase(), { 0x70, 0x98, 0x238, 0x18, 0x22C4 });
        Addrs.camCos = Core::Memory::ResolvePtrChain(Offsets::CameraAnglesBase(), { 0x70, 0x98, 0x238, 0x18, 0x22CC });
        Addrs.onGroundStatus = Core::Memory::ResolvePtrChain(Offsets::GroundStatusBase(), { 0x20, 0x20, 0x40, 0x20, 0x17 });


        m_PlayerPos.x = Core::Memory::SafeRead<float>(Addrs.playerX);
        m_PlayerPos.y = Core::Memory::SafeRead<float>(Addrs.playerY);
        m_PlayerPos.z = Core::Memory::SafeRead<float>(Addrs.playerZ);
        m_PlayerVelocity = Core::Memory::SafeRead<float>(Addrs.playerVelocity);
        m_PlayerState = Core::Memory::SafeRead<int>(Addrs.playerState);
        m_LastGroundY = Core::Memory::SafeRead<float>(Addrs.lastGroundY);
        m_TimeOfDay = Core::Memory::SafeRead<float>(Addrs.timeOfDay);
        m_TimeScale = Core::Memory::SafeRead<float>(Addrs.timeScale);
        m_ContentActive = Core::Memory::SafeRead<int>(Addrs.contentActive) == 1;
        m_DashStarted = Core::Memory::SafeRead<bool>(Addrs.dashStarted);
        m_InMenu = Core::Memory::SafeRead<int>(Offsets::InMenuAddress());

        uintptr_t camFwdX = Core::Memory::ResolvePtrChain(Offsets::CameraMatrixBase(), { 0x68, 0x568, 0x14a0, 0x250, 0x70 });
        if (camFwdX)
        {
            m_CameraForward.x = Core::Memory::SafeRead<float>(camFwdX);
            m_CameraForward.y = Core::Memory::SafeRead<float>(camFwdX + 0x4);
            m_CameraForward.z = Core::Memory::SafeRead<float>(camFwdX + 0x8);
        }
    }
}
