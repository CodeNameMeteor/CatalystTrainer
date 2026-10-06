#include "Features/MovementProperties.hpp"
#include "Core/Memory.hpp"
#include "Game/GameOffsets.hpp"
#include "imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace Features
{
    using Game::Offsets::BhopFactorBase;
    using Game::Offsets::JumpPropertiesBase;
    using Game::Offsets::WallPropertiesBase;
    namespace Mv = Game::Offsets::Movement;

    MovementManager& MovementManager::Get()
    {
        static MovementManager instance;
        return instance;
    }

    MovementManager::MovementManager()
    {
        using C = MovementCategory;
        const std::vector<unsigned int> tuning = { 0x408, 0x380 };
        auto Run = [&](unsigned int field) { std::vector<unsigned int> v = tuning; v.push_back(field); return v; };

        m_Properties = {
            // label                      category              group             base                 offsets                      default  randomize
            {"Max Run Speed",             C::Running, "Running Speeds", BhopFactorBase, Run(Mv::MaxRunSpeed),        7.2f,   true},
            {"Time to Max Speed",         C::Running, "Running Speeds", BhopFactorBase, Run(Mv::TimeToMaxSpeed),     4.0f,   false},
            {"Turning Deceleration",      C::Running, "Running Speeds", BhopFactorBase, Run(Mv::TurningDecel),       20.0f,  true},
            {"Direction Change Factor",   C::Running, "Running Speeds", BhopFactorBase, Run(Mv::DirectionChange),    7.0f,   true},
            {"Above Max Decel",           C::Running, "Running Speeds", BhopFactorBase, Run(Mv::AboveMaxSpeedDecel), 10.0f,  true},

            {"Forward Friction",          C::Running, "Friction",       BhopFactorBase, Run(Mv::ForwardFriction),    3.0f,   true},
            {"Backward Friction",         C::Running, "Friction",       BhopFactorBase, Run(Mv::BackwardFriction),   6.0f,   true},
            {"Strafe Friction",           C::Running, "Friction",       BhopFactorBase, Run(Mv::StrafeFriction),     5.0f,   true},

            {"Max Speed Jump Height",     C::Running, "Jumps", JumpPropertiesBase, {0xA48, 0x120, 0x8B4}, 1.2f,  true},
            {"Max Speed Jump Velocity",   C::Running, "Jumps", JumpPropertiesBase, {0xA48, 0x120, 0x8B0}, 8.04f, true},
            {"Non-Max Jump Height",       C::Running, "Jumps", JumpPropertiesBase, {0xA48, 0x120, 0x87C}, 1.1f,  true},
            {"Non-Max Jump Velocity",     C::Running, "Jumps", JumpPropertiesBase, {0xA48, 0x120, 0x878}, 5.96f, true},
            {"Low Speed Jump Height",     C::Running, "Jumps", JumpPropertiesBase, {0xA48, 0x120, 0x64C}, 1.1f,  true},
            {"Low Speed Jump Velocity",   C::Running, "Jumps", JumpPropertiesBase, {0xA48, 0x120, 0x648}, 2.28f, true},
            {"Standing Jump Height",      C::Running, "Jumps", JumpPropertiesBase, {0xA48, 0x120, 0x80C}, 1.1f,  true},
            {"Standing Jump Velocity",    C::Running, "Jumps", JumpPropertiesBase, {0xA48, 0x120, 0x808}, 0.0f,  true},
            {"Walking Jump Height",       C::Running, "Jumps", JumpPropertiesBase, {0xA48, 0x120, 0x7D4}, 1.1f,  true},
            {"Walking Jump Velocity",     C::Running, "Jumps", JumpPropertiesBase, {0xA48, 0x120, 0x7D0}, 4.12f, true},

            {"WC Max Angle Delta",        C::Wallclimbing, nullptr, WallPropertiesBase, {0x22B0, 0x30, 0x5BC}, 40.0f,  true},
            {"WC Rotation Speed",         C::Wallclimbing, nullptr, WallPropertiesBase, {0x22B0, 0x30, 0x5C0}, 300.0f, true},
            {"First WC Height",           C::Wallclimbing, nullptr, WallPropertiesBase, {0x26F0, 0x58, 0x380}, 2.6f,   true},
            {"First WC Time (ms)",        C::Wallclimbing, nullptr, WallPropertiesBase, {0x22B0, 0x30, 0x5A4}, 100.0f, true},
            {"Second WC Height",          C::Wallclimbing, nullptr, WallPropertiesBase, {0x22B0, 0x30, 0x598}, 2.6f,   true},
            {"Second WC Time (ms)",       C::Wallclimbing, nullptr, WallPropertiesBase, {0x22B0, 0x30, 0x5B0}, 100.0f, true},
            {"WC Jump Velocity",          C::Wallclimbing, nullptr, JumpPropertiesBase, {0xA48, 0x120, 0x728}, 7.2f,   false},

            {"WR Min Length",             C::Wallrunning, nullptr, WallPropertiesBase, {0x1A78, 0xE0},          4.0f,  true},
            {"WR Max Length",             C::Wallrunning, nullptr, WallPropertiesBase, {0x1A78, 0xE4},          10.0f, true},
            {"First WR Height",           C::Wallrunning, nullptr, WallPropertiesBase, {0x1A78, 0xE8},          1.2f,  true},
            {"First WR Time",             C::Wallrunning, nullptr, WallPropertiesBase, {0x1A78, 0x104},         80.0f, true},
            {"Second WR Height",          C::Wallrunning, nullptr, WallPropertiesBase, {0x1A78, 0xF8},          1.2f,  true},
            {"Second WR Time",            C::Wallrunning, nullptr, WallPropertiesBase, {0x1A78, 0x110},         64.0f, true},
            {"WR Jump Velocity",          C::Wallrunning, nullptr, JumpPropertiesBase, {0xA48, 0x120, 0x840},  7.2f,  false},

            {"Coil Height",               C::Coil, nullptr, WallPropertiesBase, {0xC8, 0xC98, 0x1FD0}, 0.64f, true},
            {"Coil Blend Ticks",          C::Coil, nullptr, WallPropertiesBase, {0xC8, 0xC98, 0x1FD4}, 10.0f, false},
            {"Coil Fall Speed",           C::Coil, nullptr, WallPropertiesBase, {0xC8, 0xC98, 0x1FD8}, 20.0f, true},

            {"Max Slide Speed",           C::Uncontrolled_Slide, nullptr, WallPropertiesBase, {0x1780, 0xB88, 0x63C}, 12.0f,   true},
            {"Max Strafe Speed",          C::Uncontrolled_Slide, nullptr, WallPropertiesBase, {0x1780, 0xB88, 0x630}, 3.0f,    true},
            {"Slide Rotation Speed",      C::Uncontrolled_Slide, nullptr, WallPropertiesBase, {0x1780, 0xB88, 0x638}, 1080.0f, true},

            {"Pullup Accel",              C::Mag_Pullup, nullptr, WallPropertiesBase, {0x778, 0x28, 0x324}, 20.0f, true},
            {"Pullup Decel",              C::Mag_Pullup, nullptr, WallPropertiesBase, {0x778, 0x28, 0x330}, 4.0f,  true},
            {"Pullup Gravity",            C::Mag_Pullup, nullptr, WallPropertiesBase, {0x778, 0x28, 0x334}, 14.0f, true},
            {"Pullup Max Speed",          C::Mag_Pullup, nullptr, WallPropertiesBase, {0x778, 0x28, 0x328}, 10.0f, false},

            {"Swing Gravity",             C::Mag_Swing, nullptr, WallPropertiesBase, {0x2458, 0xD0, 0xD8}, 30.0f, true},
            {"Swing Accel",               C::Mag_Swing, nullptr, WallPropertiesBase, {0x2458, 0xD0, 0xC8}, 7.0f,  true},
            {"Max Forward Speed",         C::Mag_Swing, nullptr, WallPropertiesBase, {0x2458, 0xD0, 0xCC}, 15.0f, false},
        };

        for (auto& prop : m_Properties)
            prop.value = prop.defaultValue;
    }

    static float MaxValueFor(const PropertyItem& item)
    {
        return (std::max)(100.0f, item.defaultValue * 20.0f);
    }

    bool MovementManager::WriteProperty(PropertyItem& item, float value)
    {
        if (!std::isfinite(value)) return false;
        value = (std::clamp)(value, 0.0f, MaxValueFor(item));

        uintptr_t addr = Core::Memory::ResolvePtrChain(item.base(), item.offsets);
        if (!addr) return false;

        if (!item.haveOriginal)
        {
            float current = Core::Memory::SafeRead<float>(addr, item.defaultValue);
            item.original = std::isfinite(current) ? current : item.defaultValue;
            item.haveOriginal = true;
        }

        if (!Core::Memory::SafeWrite<float>(addr, value)) return false;

        item.value = value;
        item.modified = true;
        return true;
    }

    void MovementManager::SyncFromGame(PropertyItem& item)
    {
        if (item.modified || item.editing) return;

        uintptr_t addr = Core::Memory::ResolvePtrChain(item.base(), item.offsets);
        if (!addr) return;

        float current = Core::Memory::SafeRead<float>(addr, item.value);
        if (std::isfinite(current))
            item.value = current;
    }

    void MovementManager::RandomizeAll()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        std::random_device rd;
        std::mt19937 gen(rd());

        for (auto& prop : m_Properties)
        {
            if (!prop.randomize) continue;

            float value;
            if (prop.defaultValue > 0.001f)
            {
                std::uniform_real_distribution<float> dis(prop.defaultValue * 0.5f, prop.defaultValue * 2.5f);
                value = dis(gen);
            }
            else
            {
                std::uniform_real_distribution<float> dis(0.1f, 15.0f);
                value = dis(gen);
            }

            WriteProperty(prop, value);
        }
    }

    void MovementManager::ResetAll()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        for (auto& prop : m_Properties)
        {
            if (!prop.modified) continue;

            uintptr_t addr = Core::Memory::ResolvePtrChain(prop.base(), prop.offsets);
            if (addr && prop.haveOriginal)
                Core::Memory::SafeWrite<float>(addr, prop.original);

            prop.value = prop.haveOriginal ? prop.original : prop.defaultValue;
            prop.modified = false;
            prop.editing = false;
        }
    }

    void MovementManager::RenderImGuiCategory(MovementCategory category)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        const char* currentGroup = nullptr;
        bool groupOpen = true;
        bool firstGroup = true;

        for (auto& prop : m_Properties)
        {
            if (prop.category != category) continue;

            if (prop.group && prop.group != currentGroup)
            {
                currentGroup = prop.group;
                groupOpen = ImGui::CollapsingHeader(prop.group, firstGroup ? ImGuiTreeNodeFlags_DefaultOpen : 0);
                firstGroup = false;
            }
            if (!groupOpen) continue;

            SyncFromGame(prop);

            // No +/- buttons so the input is a single item and IsItemDeactivatedAfterEdit refers to it.
            // The game is only written once editing finishes, not on every keystroke.
            float edited = prop.value;
            if (ImGui::InputFloat(prop.label, &edited, 0.0f, 0.0f, "%.3f"))
            {
                prop.value = edited;
                prop.editing = true;
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                prop.editing = false;
                WriteProperty(prop, prop.value);
            }
            else if (prop.editing && !ImGui::IsItemActive())
            {
                prop.editing = false;
            }

            if (prop.modified && ImGui::IsItemHovered() && prop.haveOriginal)
            {
                ImGui::SetTooltip("Game value: %.3f (Reset restores it)", prop.original);
            }
        }
    }
}
