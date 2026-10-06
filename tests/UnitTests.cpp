// Small self-contained tests for logic that doesn't need the game running.
#include "Core/Config.hpp"
#include "Game/GameOffsets.hpp"
#include <cmath>
#include <cstdio>
#include <string>

static int g_Failures = 0;

#define CHECK(cond)                                                              \
    do                                                                           \
    {                                                                            \
        if (!(cond))                                                             \
        {                                                                        \
            std::printf("FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__);      \
            ++g_Failures;                                                        \
        }                                                                        \
    } while (0)

static bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.01f;
}

static void TestKeyNamesRoundTrip()
{
    using Core::ConfigManager;

    // Every bindable virtual-key code must load back exactly as it was saved
    for (int vk = 1; vk <= 254; ++vk)
    {
        std::string name = ConfigManager::GetKeyName(vk);
        int parsed = ConfigManager::ParseKeyName(name);
        if (parsed != vk)
        {
            std::printf("FAILED: VK 0x%02X saved as '%s' loads back as 0x%02X\n", vk, name.c_str(), parsed);
            ++g_Failures;
        }

        // Saved names must never contain characters the INI parser treats as comments
        CHECK(name.find_first_of(";#") == std::string::npos);
    }
}

static void TestParseKeyName()
{
    using Core::ConfigManager;

    CHECK(ConfigManager::ParseKeyName("insert") == VK_INSERT);
    CHECK(ConfigManager::ParseKeyName(" F8 ") == VK_F8);
    CHECK(ConfigManager::ParseKeyName("a") == 'A');
    CHECK(ConfigManager::ParseKeyName("7") == '7');
    CHECK(ConfigManager::ParseKeyName("0xBA") == 0xBA);
    CHECK(ConfigManager::ParseKeyName("up") == VK_UP);

    // Invalid or out-of-range values are rejected
    CHECK(ConfigManager::ParseKeyName("") == 0);
    CHECK(ConfigManager::ParseKeyName("-5") == 0);
    CHECK(ConfigManager::ParseKeyName("99999") == 0);
    CHECK(ConfigManager::ParseKeyName("0x1FF") == 0);
    CHECK(ConfigManager::ParseKeyName("12abc") == 0);
    CHECK(ConfigManager::ParseKeyName("NotAKey") == 0);
    CHECK(ConfigManager::ParseKeyName(";") == 0);
}

static void TestKeybindTable()
{
    const auto& table = Core::GetKeybindTable();
    CHECK(!table.empty());

    // Default bindings must not clash with each other
    Core::Keybinds defaults;
    for (size_t i = 0; i < table.size(); ++i)
    {
        for (size_t j = i + 1; j < table.size(); ++j)
        {
            int a = (defaults.*(table[i].member)).load();
            int b = (defaults.*(table[j].member)).load();
            if (a == b)
            {
                std::printf("FAILED: default keys for %s and %s are both 0x%02X\n", table[i].iniName, table[j].iniName, a);
                ++g_Failures;
            }
        }
    }
}

static void TestTimeOfDay()
{
    // Plain time of day
    CHECK(Near(Game::HourOfDay(3600.0f * 13.5f), 13.5f));
    CHECK(Near(Game::SetHourOfDay(3600.0f * 13.0f, 6.0f), 3600.0f * 6.0f));

    // Whole days are kept when the game stores elapsed time
    const float day = 86400.0f;
    CHECK(Near(Game::HourOfDay(2.0f * day + 3600.0f * 20.0f), 20.0f));
    CHECK(Near(Game::SetHourOfDay(2.0f * day + 3600.0f * 20.0f, 8.0f), 2.0f * day + 3600.0f * 8.0f));

    // Negative values wrap into 0..24
    CHECK(Near(Game::HourOfDay(-3600.0f), 23.0f));
}

int main()
{
    TestKeyNamesRoundTrip();
    TestParseKeyName();
    TestKeybindTable();
    TestTimeOfDay();

    if (g_Failures == 0)
        std::printf("All tests passed\n");
    return g_Failures == 0 ? 0 : 1;
}
