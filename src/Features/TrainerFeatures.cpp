#include "Features/TrainerFeatures.hpp"
#include "Features/MovementProperties.hpp"
#include "Game/GameOffsets.hpp"
#include "Core/Hooks.hpp"
#include "Core/Memory.hpp"
#include <algorithm>
#include <cmath>

namespace Features
{
    using Core::Memory;
    namespace Mv = Game::Offsets::Movement;

    // Noclip speeds were tuned for a 60 Hz update; movement is now scaled by real elapsed time
    constexpr float kReferenceTickRate = 60.0f;
    constexpr float kNoclipSpeedChangePerSecond = 0.05f * kReferenceTickRate;
    constexpr DWORD kBhopArmTimeoutMs = 1500;
    constexpr DWORD kToastDurationMs = 2000;

    TrainerFeatures& TrainerFeatures::Get()
    {
        static TrainerFeatures instance;
        return instance;
    }

    // ---------------------------------------------------------------------------------------------
    // Requests from other threads
    // ---------------------------------------------------------------------------------------------

    void TrainerFeatures::HandleInput(int keyCode)
    {
        auto& cfg = Core::ConfigManager::Get();

        // Toggle Menu - always accessible. The menu applies the matching game input changes when it renders.
        if (keyCode == cfg.Keys.toggleMenu)
        {
            cfg.State.isMenuOpen = !cfg.State.isMenuOpen;
            return;
        }

        // Cheats Hotkeys (active when menu is closed)
        if (cfg.State.isMenuOpen)
            return;

        uint32_t action = 0;
        if (keyCode == cfg.Keys.godMode)           action = ActionToggleGod;
        else if (keyCode == cfg.Keys.noclip)       action = ActionToggleNoclip;
        else if (keyCode == cfg.Keys.noStumble)    action = ActionToggleNoStumble;
        else if (keyCode == cfg.Keys.setTeleport)  action = ActionSaveTeleport;
        else if (keyCode == cfg.Keys.gotoTeleport) action = ActionGotoTeleport;
        else if (keyCode == cfg.Keys.bhop)         action = ActionArmBhop;

        if (action)
            m_PendingActions.fetch_or(action);
    }

    void TrainerFeatures::RequestTimeOfDay(float hours)
    {
        m_RequestedHours = hours;
        m_TimeOfDayRequested = true;
    }

    void TrainerFeatures::RequestTimeScale()
    {
        m_TimeScaleRequested = true;
    }

    void TrainerFeatures::RequestVisuals()
    {
        m_VisualsRequested = true;
    }

    void TrainerFeatures::ShowToast(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(m_ToastMutex);
        m_Toast = message;
        m_ToastTime = GetTickCount();
    }

    std::string TrainerFeatures::GetToast() const
    {
        std::lock_guard<std::mutex> lock(m_ToastMutex);
        if (m_Toast.empty() || GetTickCount() - m_ToastTime > kToastDurationMs)
            return {};
        return m_Toast;
    }

    // ---------------------------------------------------------------------------------------------
    // Logic thread
    // ---------------------------------------------------------------------------------------------

    void TrainerFeatures::Tick(float dt)
    {
        dt = (std::clamp)(dt, 0.0f, 0.1f);

        ProcessActions();
        ProcessWorld();
        ProcessGodMode();
        ProcessNoclip(dt);
        ProcessNoStumble();
        ProcessBhop();
        ProcessWallMovement();
        ProcessFastLoads();
    }

