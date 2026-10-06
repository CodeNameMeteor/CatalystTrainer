#pragma once
#include <Windows.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <initializer_list>

namespace Core
{
    class Memory
    {
    public:
        // Get the base address of MirrorsEdgeCatalyst.exe
        static uintptr_t GetBaseAddress()
        {
            static const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
            return base;
        }

        // Rebase hardcoded 0x140000000-based addresses to current process ASLR base
        static inline uintptr_t Rebase(uintptr_t hardcodedAddress)
        {
            constexpr uintptr_t defaultBase = 0x140000000;
            return GetBaseAddress() + (hardcodedAddress - defaultBase);
        }

        // Global kill-switch for game memory writes (cleared when the game build is known to be unsupported)
        static void SetWritesEnabled(bool enabled) { s_WritesEnabled.store(enabled); }
        static bool AreWritesEnabled() { return s_WritesEnabled.load(); }

        // Validates if an address is inside valid user-mode memory
        static inline bool IsValidAddress(uintptr_t address)
        {
            return (address >= 0x10000 && address < 0x7FFFFFFEFFFF);
        }

        // Checks that [address, address + size) is committed, not a guard page, and readable (or writable)
        static bool IsAccessible(uintptr_t address, size_t size, bool write)
        {
            if (!IsValidAddress(address) || size == 0) return false;

            const uintptr_t last = address + size - 1;
            if (!IsPageAccessible(address, write)) return false;
            if ((last >> 12) != (address >> 12) && !IsPageAccessible(last, write)) return false;
            return true;
        }

        // Exception-safe memory read
        template <typename T>
        static T SafeRead(uintptr_t address, const T& defaultValue = T())
        {
            if (!IsAccessible(address, sizeof(T), false))
                return defaultValue;

            __try
            {
                T result{};
                std::memcpy(&result, reinterpret_cast<const void*>(address), sizeof(T));
                return result;
            }
            __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
            {
                return defaultValue;
            }
        }

        // Exception-safe memory write
        template <typename T>
        static bool SafeWrite(uintptr_t address, const T& value)
        {
            if (!AreWritesEnabled() || !IsAccessible(address, sizeof(T), true))
                return false;

            __try
            {
                std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
                return true;
            }
            __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
            {
                return false;
            }
        }

        // Safe multi-level pointer resolution
        static uintptr_t ResolvePtrChain(uintptr_t base, const unsigned int* offsets, size_t count)
        {
            if (!IsValidAddress(base))
                return 0;

            uintptr_t current = base;

            for (size_t i = 0; i < count; ++i)
            {
                current = SafeRead<uintptr_t>(current);
                if (!IsValidAddress(current))
                    return 0;

                current += offsets[i];
            }

            return IsValidAddress(current) ? current : 0;
        }

        static uintptr_t ResolvePtrChain(uintptr_t base, std::initializer_list<unsigned int> offsets)
        {
            return ResolvePtrChain(base, offsets.begin(), offsets.size());
        }

        static uintptr_t ResolvePtrChain(uintptr_t base, const std::vector<unsigned int>& offsets)
        {
            return ResolvePtrChain(base, offsets.data(), offsets.size());
        }

        // Resolves 64-bit RIP-relative instruction operands: Target = (InstructionAddr + InstructionLen) + *(int32_t*)(InstructionAddr + OffsetToDisplacement)
        static inline uintptr_t ResolveRelativeAddress(uintptr_t instructionAddress, int offsetToDisplacement, int instructionLength)
        {
            if (!IsAccessible(instructionAddress + offsetToDisplacement, sizeof(int32_t), false)) return 0;
            int32_t relativeOffset = SafeRead<int32_t>(instructionAddress + offsetToDisplacement);
            return instructionAddress + instructionLength + relativeOffset;
        }

        // AOB Pattern Scanner over executable sections (supports IDA-style signatures like "48 8B 05 ? ? ? ?").
        // Returns 0 for malformed patterns or when nothing matches.
        static uintptr_t FindPattern(const char* pattern, const char* moduleName = nullptr)
        {
            if (!pattern) return 0;

            HMODULE hMod = moduleName ? GetModuleHandleA(moduleName) : GetModuleHandleA(nullptr);
            if (!hMod) return 0;

            // Parse IDA pattern into bytes and wildcards (-1)
            std::vector<int> bytes;
            const char* current = pattern;
            while (*current)
            {
                if (*current == ' ') { ++current; continue; }

                if (*current == '?')
                {
                    bytes.push_back(-1);
                    ++current;
                    if (*current == '?') ++current;
                }
                else
                {
                    int hi = HexDigit(current[0]);
                    if (hi < 0) return 0; // Malformed pattern
                    int lo = HexDigit(current[1]);
                    if (lo < 0)
                    {
                        bytes.push_back(hi);
                        current += 1;
                    }
                    else
                    {
                        bytes.push_back(hi * 16 + lo);
                        current += 2;
                    }
                }

                if (*current != '\0' && *current != ' ') return 0; // Tokens must be space-separated
            }

            if (bytes.empty()) return 0;

            const uintptr_t imageBase = reinterpret_cast<uintptr_t>(hMod);
            auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(imageBase);
            if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return 0;
            auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(imageBase + dosHeader->e_lfanew);
            if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return 0;

            PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
            for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++section)
            {
                if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;

                const uintptr_t start = imageBase + section->VirtualAddress;
                const size_t size = section->Misc.VirtualSize;
                if (size < bytes.size()) continue;

                uintptr_t match = ScanRegion(reinterpret_cast<const uint8_t*>(start), size, bytes.data(), bytes.size());
                if (match) return match;
            }
            return 0;
        }

    private:
        inline static std::atomic<bool> s_WritesEnabled{ true };

        static bool IsPageAccessible(uintptr_t address, bool write)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (!VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)))
                return false;
            if (mbi.State != MEM_COMMIT)
                return false;
            if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
                return false;

            constexpr DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                       PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
            constexpr DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
            return (mbi.Protect & (write ? writable : readable)) != 0;
        }

        static int HexDigit(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }

        // Kept free of C++ objects so it can use SEH
        static uintptr_t ScanRegion(const uint8_t* start, size_t size, const int* pattern, size_t patternSize)
        {
            __try
            {
                for (size_t i = 0; i + patternSize <= size; ++i)
                {
                    size_t j = 0;
                    for (; j < patternSize; ++j)
                    {
                        if (pattern[j] != -1 && start[i + j] != static_cast<uint8_t>(pattern[j]))
                            break;
                    }
                    if (j == patternSize)
                        return reinterpret_cast<uintptr_t>(start + i);
                }
            }
            __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
            {
            }
            return 0;
        }
    };
}
