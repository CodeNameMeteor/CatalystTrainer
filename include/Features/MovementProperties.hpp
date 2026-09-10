#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace Features
{
    enum class MovementCategory
    {
        Running,
        Wallclimbing,
        Wallrunning,
        Coil,
        Uncontrolled_Slide,
        Mag_Pullup,
        Mag_Swing
    };

    struct PropertyItem
    {
        const char* label;
        float* valuePtr;
        float defaultValue;
        uintptr_t baseAddress;
        std::vector<unsigned int> offsets;
    };

    class MovementManager
    {
    public:
        static MovementManager& Get();

        void RenderImGuiCategory(MovementCategory category);
        void RandomizeAll();
        void ResetAll();

        // Running & Jumps
        float maxRunSpeed = 7.2f;
        float timeToMaxSpeed = 4.0f;
        float turningDecelerationFactor = 20.0f;
        float directionChangeFactor = 7.0f;
        float aboveMaxSpeedDeceleration = 10.0f;
        float forwardFriction = 3.0f;
        float backwardFriction = 6.0f;
        float strafeFriction = 5.0f;

        float maxSpeedJumpHeight = 1.2f;
        float maxSpeedJumpVelocity = 8.04f;
        float nonMaxSpeedJumpHeight = 1.1f;
        float nonMaxSpeedJumpVelocity = 5.96f;
        float lowSpeedJumpHeight = 1.1f;
        float lowSpeedJumpVelocity = 2.28f;
        float standingJumpHeight = 1.1f;
        float standingJumpVelocity = 0.0f;
        float walkingJumpHeight = 1.1f;
        float walkingJumpVelocity = 4.12f;

        // Wallclimbing
        float wcMinimumLength = 0.0f;
        float wcMaximumLength = 0.0f;
        float wcMaxAngleDelta = 40.0f;
        float wcRotationSpeed = 300.0f;
        float fwcHeight = 2.6f;
        float fwcTimeMs = 100.0f;
        float fwcAlignTimeMs = 16.0f;
        float fwcTimeToApexMs = 60.0f;
        float fwcPostApexHeight = 1.3f;
        float fwcPostApexFalloffStrength = 2.0f;
        float fwcPostApexVerticalFalloffStrength = 2.0f;
        float swcHeight = 2.6f;
        float swcTimeMs = 100.0f;
        float swcAlignTimeMs = 16.0f;
        float swcTimeToApexMs = 60.0f;
        float swcPostApexHeight = 1.3f;
        float swcPostApexFalloffStrength = 2.0f;
        float swcPostApexVerticalFalloffStrength = 2.0f;
        float wcjHeight = 1.1f;
        float wcjVelocity = 7.2f;

        // Wallrunning
        float wrMinimumLength = 4.0f;
        float wrMaximumLength = 10.0f;
        float wrMaxAngleDelta = 5.0f;
        float wrRotationSpeed = 160.0f;
        float fwrHeight = 1.2f;
        float fwrTimeMs = 80.0f;
        float fwrAlignTimeMs = 16.0f;
        float fwrTimeToApexMs = 32.0f;
        float fwrPostApexHeight = 2.2f;
        float fwrPostApexFalloffStrength = 3.0f;
        float fwrPostApexVerticalFalloffStrength = 2.0f;
        float swrHeight = 1.2f;
        float swrTimeMs = 80.0f;
        float swrAlignTimeMs = 16.0f;
        float swrTimeToApexMs = 32.0f;
        float swrPostApexHeight = 2.2f;
        float swrPostApexFalloffStrength = 3.0f;
        float swrPostApexVerticalFalloffStrength = 2.0f;
        float wrjHeight = 1.1f;
        float wrjVelocity = 7.2f;

        // Coil & Slide
        float coilHeight = 0.64f;
        float coilBlend = 10.0f;
        float coilFallSpeed = 20.0f;
        float maxSlideSpeed = 12.0f;
        float maxStrafeSpeed = 3.0f;
        float rotationSpeed = 1080.0f;

        // Mag Pullup & Swing
        float pullupAcc = 20.0f;
        float pullupDec = 4.0f;
        float pullupEndOffsetY = 3.03f;
        float pullupEndOffsetZ = 1.085f;
        float pullupGravity = 14.0f;
        float pullupMaxGravity = 10.0f;
        float pullupMaxLedgeAlignDistance = 10.0f;
        float pullupMinLedgeAlignDistance = 10.0f;
        float pullupMaxSpeed = 10.0f;
        float pullupRopeSlack = 0.0f;
        float pullupmaxRotationSpeed = 100.0f;
        float pullupStartSpeed = 6.0f;

        float swingDeceleration = 0.2f;
        float swingGravity = 30.0f;
        float swingAcceleration = 7.0f;
        float swingMaxGravity = 25.0f;
        float swingMaxForwardSpeed = 15.0f;
        float swingMaxBackwardSpeed = 16.0f;
        float swingParticleGravity = 10.0f;

    private:
        MovementManager();
        std::vector<PropertyItem> m_AllProperties;
    };
}
