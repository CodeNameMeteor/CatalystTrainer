#pragma once
#include <vector>
#include <cstdint>
#include <mutex>

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
        MovementCategory category;
        const char* group;              // Collapsing header within the category, or nullptr
        uintptr_t (*base)();
        std::vector<unsigned int> offsets;
        float defaultValue;             // Used for randomisation ranges and as a fallback
        bool randomize;

        float value = 0.0f;             // Value shown in the menu
        bool editing = false;           // User is typing; don't overwrite with the game value
        bool modified = false;          // Trainer has written this property
        bool haveOriginal = false;
        float original = 0.0f;          // Game value before the trainer first changed it
    };

    // Shown in the "Movement Tuning" tab. Values are read from the game while untouched; the first write
    // captures the game's own value so "Reset" and unloading can put it back exactly.
    class MovementManager
    {
    public:
        static MovementManager& Get();

        void RenderImGuiCategory(MovementCategory category);
        void RandomizeAll();

        // Restores every property the trainer changed to the value the game had before
        void ResetAll();

    private:
        MovementManager();

        bool WriteProperty(PropertyItem& item, float value);
        void SyncFromGame(PropertyItem& item);

        std::mutex m_Mutex;
        std::vector<PropertyItem> m_Properties;
    };
}
