#include "Features/MovementProperties.hpp"
#include "Core/Memory.hpp"
#include "Game/GameOffsets.hpp"
#include "imgui/imgui.h"
#include <random>

namespace Features
{
    MovementManager& MovementManager::Get()
    {
        static MovementManager instance;
        return instance;
    }

    MovementManager::MovementManager()
    {
        uintptr_t bhopBase = Game::Offsets::BhopFactorBase();
        uintptr_t jumpBase = Game::Offsets::JumpPropertiesBase();
        uintptr_t wallBase = Game::Offsets::WallPropertiesBase();

        // Populate comprehensive properties table for randomizer and bulk resets
        m_AllProperties = {
            {"Max Run Speed", &maxRunSpeed, 7.2f, bhopBase, {0x408, 0x380, 0x1C5C}},
            {"Turning Deceleration", &turningDecelerationFactor, 20.0f, bhopBase, {0x408, 0x380, 0x1C54}},
            {"Direction Change", &directionChangeFactor, 7.0f, bhopBase, {0x408, 0x380, 0x1C50}},
            {"Above Max Decel", &aboveMaxSpeedDeceleration, 10.0f, bhopBase, {0x408, 0x380, 0x1C4C}},
            {"Forward Friction", &forwardFriction, 3.0f, bhopBase, {0x408, 0x380, 0x1C40}},
            {"Backward Friction", &backwardFriction, 6.0f, bhopBase, {0x408, 0x380, 0x1C44}},
            {"Strafe Friction", &strafeFriction, 5.0f, bhopBase, {0x408, 0x380, 0x1C48}},

            {"Max Speed Jump Height", &maxSpeedJumpHeight, 1.2f, jumpBase, {0xA48, 0x120, 0x8B4}},
            {"Max Speed Jump Vel", &maxSpeedJumpVelocity, 8.04f, jumpBase, {0xA48, 0x120, 0x8B0}},
            {"Non-Max Jump Height", &nonMaxSpeedJumpHeight, 1.1f, jumpBase, {0xA48, 0x120, 0x87C}},
            {"Non-Max Jump Vel", &nonMaxSpeedJumpVelocity, 5.96f, jumpBase, {0xA48, 0x120, 0x878}},
            {"Low Speed Jump Height", &lowSpeedJumpHeight, 1.1f, jumpBase, {0xA48, 0x120, 0x64C}},
            {"Low Speed Jump Vel", &lowSpeedJumpVelocity, 2.28f, jumpBase, {0xA48, 0x120, 0x648}},
            {"Standing Jump Height", &standingJumpHeight, 1.1f, jumpBase, {0xA48, 0x120, 0x80C}},
            {"Standing Jump Vel", &standingJumpVelocity, 0.0f, jumpBase, {0xA48, 0x120, 0x808}},
            {"Walking Jump Height", &walkingJumpHeight, 1.1f, jumpBase, {0xA48, 0x120, 0x7D4}},
            {"Walking Jump Vel", &walkingJumpVelocity, 4.12f, jumpBase, {0xA48, 0x120, 0x7D0}},

            {"WC Max Angle Delta", &wcMaxAngleDelta, 40.0f, wallBase, {0x22B0, 0x30, 0x5BC}},
            {"WC Rotation Speed", &wcRotationSpeed, 300.0f, wallBase, {0x22B0, 0x30, 0x5C0}},
            {"First WC Height", &fwcHeight, 2.6f, wallBase, {0x26F0, 0x58, 0x380}},
            {"First WC Time (ms)", &fwcTimeMs, 100.0f, wallBase, {0x22B0, 0x30, 0x5A4}},
            {"Second WC Height", &swcHeight, 2.6f, wallBase, {0x22B0, 0x30, 0x598}},
            {"Second WC Time (ms)", &swcTimeMs, 100.0f, wallBase, {0x22B0, 0x30, 0x5B0}},

            {"WR Minimum Length", &wrMinimumLength, 4.0f, wallBase, {0x1A78, 0xE0}},
            {"WR Maximum Length", &wrMaximumLength, 10.0f, wallBase, {0x1A78, 0xE4}},
            {"First WR Height", &fwrHeight, 1.2f, wallBase, {0x1A78, 0xE8}},
            {"First WR Time", &fwrTimeMs, 80.0f, wallBase, {0x1A78, 0x104}},
            {"Second WR Height", &swrHeight, 1.2f, wallBase, {0x1A78, 0xF8}},
            {"Second WR Time", &swrTimeMs, 64.0f, wallBase, {0x1A78, 0x110}},

            {"Coil Height", &coilHeight, 0.64f, wallBase, {0xC8, 0xC98, 0x1FD0}},
            {"Coil Fall Speed", &coilFallSpeed, 20.0f, wallBase, {0xC8, 0xC98, 0x1FD8}},
            {"Max Slide Speed", &maxSlideSpeed, 12.0f, wallBase, {0x1780, 0xB88, 0x63C}},
            {"Max Strafe Speed", &maxStrafeSpeed, 3.0f, wallBase, {0x1780, 0xB88, 0x630}},
            {"Slide Rotation Speed", &rotationSpeed, 1080.0f, wallBase, {0x1780, 0xB88, 0x638}},

            {"Mag Pullup Accel", &pullupAcc, 20.0f, wallBase, {0x778, 0x28, 0x324}},
            {"Mag Pullup Decel", &pullupDec, 4.0f, wallBase, {0x778, 0x28, 0x330}},
            {"Mag Pullup Gravity", &pullupGravity, 14.0f, wallBase, {0x778, 0x28, 0x334}},
            {"Mag Swing Gravity", &swingGravity, 30.0f, wallBase, {0x2458, 0xD0, 0xD8}},
            {"Mag Swing Accel", &swingAcceleration, 7.0f, wallBase, {0x2458, 0xD0, 0xC8}}
        };
    }

