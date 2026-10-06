#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include "Core/Memory.hpp"

namespace Game
{
    namespace Offsets
    {
        // Root pointers, as RVAs from the base of MirrorsEdgeCatalyst.exe.
        // These are only valid for one specific build of the game, see BuildInfo below.
        inline uintptr_t Base()               { return Core::Memory::GetBaseAddress(); }

        inline uintptr_t LoadingState()       { return Base() + 0x240C2B8; }
        inline uintptr_t PlayerEntity()       { return Base() + 0x2106A68; }
        inline uintptr_t PlayerVelocity()     { return Base() + 0x23E29B8; }
        inline uintptr_t ImmortalBase()       { return Base() + 0x2577770; }
        inline uintptr_t InputBase()          { return Base() + 0x214B460; }
        inline uintptr_t GroundStatusBase()   { return Base() + 0x23DA028; }
        inline uintptr_t TimeOfDayBase()      { return Base() + 0x255C2F8; }
        inline uintptr_t ContentActiveBase()  { return Base() + 0x257E7C8; }
        inline uintptr_t DashStartedBase()    { return Base() + 0x257C9D0; }
        inline uintptr_t CameraAnglesBase()   { return Base() + 0x2578A68; }
        inline uintptr_t CameraMatrixBase()   { return Base() + 0x2401CB0; }
        inline uintptr_t VisualsBase()        { return Base() + 0x23DD0A8; }
        inline uintptr_t EngineSettings()     { return Base() + 0x2142A68; }
        inline uintptr_t InMenuAddress()      { return Base() + 0x25946E4; }
        inline uintptr_t BhopFactorBase()     { return Base() + 0x23BD908; }
        inline uintptr_t JumpTriggerBase()    { return Base() + 0x256D7C0; }
        inline uintptr_t JumpPropertiesBase() { return Base() + 0x2105510; }
        inline uintptr_t WallPropertiesBase() { return Base() + 0x23DD0B8; }

        // Running/friction tuning block: BhopFactorBase -> 0x408 -> 0x380 -> field
        namespace Movement
        {
            constexpr unsigned int ForwardFriction    = 0x1C40;
            constexpr unsigned int BackwardFriction   = 0x1C44;
            constexpr unsigned int StrafeFriction     = 0x1C48;
            constexpr unsigned int AboveMaxSpeedDecel = 0x1C4C;
            constexpr unsigned int DirectionChange    = 0x1C50;
            constexpr unsigned int TurningDecel       = 0x1C54;
            constexpr unsigned int TimeToMaxSpeed     = 0x1C58;
            constexpr unsigned int MaxRunSpeed        = 0x1C5C;
        }

        inline uintptr_t MovementField(unsigned int field)
        {
            return Core::Memory::ResolvePtrChain(BhopFactorBase(), { 0x408, 0x380, field });
        }

        // EngineSettings -> field
        namespace Engine
        {
            constexpr unsigned int SimulationRate = 0x2C;
            constexpr unsigned int TimeScale      = 0x48;
        }

        inline uintptr_t EngineField(unsigned int field)
        {
            return Core::Memory::ResolvePtrChain(EngineSettings(), { field });
        }

        // VisualsBase -> 0x2450 -> 0x8 -> field. These are single-byte flags.
        namespace Visuals
        {
            constexpr unsigned int Blur     = 0x228;
            constexpr unsigned int Bloom    = 0x22B;
            constexpr unsigned int Vignette = 0x239;
        }

        inline uintptr_t VisualsField(unsigned int field)
        {
            return Core::Memory::ResolvePtrChain(VisualsBase(), { 0x2450, 0x8, field });
        }

        inline uintptr_t JumpTrigger()
        {
            return Core::Memory::ResolvePtrChain(JumpTriggerBase(), { 0x1BB0 });
        }
    }

    // Player states written by the trainer (values observed in game, names describe how they are used here)
    namespace PlayerStates
    {
        constexpr int Neutral       = 2;  // Written to leave noclip cleanly
        constexpr int StumbleLandA  = 3;  // States where No Stumble must not interfere
        constexpr int StumbleLandB  = 4;
        constexpr int Jump          = 8;  // Forced for the bunnyhop
    }

    // Identifies the game build the offsets above were taken from. Fill these in from the values shown in
    // the trainer's Settings tab on the supported build; while both are 0 the build is "unverified" and the
    // trainer still runs, but shows a warning. On a known mismatch all memory writes are disabled.
    namespace BuildInfo
    {
        constexpr uint32_t kSupportedTimeDateStamp = 0;
        constexpr uint32_t kSupportedSizeOfImage   = 0;

        enum class Status
        {
            Supported,
            Unverified,
            Unsupported
        };

        struct Detected
        {
            uint32_t timeDateStamp = 0;
            uint32_t sizeOfImage = 0;
        };

        inline Detected Detect()
        {
            Detected result;
            uintptr_t base = Core::Memory::GetBaseAddress();
            if (!base) return result;

            auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return result;
            auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE) return result;

            result.timeDateStamp = nt->FileHeader.TimeDateStamp;
            result.sizeOfImage = nt->OptionalHeader.SizeOfImage;
            return result;
        }

        inline Status Check(const Detected& detected)
        {
            if (kSupportedTimeDateStamp == 0 && kSupportedSizeOfImage == 0)
                return Status::Unverified;
            if (detected.timeDateStamp == kSupportedTimeDateStamp && detected.sizeOfImage == kSupportedSizeOfImage)
                return Status::Supported;
            return Status::Unsupported;
        }
    }

    // Replaces the time-of-day part of a raw game time value while keeping any whole days it contains
    inline float SetHourOfDay(float rawTime, float hours)
    {
        constexpr float kDay = 86400.0f;
        float normalized = std::fmod(rawTime, kDay);
        if (normalized < 0.0f) normalized += kDay;
        return (rawTime - normalized) + hours * 3600.0f;
    }

    inline float HourOfDay(float rawTime)
    {
        constexpr float kDay = 86400.0f;
        float normalized = std::fmod(rawTime, kDay);
        if (normalized < 0.0f) normalized += kDay;
        return normalized / 3600.0f;
    }
}
