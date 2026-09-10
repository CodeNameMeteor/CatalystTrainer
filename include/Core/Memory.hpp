#pragma once
#include <Windows.h>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace Core
{
    class Memory
    {
    public:
        // Get the base address of MirrorsEdgeCatalyst.exe
        static uintptr_t GetBaseAddress()
        {
            static uintptr_t base = 0;
            if (!base)
            {
                base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
            }
            return base;
        }

        // Rebase hardcoded 0x140000000-based addresses to current process ASLR base
        static inline uintptr_t Rebase(uintptr_t hardcodedAddress)
        {
            constexpr uintptr_t defaultBase = 0x140000000;
            return GetBaseAddress() + (hardcodedAddress - defaultBase);
        }

        // Validates if an address is inside valid user-mode memory
        static inline bool IsValidAddress(uintptr_t address)
        {
            return (address >= 0x10000 && address < 0x7FFFFFFFFFFF);
        }

#include <cstring>

        // Exception-safe memory read
        template <typename T>
        static T SafeRead(uintptr_t address, const T& defaultValue = T())
        {
            if (!IsValidAddress(address))
                return defaultValue;

            __try
            {
                T result{};
                std::memcpy(&result, reinterpret_cast<const void*>(address), sizeof(T));
                return result;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return defaultValue;
            }
        }

        // Exception-safe memory write
        template <typename T>
        static bool SafeWrite(uintptr_t address, const T& value)
        {
            if (!IsValidAddress(address))
                return false;

            __try
            {
                std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // Safe multi-level pointer resolution
        static uintptr_t ResolvePtrChain(uintptr_t base, const std::vector<unsigned int>& offsets)
        {
            if (!IsValidAddress(base))
                return 0;

            uintptr_t current = base;

            for (size_t i = 0; i < offsets.size(); ++i)
            {
                if (!IsValidAddress(current))
                    return 0;

                current = SafeRead<uintptr_t>(current);
                if (!IsValidAddress(current))
                    return 0;

                current += offsets[i];
            }

            return IsValidAddress(current) ? current : 0;
        }

        // Resolves 64-bit RIP-relative instruction operands: Target = (InstructionAddr + InstructionLen) + *(int32_t*)(InstructionAddr + OffsetToDisplacement)
        static inline uintptr_t ResolveRelativeAddress(uintptr_t instructionAddress, int offsetToDisplacement, int instructionLength)
        {
            if (!IsValidAddress(instructionAddress)) return 0;
            int32_t relativeOffset = SafeRead<int32_t>(instructionAddress + offsetToDisplacement);
            return instructionAddress + instructionLength + relativeOffset;
        }

        // Fast AOB Pattern Scanner (supports IDA-style signatures like "48 8B 05 ? ? ? ?")
        static uintptr_t FindPattern(const char* pattern, const char* moduleName = nullptr)
        {
            HMODULE hMod = moduleName ? GetModuleHandleA(moduleName) : GetModuleHandleA(nullptr);
            if (!hMod) return 0;

            auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hMod);
            if (!dosHeader || dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return 0;
            auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uintptr_t>(hMod) + dosHeader->e_lfanew);
            if (!ntHeaders || ntHeaders->Signature != IMAGE_NT_SIGNATURE) return 0;

            uintptr_t startAddress = reinterpret_cast<uintptr_t>(hMod);
            uintptr_t endAddress = startAddress + ntHeaders->OptionalHeader.SizeOfImage;

            // Parse IDA pattern into bytes and wildcards
            std::vector<int> bytes;
            const char* current = pattern;
            while (*current)
            {
                if (*current == ' ') { ++current; continue; }
                if (*current == '?')
                {
                    bytes.push_back(-1); // Wildcard
                    ++current;
                    if (*current == '?') ++current;
                }
                else
                {
                    bytes.push_back(static_cast<int>(strtoul(current, const_cast<char**>(&current), 16)));
                }
            }

            if (bytes.empty()) return 0;

            const size_t patternSize = bytes.size();
            const uint8_t* scanStart = reinterpret_cast<const uint8_t*>(startAddress);
            const size_t scanLen = endAddress - startAddress - patternSize;

            for (size_t i = 0; i < scanLen; ++i)
            {
                bool found = true;
                for (size_t j = 0; j < patternSize; ++j)
                {
                    if (bytes[j] != -1 && scanStart[i + j] != static_cast<uint8_t>(bytes[j]))
                    {
                        found = false;
                        break;
                    }
                }
                if (found)
                {
                    return startAddress + i;
                }
            }
            return 0;
        }
    };
}
