#include "Features/TrainerFeatures.hpp"
#include "Features/MovementProperties.hpp"
#include "Game/GameOffsets.hpp"
#include "Core/Memory.hpp"
#include <cmath>

namespace Features
{
    TrainerFeatures& TrainerFeatures::Get()
    {
        static TrainerFeatures instance;
        return instance;
    }

    void TrainerFeatures::HandleInput(int keyCode)
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        // Toggle Menu - always accessible
        if (keyCode == cfg.Keys.toggleMenu)
        {
            cfg.State.isMenuOpen = !cfg.State.isMenuOpen;
            if (cfg.State.isMenuOpen)
            {
                ClipCursor(nullptr);
                Core::Memory::SafeWrite<int>(game.Addrs.inputEnabled, 1);
                Core::Memory::SafeWrite<int>(game.Addrs.mouseEnabled, 1);
            }
            else if (game.IsInMenu() == 0)
            {
                Core::Memory::SafeWrite<int>(game.Addrs.inputEnabled, 0);
                Core::Memory::SafeWrite<int>(game.Addrs.mouseEnabled, 0);
            }
            return;
        }

        // Cheats Hotkeys (active when menu is closed)
        if (!cfg.State.isMenuOpen)
        {
            if (keyCode == cfg.Keys.godMode)
            {
                cfg.State.godMode = !cfg.State.godMode;
                Core::Memory::SafeWrite<int>(game.Addrs.immortal, cfg.State.godMode ? 1 : 0);
            }
            else if (keyCode == cfg.Keys.noclip)
            {
                cfg.State.noclip = !cfg.State.noclip;
                cfg.State.godMode = cfg.State.noclip;
                Core::Memory::SafeWrite<int>(game.Addrs.immortal, cfg.State.godMode ? 1 : 0);
            }
            else if (keyCode == cfg.Keys.noStumble)
            {
                cfg.State.noStumble = !cfg.State.noStumble;
            }
            else if (keyCode == cfg.Keys.setTeleport)
            {
                SaveTeleportPosition();
            }
            else if (keyCode == cfg.Keys.gotoTeleport)
            {
                m_NeedsTeleport = true;
            }
            else if (keyCode == cfg.Keys.bhop && !cfg.State.bhop && !game.IsContentActive())
            {
                cfg.State.bhop = true;
                m_BhopTriggered = false;
                m_BhopSavedVel = game.GetPlayerVelocity() + 0.1f;

                uintptr_t fBase = Game::Offsets::BhopFactorBase();
                uintptr_t jBase = Game::Offsets::JumpTriggerBase();
                uintptr_t jumpTriggerAddr = Core::Memory::ResolvePtrChain(jBase, { 0x1bb0 });

                // Zero out jump trigger
                Core::Memory::SafeWrite<int>(jumpTriggerAddr, 0);

                // Zero out turning decel, max speed decel and frictions
                Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C54 }), 0.0f);
                Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C4C }), 0.0f);
                Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C50 }), 1000.0f);
                Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C40 }), 0.0f);
                Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C44 }), 0.0f);
                Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C48 }), 0.0f);
            }
        }
    }

    void TrainerFeatures::SaveTeleportPosition()
    {
        auto& game = Game::GameState::Get();
        if (!game.HasPlayer()) return;

        m_SavedPos = game.GetPlayerPos();
        m_SavedVelocity = game.GetPlayerVelocity();
        m_SavedCamSin = Core::Memory::SafeRead<float>(game.Addrs.camSin);
        m_SavedCamCos = Core::Memory::SafeRead<float>(game.Addrs.camCos);

        float s = m_SavedCamSin < 0 ? -m_SavedCamSin : m_SavedCamSin;
        float c = m_SavedCamSin < 0 ? -m_SavedCamCos : m_SavedCamCos;
        m_SavedCamPos = { c * c - s * s, 0.0f, -2.0f * s * c };

        m_TeleportSaved = true;
    }