    void TrainerFeatures::ProcessActions()
    {
        uint32_t actions = m_PendingActions.exchange(0);
        if (!actions) return;

        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();
        const char* notInGame = game.HasPlayer() ? "" : " (applies once in game)";

        if (actions & ActionToggleGod)
        {
            cfg.State.godMode = !cfg.State.godMode;
            ShowToast(std::string("God Mode: ") + (cfg.State.godMode ? "ON" : "OFF") + notInGame);
        }
        if (actions & ActionToggleNoclip)
        {
            cfg.State.noclip = !cfg.State.noclip;
            ShowToast(std::string("Noclip: ") + (cfg.State.noclip ? "ON" : "OFF") + notInGame);
        }
        if (actions & ActionToggleNoStumble)
        {
            cfg.State.noStumble = !cfg.State.noStumble;
            ShowToast(std::string("No Stumble: ") + (cfg.State.noStumble ? "ON" : "OFF"));
        }
        if (actions & ActionSaveTeleport)
        {
            SaveTeleportPosition();
        }
        if (actions & ActionGotoTeleport)
        {
            TeleportToSaved();
        }
        if (actions & ActionArmBhop)
        {
            if (!cfg.State.bhop && !game.IsContentActive())
                ArmBhop();
        }
    }

    void TrainerFeatures::ProcessWorld()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto addrs = Game::GameState::Get().GetAddrs();

        if (m_TimeOfDayRequested.exchange(false) && addrs.timeOfDay)
        {
            float raw = Memory::SafeRead<float>(addrs.timeOfDay);
            float newTime = Game::SetHourOfDay(raw, m_RequestedHours);
            Memory::SafeWrite<float>(addrs.timeOfDay, newTime);
            cfg.State.frozenTimeValue = newTime;
        }

        if (cfg.State.freezeTime)
        {
            Memory::SafeWrite<float>(addrs.timeOfDay, cfg.State.frozenTimeValue);
        }

        // Left pending while Fast Loads owns the time scale, applied once the load finishes
        if (!m_FastLoadsApplied && m_TimeScaleRequested.exchange(false))
        {
            uintptr_t timeScaleAddr = Game::Offsets::EngineField(Game::Offsets::Engine::TimeScale);
            if (timeScaleAddr)
            {
                if (!m_TimeScaleChanged)
                {
                    m_OriginalTimeScale = Memory::SafeRead<float>(timeScaleAddr, 1.0f);
                    m_TimeScaleChanged = true;
                }
                Memory::SafeWrite<float>(timeScaleAddr, cfg.State.timeScale);
            }
        }