    void MovementManager::RandomizeAll()
    {
        std::random_device rd;
        std::mt19937 gen(rd());

        for (auto& prop : m_AllProperties)
        {
            if (prop.defaultValue > 0.001f)
            {
                std::uniform_real_distribution<float> dis(prop.defaultValue * 0.5f, prop.defaultValue * 2.5f);
                *prop.valuePtr = dis(gen);
            }
            else
            {
                std::uniform_real_distribution<float> dis(0.1f, 15.0f);
                *prop.valuePtr = dis(gen);
            }

            uintptr_t targetAddr = Core::Memory::ResolvePtrChain(prop.baseAddress, prop.offsets);
            Core::Memory::SafeWrite<float>(targetAddr, *prop.valuePtr);
        }
    }

    void MovementManager::ResetAll()
    {
        for (auto& prop : m_AllProperties)
        {
            *prop.valuePtr = prop.defaultValue;
            uintptr_t targetAddr = Core::Memory::ResolvePtrChain(prop.baseAddress, prop.offsets);
            Core::Memory::SafeWrite<float>(targetAddr, *prop.valuePtr);
        }
    }

    void MovementManager::RenderImGuiCategory(MovementCategory category)
    {
        uintptr_t bhopBase = Game::Offsets::BhopFactorBase();
        uintptr_t jumpBase = Game::Offsets::JumpPropertiesBase();
        uintptr_t wallBase = Game::Offsets::WallPropertiesBase();

        auto DrawInput = [](const char* name, float* val, uintptr_t base, const std::vector<unsigned int>& offs) {
            if (ImGui::InputFloat(name, val))
            {
                uintptr_t addr = Core::Memory::ResolvePtrChain(base, offs);
                Core::Memory::SafeWrite<float>(addr, *val);
            }
        };

        switch (category)
        {
        case MovementCategory::Running:
            if (ImGui::CollapsingHeader("Running Speeds", ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawInput("Max Run Speed", &maxRunSpeed, bhopBase, {0x408, 0x380, 0x1C5C});
                DrawInput("Time to Max Speed", &timeToMaxSpeed, bhopBase, {0x408, 0x380, 0x1C58});
                DrawInput("Turning Deceleration", &turningDecelerationFactor, bhopBase, {0x408, 0x380, 0x1C54});
                DrawInput("Direction Change Factor", &directionChangeFactor, bhopBase, {0x408, 0x380, 0x1C50});
                DrawInput("Above Max Decel", &aboveMaxSpeedDeceleration, bhopBase, {0x408, 0x380, 0x1C4C});
            }
            if (ImGui::CollapsingHeader("Friction"))
            {
                DrawInput("Forward Friction", &forwardFriction, bhopBase, {0x408, 0x380, 0x1C40});
                DrawInput("Backward Friction", &backwardFriction, bhopBase, {0x408, 0x380, 0x1C44});
                DrawInput("Strafe Friction", &strafeFriction, bhopBase, {0x408, 0x380, 0x1C48});
            }
            if (ImGui::CollapsingHeader("Jumps"))
            {
                DrawInput("Max Speed Jump Height", &maxSpeedJumpHeight, jumpBase, {0xA48, 0x120, 0x8B4});
                DrawInput("Max Speed Jump Velocity", &maxSpeedJumpVelocity, jumpBase, {0xA48, 0x120, 0x8B0});
                DrawInput("Standing Jump Height", &standingJumpHeight, jumpBase, {0xA48, 0x120, 0x80C});
                DrawInput("Walking Jump Velocity", &walkingJumpVelocity, jumpBase, {0xA48, 0x120, 0x7D0});
            }
            break;

        case MovementCategory::Wallclimbing:
            DrawInput("WC Max Angle Delta", &wcMaxAngleDelta, wallBase, {0x22B0, 0x30, 0x5BC});
            DrawInput("WC Rotation Speed", &wcRotationSpeed, wallBase, {0x22B0, 0x30, 0x5C0});
            DrawInput("First WC Height", &fwcHeight, wallBase, {0x26F0, 0x58, 0x380});
            DrawInput("Second WC Height", &swcHeight, wallBase, {0x22B0, 0x30, 0x598});
            DrawInput("WC Jump Velocity", &wcjVelocity, jumpBase, {0xA48, 0x120, 0x728});
            break;

        case MovementCategory::Wallrunning:
            DrawInput("WR Min Length", &wrMinimumLength, wallBase, {0x1A78, 0xE0});
            DrawInput("WR Max Length", &wrMaximumLength, wallBase, {0x1A78, 0xE4});
            DrawInput("First WR Height", &fwrHeight, wallBase, {0x1A78, 0xE8});
            DrawInput("Second WR Height", &swrHeight, wallBase, {0x1A78, 0xF8});
            DrawInput("WR Jump Velocity", &wrjVelocity, jumpBase, {0xA48, 0x120, 0x840});
            break;

        case MovementCategory::Coil:
            DrawInput("Coil Height", &coilHeight, wallBase, {0xC8, 0xC98, 0x1FD0});
            DrawInput("Coil Blend Ticks", &coilBlend, wallBase, {0xC8, 0xC98, 0x1FD4});
            DrawInput("Coil Fall Speed", &coilFallSpeed, wallBase, {0xC8, 0xC98, 0x1FD8});
            break;

        case MovementCategory::Uncontrolled_Slide:
            DrawInput("Max Slide Speed", &maxSlideSpeed, wallBase, {0x1780, 0xB88, 0x63C});
            DrawInput("Max Strafe Speed", &maxStrafeSpeed, wallBase, {0x1780, 0xB88, 0x630});
            DrawInput("Rotation Speed", &rotationSpeed, wallBase, {0x1780, 0xB88, 0x638});
            break;

        case MovementCategory::Mag_Pullup:
            DrawInput("Pullup Accel", &pullupAcc, wallBase, {0x778, 0x28, 0x324});
            DrawInput("Pullup Decel", &pullupDec, wallBase, {0x778, 0x28, 0x330});
            DrawInput("Pullup Gravity", &pullupGravity, wallBase, {0x778, 0x28, 0x334});
            DrawInput("Pullup Max Speed", &pullupMaxSpeed, wallBase, {0x778, 0x28, 0x328});
            break;

        case MovementCategory::Mag_Swing:
            DrawInput("Swing Gravity", &swingGravity, wallBase, {0x2458, 0xD0, 0xD8});
            DrawInput("Swing Accel", &swingAcceleration, wallBase, {0x2458, 0xD0, 0xC8});
            DrawInput("Max Forward Speed", &swingMaxForwardSpeed, wallBase, {0x2458, 0xD0, 0xCC});
            break;
        }
    }
}