void TrainerFeatures::TeleportToSaved()
{
    auto& game = Game::GameState::Get();
    uintptr_t fBase = Game::Offsets::BhopFactorBase();
    if (!m_TeleportSaved || !game.HasPlayer()) return;



    // Spike physics thresholds to avoid rubberbanding
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C5C }), 0.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C54 }), 0.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C50 }), 0.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C4C }), 0.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C40 }), 0.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C44 }), 0.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C48 }), 0.0f);

    // Prevent fall damage
    Core::Memory::SafeWrite<float>(game.Addrs.lastGroundY, -2000.0f);

    // Write new camera orientation and position directly into the transform block
    Core::Memory::SafeWrite<float>(game.Addrs.camSin, m_SavedCamSin);
    Core::Memory::SafeWrite<float>(game.Addrs.camCos, m_SavedCamCos);
    Core::Memory::SafeWrite<float>(game.Addrs.playerX, m_SavedPos.x);
    Core::Memory::SafeWrite<float>(game.Addrs.playerY, m_SavedPos.y -3.0f);
    Core::Memory::SafeWrite<float>(game.Addrs.playerZ, m_SavedPos.z);

        // Write new camera orientation and position directly into the transform block
    Core::Memory::SafeWrite<float>(game.Addrs.camSin, m_SavedCamSin);
    Core::Memory::SafeWrite<float>(game.Addrs.camCos, m_SavedCamCos);
    Core::Memory::SafeWrite<float>(game.Addrs.playerX, m_SavedPos.x);
    Core::Memory::SafeWrite<float>(game.Addrs.playerY, m_SavedPos.y);
    Core::Memory::SafeWrite<float>(game.Addrs.playerZ, m_SavedPos.z);

        // Write new camera orientation and position directly into the transform block
    Core::Memory::SafeWrite<float>(game.Addrs.camSin, m_SavedCamSin);
    Core::Memory::SafeWrite<float>(game.Addrs.camCos, m_SavedCamCos);
    Core::Memory::SafeWrite<float>(game.Addrs.playerX, m_SavedPos.x);
    Core::Memory::SafeWrite<float>(game.Addrs.playerY, m_SavedPos.y);
    Core::Memory::SafeWrite<float>(game.Addrs.playerZ, m_SavedPos.z);

    Sleep(50);

    // Restore physics thresholds
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C5C }), 7.2f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C54 }), 20.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C50 }), 7.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C4C }), 10.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C40 }), 3.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C44 }), 6.0f);
    Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C48 }), 5.0f);
}

    void TrainerFeatures::Shutdown()
    {
        auto& game = Game::GameState::Get();

        // Restore Input
        Core::Memory::SafeWrite<int>(game.Addrs.inputEnabled, 0);
        Core::Memory::SafeWrite<int>(game.Addrs.mouseEnabled, 0);

        // Restore God Mode
        Core::Memory::SafeWrite<int>(game.Addrs.immortal, 0);

        // Restore Noclip state
        if (m_NoclipActive && game.HasPlayer())
        {
            Core::Memory::SafeWrite<int>(game.Addrs.playerState, 2);
            Core::Memory::SafeWrite<float>(game.Addrs.playerVelocity, 0.0f);
        }

        // Restore Time Scale
        uintptr_t engine = Game::Offsets::EngineSettings();
        Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(engine, { 0x48 }), 1.0f);

        // Reset all custom movement property sliders to vanilla
        Features::MovementManager::Get().ResetAll();
    }

    void TrainerFeatures::Tick()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        if (m_NeedsTeleport)
        {
            TeleportToSaved();
            m_NeedsTeleport = false;
        }

        if (cfg.State.freezeTime)
        {
            Core::Memory::SafeWrite<float>(game.Addrs.timeOfDay, cfg.State.frozenTimeValue);
        }

        //ProcessGodMode();
        ProcessNoclip();
        ProcessNoStumble();
        ProcessBhop();
        ProcessWallMovement();
        ProcessFastLoads();
    }

    void TrainerFeatures::ProcessGodMode()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();
        bool godMode = false;

        if (cfg.State.godMode && !godMode)
        {
            Core::Memory::SafeWrite<int>(game.Addrs.immortal, 1);
            godMode = true;
        }
        else if (!cfg.State.godMode && godMode)
        {
            Core::Memory::SafeWrite<int>(game.Addrs.immortal, 0);
            godMode = false;
        }
    }

    void TrainerFeatures::ProcessNoclip()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        if (!cfg.State.noclip || !game.HasPlayer())
        {
            if (m_NoclipActive)
            {
                m_NoclipActive = false;
                if (game.HasPlayer())
                {
                    Core::Memory::SafeWrite<int>(game.Addrs.playerState, 2);
                    //Core::Memory::SafeWrite<float>(game.Addrs.lastGroundY, game.GetPlayerPos().y);
                    Core::Memory::SafeWrite<float>(game.Addrs.playerVelocity, 0.0f);
                }
            }
            return;
        }

        if (!m_NoclipActive)
        {
            m_NoclipPos = game.GetPlayerPos();
            m_NoclipActive = true;
        }


        if (!cfg.State.isMenuOpen)
        {
            if (GetAsyncKeyState(cfg.Keys.incNoclipSpeed) & 0x8000)
                cfg.State.noclipSpeed = (std::min)(cfg.State.noclipSpeed + 0.05f, 10.0f);
            if (GetAsyncKeyState(cfg.Keys.decNoclipSpeed) & 0x8000)
                cfg.State.noclipSpeed = (std::max)(cfg.State.noclipSpeed - 0.05f, 0.1f);
        }

        // Direction Vectors from Camera Look
        Game::Vector3 forward = game.GetCameraForward();
        Game::Vector3 left = game.GetCameraLeft();
        Game::Vector3 right = game.GetCameraRight();
        Game::Vector3 delta = { 0.0f, 0.0f, 0.0f };

        float speedMultiplier = 1.0f;
        if (!cfg.State.isMenuOpen)
        {
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
                if(speedMultiplier == 1.0f)
                    speedMultiplier = 1.5f;
                else speedMultiplier = 1.0f;
            if (GetAsyncKeyState(VK_MENU) & 0x8000)
                if(speedMultiplier == 0.3f)
                    speedMultiplier = 1.0f;
                else speedMultiplier = 0.3f;

            if (GetAsyncKeyState('W') & 0x8000) { delta.x += forward.x; delta.y += forward.y; delta.z += forward.z; }
            if (GetAsyncKeyState('S') & 0x8000) { delta.x -= forward.x; delta.y -= forward.y; delta.z -= forward.z; }
            if (GetAsyncKeyState('A') & 0x8000) { delta.x += left.x;    delta.y += left.y;    delta.z += left.z; }
            if (GetAsyncKeyState('D') & 0x8000) { delta.x += right.x;   delta.y += right.y;   delta.z += right.z; }
            if (GetAsyncKeyState(VK_SPACE) & 0x8000)   delta.y += 0.6f;
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000 || GetAsyncKeyState('C') & 0x8000) delta.y -= 0.6f;
        }

        float currentSpeed = cfg.State.noclipSpeed * speedMultiplier;
        m_NoclipPos.x += delta.x * currentSpeed;
        m_NoclipPos.y += delta.y * currentSpeed;
        m_NoclipPos.z += delta.z * currentSpeed;


        Core::Memory::SafeWrite<float>(game.Addrs.playerX, m_NoclipPos.x);
        Core::Memory::SafeWrite<float>(game.Addrs.playerY, m_NoclipPos.y);
        Core::Memory::SafeWrite<float>(game.Addrs.playerZ, m_NoclipPos.z);

        Core::Memory::SafeWrite<float>(game.Addrs.playerVelocity, 0.0f);
        Core::Memory::SafeWrite<float>(game.Addrs.lastGroundY, -2000.0f);

        Core::Memory::SafeWrite<int>(game.Addrs.playerState, 2);


        Core::Memory::SafeWrite<int>(game.Addrs.wallclimbCount, 0);
        Core::Memory::SafeWrite<int>(game.Addrs.wallrunCount, 0);
        Core::Memory::SafeWrite<int>(game.Addrs.wallCount, 0);
    }

    void TrainerFeatures::ProcessNoStumble()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        if (cfg.State.noStumble && !cfg.State.noclip)
        {
            int state = game.GetPlayerState();
            if ((game.GetLastGroundY() - game.GetPlayerPos().y >= 1.0f) && state != 3 && state != 4)
            {
                Core::Memory::SafeWrite<float>(game.Addrs.lastGroundY, game.GetPlayerPos().y);
            }
        }
    }

    void TrainerFeatures::ProcessBhop()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        if (!cfg.State.bhop || !game.HasPlayer()) return;

        bool onGround = Core::Memory::SafeRead<bool>(game.Addrs.onGroundStatus, true);
        uintptr_t jBase = Game::Offsets::JumpTriggerBase();
        uintptr_t fBase = Game::Offsets::BhopFactorBase();
        uintptr_t jumpTriggerAddr = Core::Memory::ResolvePtrChain(jBase, { 0x1bb0 });
        //uintptr_t jumpTriggerAddr = Core::Memory::ResolvePtrChain(jBase, { 0x1b98 }); F bind

        if (!onGround && !m_BhopTriggered)
        {
            Core::Memory::SafeWrite<float>(game.Addrs.playerVelocity, m_BhopSavedVel);
            Core::Memory::SafeWrite<int>(game.Addrs.playerState, 8);
            Core::Memory::SafeWrite<int>(jumpTriggerAddr, 1);
            m_BhopTriggered = true;
            m_BhopTimer = GetTickCount();
        }

        if (m_BhopTriggered && (GetTickCount() - m_BhopTimer > 50))
        {
            cfg.State.bhop = false;
            m_BhopTriggered = false;

            // Reset jump trigger
            Core::Memory::SafeWrite<int>(jumpTriggerAddr, 0);

            // Restore player velocity with boost
            Core::Memory::SafeWrite<float>(game.Addrs.playerVelocity, m_BhopSavedVel + 0.1f);

            // Restore normal physics friction
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C54 }), 20.0f);
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C4C }), 10.0f);
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C50 }), 7.0f);
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C40 }), 4.0f);
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C44 }), 6.0f);
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(fBase, { 0x408, 0x380, 0x1C48 }), 4.0f);
        }
    }

    void TrainerFeatures::ProcessWallMovement()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        if (cfg.State.infiniteWallclimbs)
        {
            if (Core::Memory::SafeRead<int>(game.Addrs.wallclimbCount) != 0)
            {
                if (Core::Memory::SafeRead<int>(game.Addrs.wallCount) > 2)
                    Core::Memory::SafeWrite<int>(game.Addrs.wallCount, 2);
                Core::Memory::SafeWrite<int>(game.Addrs.wallclimbCount, 0);
            }
        }

        if (cfg.State.infiniteWallruns)
        {
            if (Core::Memory::SafeRead<int>(game.Addrs.wallrunCount) != 0)
            {
                if (Core::Memory::SafeRead<int>(game.Addrs.wallCount) > 2)
                    Core::Memory::SafeWrite<int>(game.Addrs.wallCount, 2);
                Core::Memory::SafeWrite<int>(game.Addrs.wallrunCount, 0);
            }
        }

        if (cfg.State.sameWall)
        {
            if (Core::Memory::SafeRead<int>(game.Addrs.wallCount) != 0)
                Core::Memory::SafeWrite<int>(game.Addrs.wallCount, 0);
        }
    }

    void TrainerFeatures::ProcessFastLoads()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();
        bool fastLoads = false;
        uintptr_t engine = Game::Offsets::EngineSettings();
                
        if (cfg.State.fastLoads && !fastLoads)
        {
            fastLoads = true;
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(engine, { 0x2c }), 50000.0f);
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(engine, { 0x48 }), 3000.0f);
        }
        else if (!cfg.State.fastLoads && fastLoads)
        {
            fastLoads = false;
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(engine, { 0x2c }), 30.0f);
            Core::Memory::SafeWrite<float>(Core::Memory::ResolvePtrChain(engine, { 0x48 }), cfg.State.timeScale);
        }

    }
}