        SyncVisualsFromGame();
        if (m_VisualsRequested.exchange(false) && m_VisualsSynced)
        {
            namespace Vis = Game::Offsets::Visuals;
            Memory::SafeWrite<uint8_t>(Game::Offsets::VisualsField(Vis::Bloom), cfg.State.bloom ? 1 : 0);
            Memory::SafeWrite<uint8_t>(Game::Offsets::VisualsField(Vis::Blur), cfg.State.blur ? 1 : 0);
            Memory::SafeWrite<uint8_t>(Game::Offsets::VisualsField(Vis::Vignette), cfg.State.vignette ? 1 : 0);
        }
    }

    void TrainerFeatures::SyncVisualsFromGame()
    {
        if (m_VisualsSynced) return;

        namespace Vis = Game::Offsets::Visuals;
        uintptr_t bloom = Game::Offsets::VisualsField(Vis::Bloom);
        uintptr_t blur = Game::Offsets::VisualsField(Vis::Blur);
        uintptr_t vignette = Game::Offsets::VisualsField(Vis::Vignette);
        if (!bloom || !blur || !vignette) return;

        // These are single-byte flags; the checkboxes start from whatever the game currently uses
        m_OriginalBloom = Memory::SafeRead<uint8_t>(bloom);
        m_OriginalBlur = Memory::SafeRead<uint8_t>(blur);
        m_OriginalVignette = Memory::SafeRead<uint8_t>(vignette);

        auto& cfg = Core::ConfigManager::Get();
        cfg.State.bloom = m_OriginalBloom != 0;
        cfg.State.blur = m_OriginalBlur != 0;
        cfg.State.vignette = m_OriginalVignette != 0;
        m_VisualsSynced = true;
    }

    void TrainerFeatures::ProcessGodMode()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto addrs = Game::GameState::Get().GetAddrs();

        // Noclip implies god mode, without changing the user's God Mode setting
        bool wanted = cfg.State.godMode || cfg.State.noclip;

        if (wanted)
        {
            // Re-applied every tick so it survives respawns and level loads
            if (addrs.immortal && Memory::SafeRead<int>(addrs.immortal) != 1)
                Memory::SafeWrite<int>(addrs.immortal, 1);
            m_GodApplied = true;
        }
        else if (m_GodApplied)
        {
            Memory::SafeWrite<int>(addrs.immortal, 0);
            m_GodApplied = false;
        }
    }

    void TrainerFeatures::SaveTeleportPosition()
    {
        auto& game = Game::GameState::Get();
        if (!game.HasPlayer())
        {
            ShowToast("Teleport: not in game");
            return;
        }

        auto addrs = game.GetAddrs();
        m_SavedPos = game.GetPlayerPos();
        m_SavedCamSin = Memory::SafeRead<float>(addrs.camSin);
        m_SavedCamCos = Memory::SafeRead<float>(addrs.camCos);
        m_TeleportSaved = true;
        ShowToast("Teleport position saved");
    }

    TrainerFeatures::MovementSnapshot TrainerFeatures::CaptureMovement(std::initializer_list<unsigned int> fields)
    {
        MovementSnapshot snapshot;
        for (unsigned int field : fields)
        {
            uintptr_t addr = Game::Offsets::MovementField(field);
            if (!addr) return {};
            snapshot.values.emplace_back(addr, Memory::SafeRead<float>(addr));
        }
        snapshot.valid = true;
        return snapshot;
    }

    void TrainerFeatures::RestoreMovement(const MovementSnapshot& snapshot)
    {
        if (!snapshot.valid) return;
        for (const auto& [addr, value] : snapshot.values)
            Memory::SafeWrite<float>(addr, value);
    }

    void TrainerFeatures::TeleportToSaved()
    {
        auto& game = Game::GameState::Get();
        if (!m_TeleportSaved || !game.HasPlayer())
        {
            ShowToast(m_TeleportSaved ? "Teleport: not in game" : "Teleport: no position saved");
            return;
        }

        auto addrs = game.GetAddrs();

        // Temporarily zero the physics thresholds to avoid rubberbanding, then put the game's values back
        MovementSnapshot saved = CaptureMovement({ Mv::MaxRunSpeed, Mv::TurningDecel, Mv::DirectionChange,
            Mv::AboveMaxSpeedDecel, Mv::ForwardFriction, Mv::BackwardFriction, Mv::StrafeFriction });
        for (const auto& [addr, value] : saved.values)
            Memory::SafeWrite<float>(addr, 0.0f);

        // Prevent fall damage
        Memory::SafeWrite<float>(addrs.lastGroundY, -2000.0f);

        // Write new camera orientation and position directly into the transform block
        Memory::SafeWrite<float>(addrs.camSin, m_SavedCamSin);
        Memory::SafeWrite<float>(addrs.camCos, m_SavedCamCos);
        Memory::SafeWrite<float>(addrs.playerX, m_SavedPos.x);
        Memory::SafeWrite<float>(addrs.playerY, m_SavedPos.y);
        Memory::SafeWrite<float>(addrs.playerZ, m_SavedPos.z);

        // Keep noclip from pulling the player back to where it was flying
        if (m_NoclipActive)
            m_NoclipPos = m_SavedPos;

        Sleep(50);

        RestoreMovement(saved);
    }

    void TrainerFeatures::Shutdown()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();
        auto addrs = game.GetAddrs();

        // Give input back to the game if the trainer menu had taken it
        if (cfg.State.isMenuOpen)
        {
            cfg.State.isMenuOpen = false;
            if (!game.IsInMenu())
            {
                Memory::SafeWrite<int>(addrs.inputEnabled, 0);
                Memory::SafeWrite<int>(addrs.mouseEnabled, 0);
            }
            ClipCursor(nullptr);
        }

        if (cfg.State.bhop)
            FinishBhop(false);

        RestoreFastLoads();

        if (m_GodApplied)
        {
            Memory::SafeWrite<int>(addrs.immortal, 0);
            m_GodApplied = false;
        }

        if (m_NoclipActive && game.HasPlayer())
        {
            Memory::SafeWrite<int>(addrs.playerState, Game::PlayerStates::Neutral);
            Memory::SafeWrite<float>(addrs.playerVelocity, 0.0f);
            m_NoclipActive = false;
        }

        if (m_TimeScaleChanged)
        {
            Memory::SafeWrite<float>(Game::Offsets::EngineField(Game::Offsets::Engine::TimeScale), m_OriginalTimeScale);
            m_TimeScaleChanged = false;
        }

        if (m_VisualsSynced)
        {
            namespace Vis = Game::Offsets::Visuals;
            Memory::SafeWrite<uint8_t>(Game::Offsets::VisualsField(Vis::Bloom), m_OriginalBloom);
            Memory::SafeWrite<uint8_t>(Game::Offsets::VisualsField(Vis::Blur), m_OriginalBlur);
            Memory::SafeWrite<uint8_t>(Game::Offsets::VisualsField(Vis::Vignette), m_OriginalVignette);
        }

        cfg.State.freezeTime = false;

        // Put every movement property the trainer changed back to the game's value
        Features::MovementManager::Get().ResetAll();
    }

    void TrainerFeatures::ProcessNoclip(float dt)
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();
        auto addrs = game.GetAddrs();

        if (!cfg.State.noclip || !game.HasPlayer())
        {
            if (m_NoclipActive)
            {
                m_NoclipActive = false;
                if (game.HasPlayer())
                {
                    Memory::SafeWrite<int>(addrs.playerState, Game::PlayerStates::Neutral);
                    Memory::SafeWrite<float>(addrs.playerVelocity, 0.0f);
                }
            }
            return;
        }

        if (!m_NoclipActive)
        {
            m_NoclipPos = game.GetPlayerPos();
            m_NoclipActive = true;
        }

        // GetAsyncKeyState is global, so only read it while the game window has focus
        HWND gameWindow = Core::GetGameWindow();
        bool acceptInput = !cfg.State.isMenuOpen && gameWindow && GetForegroundWindow() == gameWindow;
        auto IsDown = [](int vk) { return vk > 0 && (GetAsyncKeyState(vk) & 0x8000) != 0; };

        Game::Vector3 delta = { 0.0f, 0.0f, 0.0f };
        float speedMultiplier = 1.0f;

        if (acceptInput)
        {
            float speed = cfg.State.noclipSpeed;
            if (IsDown(cfg.Keys.incNoclipSpeed)) speed += kNoclipSpeedChangePerSecond * dt;
            if (IsDown(cfg.Keys.decNoclipSpeed)) speed -= kNoclipSpeedChangePerSecond * dt;
            cfg.State.noclipSpeed = (std::clamp)(speed, Core::kMinNoclipSpeed, Core::kMaxNoclipSpeed);

            if (IsDown(cfg.Keys.noclipSlow))      speedMultiplier = 0.3f;
            else if (IsDown(cfg.Keys.noclipFast)) speedMultiplier = 1.5f;

            // Direction Vectors from Camera Look
            Game::Vector3 forward = game.GetCameraForward();
            Game::Vector3 left = game.GetCameraLeft();
            Game::Vector3 right = game.GetCameraRight();

            if (IsDown(cfg.Keys.noclipForward)) { delta.x += forward.x; delta.y += forward.y; delta.z += forward.z; }
            if (IsDown(cfg.Keys.noclipBack))    { delta.x -= forward.x; delta.y -= forward.y; delta.z -= forward.z; }
            if (IsDown(cfg.Keys.noclipLeft))    { delta.x += left.x;    delta.y += left.y;    delta.z += left.z; }
            if (IsDown(cfg.Keys.noclipRight))   { delta.x += right.x;   delta.y += right.y;   delta.z += right.z; }
            if (IsDown(cfg.Keys.noclipUp))      delta.y += 0.6f;
            if (IsDown(cfg.Keys.noclipDown))    delta.y -= 0.6f;
        }

        float step = cfg.State.noclipSpeed * speedMultiplier * dt * kReferenceTickRate;
        m_NoclipPos.x += delta.x * step;
        m_NoclipPos.y += delta.y * step;
        m_NoclipPos.z += delta.z * step;

        Memory::SafeWrite<float>(addrs.playerX, m_NoclipPos.x);
        Memory::SafeWrite<float>(addrs.playerY, m_NoclipPos.y);
        Memory::SafeWrite<float>(addrs.playerZ, m_NoclipPos.z);

        Memory::SafeWrite<float>(addrs.playerVelocity, 0.0f);
        Memory::SafeWrite<float>(addrs.lastGroundY, -2000.0f);

        Memory::SafeWrite<int>(addrs.playerState, Game::PlayerStates::Neutral);

        Memory::SafeWrite<int>(addrs.wallclimbCount, 0);
        Memory::SafeWrite<int>(addrs.wallrunCount, 0);
        Memory::SafeWrite<int>(addrs.wallCount, 0);
    }

    void TrainerFeatures::ProcessNoStumble()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        if (cfg.State.noStumble && !cfg.State.noclip)
        {
            int state = game.GetPlayerState();
            float playerY = game.GetPlayerPos().y;
            if ((game.GetLastGroundY() - playerY >= 1.0f) &&
                state != Game::PlayerStates::StumbleLandA && state != Game::PlayerStates::StumbleLandB)
            {
                Memory::SafeWrite<float>(game.GetAddrs().lastGroundY, playerY);
            }
        }
    }

    void TrainerFeatures::ArmBhop()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        m_BhopSavedMovement = CaptureMovement({ Mv::TurningDecel, Mv::AboveMaxSpeedDecel, Mv::DirectionChange,
            Mv::ForwardFriction, Mv::BackwardFriction, Mv::StrafeFriction });
        if (!m_BhopSavedMovement.valid || !game.HasPlayer())
        {
            ShowToast("Bhop: not in game");
            return;
        }

        cfg.State.bhop = true;
        m_BhopTriggered = false;
        m_BhopSavedVel = game.GetPlayerVelocity() + 0.1f;
        m_BhopArmTime = GetTickCount();

        // Zero out jump trigger
        Memory::SafeWrite<int>(Game::Offsets::JumpTrigger(), 0);

        // Zero out turning decel, max speed decel and frictions
        Memory::SafeWrite<float>(Game::Offsets::MovementField(Mv::TurningDecel), 0.0f);
        Memory::SafeWrite<float>(Game::Offsets::MovementField(Mv::AboveMaxSpeedDecel), 0.0f);
        Memory::SafeWrite<float>(Game::Offsets::MovementField(Mv::DirectionChange), 1000.0f);
        Memory::SafeWrite<float>(Game::Offsets::MovementField(Mv::ForwardFriction), 0.0f);
        Memory::SafeWrite<float>(Game::Offsets::MovementField(Mv::BackwardFriction), 0.0f);
        Memory::SafeWrite<float>(Game::Offsets::MovementField(Mv::StrafeFriction), 0.0f);
    }

    void TrainerFeatures::FinishBhop(bool applyBoost)
    {
        auto& cfg = Core::ConfigManager::Get();

        cfg.State.bhop = false;
        m_BhopTriggered = false;

        // Reset jump trigger
        Memory::SafeWrite<int>(Game::Offsets::JumpTrigger(), 0);

        // Restore player velocity with boost
        if (applyBoost)
            Memory::SafeWrite<float>(Game::GameState::Get().GetAddrs().playerVelocity, m_BhopSavedVel + 0.1f);

        // Restore the game's friction values
        RestoreMovement(m_BhopSavedMovement);
        m_BhopSavedMovement = {};
    }

    void TrainerFeatures::ProcessBhop()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        if (!cfg.State.bhop) return;

        if (!game.HasPlayer())
        {
            FinishBhop(false);
            return;
        }

        auto addrs = game.GetAddrs();
        bool onGround = Memory::SafeRead<bool>(addrs.onGroundStatus, true);

        if (!onGround && !m_BhopTriggered)
        {
            Memory::SafeWrite<float>(addrs.playerVelocity, m_BhopSavedVel);
            Memory::SafeWrite<int>(addrs.playerState, Game::PlayerStates::Jump);
            Memory::SafeWrite<int>(Game::Offsets::JumpTrigger(), 1);
            m_BhopTriggered = true;
            m_BhopTimer = GetTickCount();
        }

        if (m_BhopTriggered && (GetTickCount() - m_BhopTimer > 50))
        {
            FinishBhop(true);
        }
        else if (!m_BhopTriggered && (GetTickCount() - m_BhopArmTime > kBhopArmTimeoutMs))
        {
            // Player never left the ground: don't leave the zero-friction physics active
            FinishBhop(false);
        }
    }

    void TrainerFeatures::ProcessWallMovement()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto addrs = Game::GameState::Get().GetAddrs();

        if (cfg.State.infiniteWallclimbs)
        {
            if (Memory::SafeRead<int>(addrs.wallclimbCount) != 0)
            {
                if (Memory::SafeRead<int>(addrs.wallCount) > 2)
                    Memory::SafeWrite<int>(addrs.wallCount, 2);
                Memory::SafeWrite<int>(addrs.wallclimbCount, 0);
            }
        }

        if (cfg.State.infiniteWallruns)
        {
            if (Memory::SafeRead<int>(addrs.wallrunCount) != 0)
            {
                if (Memory::SafeRead<int>(addrs.wallCount) > 2)
                    Memory::SafeWrite<int>(addrs.wallCount, 2);
                Memory::SafeWrite<int>(addrs.wallrunCount, 0);
            }
        }

        if (cfg.State.sameWall)
        {
            if (Memory::SafeRead<int>(addrs.wallCount) != 0)
                Memory::SafeWrite<int>(addrs.wallCount, 0);
        }
    }

    void TrainerFeatures::ProcessFastLoads()
    {
        auto& cfg = Core::ConfigManager::Get();
        auto& game = Game::GameState::Get();

        // Only speed the engine up while a load is actually in progress, and only when the loading flag
        // could really be read (an unresolved flag must not be treated as "loading")
        bool shouldApply = cfg.State.fastLoads && game.IsLoadingKnown() && game.IsLoading();

        if (shouldApply && !m_FastLoadsApplied)
        {
            uintptr_t simRate = Game::Offsets::EngineField(Game::Offsets::Engine::SimulationRate);
            uintptr_t timeScale = Game::Offsets::EngineField(Game::Offsets::Engine::TimeScale);
            if (!simRate || !timeScale) return;

            m_OriginalSimRate = Memory::SafeRead<float>(simRate, 30.0f);
            m_OriginalLoadTimeScale = Memory::SafeRead<float>(timeScale, 1.0f);
            Memory::SafeWrite<float>(simRate, 50000.0f);
            Memory::SafeWrite<float>(timeScale, 3000.0f);
            m_FastLoadsApplied = true;
        }
        else if (!shouldApply && m_FastLoadsApplied)
        {
            RestoreFastLoads();
        }
    }

    void TrainerFeatures::RestoreFastLoads()
    {
        if (!m_FastLoadsApplied) return;

        Memory::SafeWrite<float>(Game::Offsets::EngineField(Game::Offsets::Engine::SimulationRate), m_OriginalSimRate);
        Memory::SafeWrite<float>(Game::Offsets::EngineField(Game::Offsets::Engine::TimeScale), m_OriginalLoadTimeScale);
        m_FastLoadsApplied = false;
    }
}
