#pragma once
#include <cstdint>
#include <vector>
#include "Core/Memory.hpp"

namespace Game
{
    namespace Offsets
    {
        // Dynamic rebased root pointers
        inline uintptr_t LoadingState()       { return Core::Memory::GetBaseAddress() + 0x240C2B8; }
        inline uintptr_t PlayerEntity()       { return Core::Memory::Rebase(0x142106A68); }
        inline uintptr_t PlayerVelocity()     { return Core::Memory::Rebase(0x1423E29B8); }
        inline uintptr_t ImmortalBase()       { return Core::Memory::GetBaseAddress() + 0x02577770; }
        inline uintptr_t InputBase()          { return Core::Memory::GetBaseAddress() + 0x0214B460; }
        inline uintptr_t GroundStatusBase()   { return Core::Memory::GetBaseAddress() + 0x023DA028; }
        inline uintptr_t TimeOfDayBase()      { return Core::Memory::Rebase(0x14255C2F8); }
        inline uintptr_t ContentActiveBase()  { return Core::Memory::GetBaseAddress() + 0x257E7C8; }
        inline uintptr_t DashStartedBase()    { return Core::Memory::GetBaseAddress() + 0x0257C9D0; }
        inline uintptr_t CameraAnglesBase()   { return Core::Memory::Rebase(0x142578A68); }
        inline uintptr_t CameraMatrixBase()   { return Core::Memory::Rebase(0x142401CB0); }
        inline uintptr_t VisualsBase()        { return Core::Memory::Rebase(0x1423DD0A8); }
        inline uintptr_t EngineSettings()     { return Core::Memory::Rebase(0x142142A68); }
        inline uintptr_t InMenuAddress()      { return Core::Memory::Rebase(0x1425946E4); }
        inline uintptr_t BhopFactorBase()     { return Core::Memory::Rebase(0x1423BD908); }
        inline uintptr_t JumpTriggerBase()    { return Core::Memory::Rebase(0x14256D7C0); }
        inline uintptr_t JumpPropertiesBase() { return Core::Memory::Rebase(0x142105510); }
        inline uintptr_t WallPropertiesBase() { return Core::Memory::Rebase(0x1423DD0B8); }
    }
}
