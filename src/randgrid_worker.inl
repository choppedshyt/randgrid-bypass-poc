// The independent background dependency has its own native cleanup and Boolean
// decision. This admission never invents a device handle or an IOCTL response.
#include "randgrid_offline_state.hpp"
#include <windows.h>
#include <cstring>
#include "../../utils/minhook/include/MinHook.h"

extern "C" void RandgridWorkerAdmission();
extern "C"
{
    randgrid_offline_state g_randgrid_worker = {{'R','G','W','R','K','0','0','1'}, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

namespace randgrid_worker
{
    constexpr BYTE original[] = {0x0F,0xB6,0x84,0x24,0xEE,0x09,0x00,0x00,0xE9,0x32,0xA8,0xB8,0xE7};

    bool matches(std::uintptr_t address, const BYTE* expected, SIZE_T length) noexcept
    {
        BYTE actual[32]{};
        SIZE_T copied{};
        return length <= sizeof(actual)
            && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), actual, length, &copied)
            && copied == length && std::memcmp(actual, expected, length) == 0;
    }

    bool prepare(std::uintptr_t image) noexcept
    {
        const auto saved_error = GetLastError();
        const BYTE native_call[] = {0xFF,0x94,0x24,0x00,0xF6,0x05,0x00,0x90,0xE9,0x9E,0xA5,0x26,0x16};
        const BYTE object_argument[] = {0x4C,0x8D,0x84,0x24,0xE8,0x1E,0x06,0x00,0xE9,0x9E,0x1C,0xB0,0xE7};
        const BYTE result_test[] = {0x85,0xC0,0xE9,0x71,0x94,0x7C,0x20};
        const BYTE bookkeeping[] = {0xE9,0xD5,0xA1,0x7C,0x00};
        bool ready = false;
        if (matches(image + 0x1925C3E5, original, sizeof(original))
            && matches(image + 0xDC65C4, native_call, sizeof(native_call))
            && matches(image + 0x192C490C, object_argument, sizeof(object_argument))
            && matches(image + 0xDE6C24, result_test, sizeof(result_test))
            && matches(image + 0xDED05A, bookkeeping, sizeof(bookkeeping)))
        {
            g_randgrid_worker.image = image;
            g_randgrid_worker.positive = image + 0xDED05A;
            g_randgrid_worker.failure = image + 0xDE6C2C;
            g_randgrid_worker.bookkeeping = image + 0xDED05A;
            void* trampoline{};
            const auto target = reinterpret_cast<void*>(image + 0x1925C3E5);
            const auto created = MH_CreateHook(target, reinterpret_cast<void*>(&RandgridWorkerAdmission), &trampoline);
            if (created == MH_OK)
            {
                g_randgrid_worker.enabled = 1;
                g_randgrid_worker.install_state = 1;
                const auto enabled = MH_EnableHook(target);
                if (enabled == MH_OK)
                {
                    ready = true;
                }
                else
                {
                    g_randgrid_worker.enabled = 0;
                    g_randgrid_worker.install_state = -3;
                    MH_RemoveHook(target);
                }
            }
            else
            {
                g_randgrid_worker.install_state = -2;
            }
        }
        else
        {
            g_randgrid_worker.install_state = -1;
        }
        SetLastError(saved_error);
        return ready;
    }
}
