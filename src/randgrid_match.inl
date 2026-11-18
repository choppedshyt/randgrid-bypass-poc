// Exact-build offline admission for the independent local-match dependency.
// Installed once, after native restoration, alongside the persistent worker.
// Real native results and handles are retained; only C0000034 with no handle
// can select the verified, resource-free accepted bookkeeping continuation.
#include "randgrid_offline_state.hpp"
#include <windows.h>
#include <cstring>
#include "../../utils/minhook/include/MinHook.h"

extern "C" void RandgridMatchAdmission();
extern "C"
{
    randgrid_offline_state g_randgrid_match = {{'R','G','M','C','H','0','0','1'}, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}
static_assert(offsetof(randgrid_offline_state, positive) == 32);
static_assert(offsetof(randgrid_offline_state, failure) == 40);
static_assert(offsetof(randgrid_offline_state, last_status) == 56);

namespace randgrid_match
{
    struct preserve_last_error
    {
        DWORD value = GetLastError();
        ~preserve_last_error() noexcept { SetLastError(value); }
    };

    bool matches(std::uintptr_t address, const BYTE* expected, SIZE_T length) noexcept
    {
        BYTE actual[96]{};
        SIZE_T copied{};
        return length <= sizeof(actual)
            && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), actual, length, &copied)
            && copied == length && std::memcmp(actual, expected, length) == 0;
    }

    bool fail(LONG state, std::uintptr_t rva, int status = 0) noexcept
    {
        g_randgrid_match.enabled = 0;
        g_randgrid_match.install_state = state;
        try
        {
            console::print(std::format("offline Randgrid match admission: unavailable state={} RVA=0x{:X} status={}; native decision retained",
                state, rva, status), true);
        }
        catch (...) {}
        return false;
    }

    bool prepare(std::uintptr_t image) noexcept
    {
        const preserve_last_error restore_error;
        const std::uintptr_t positive = image + 0x15E8A23;
        const std::uintptr_t failure = image + 0x34BC0CF;
        BYTE status_bridge[] = {
            0x85,0xD2,0x41,0x50,0x49,0xB8,0,0,0,0,0,0,0,0,
            0xFF,0x34,0x24,0x4C,0x89,0x44,0x24,0x08,0x41,0x58,0x52,0x53,
            0x48,0x8B,0x54,0x24,0x10,0x48,0xBB,0,0,0,0,0,0,0,0,
            0x48,0x0F,0x48,0xD3,0x48,0x89,0x54,0x24,0x10,0x5B,0x5A,0xC3
        };
        static_assert(sizeof(status_bridge) == 53);
        std::memcpy(status_bridge + 6, &positive, sizeof(positive));
        std::memcpy(status_bridge + 33, &failure, sizeof(failure));
        const BYTE native_call[] = {0xFF,0xD6,0x90,0xE9,0xAD,0xCA,0xF0,0x1D};
        const BYTE cleanup[] = {0xF0,0x0F,0xBA,0x33,0x00,0x41,0x56,0x59,0xF3,0xAA};
        const BYTE object_arguments[] = {
            0x4C,0x8D,0x8D,0xB8,0x04,0x00,0x00,0x44,0x89,0x64,0x24,0x50,
            0x4C,0x8D,0x85,0x98,0x06,0x00,0x00,0x4C,0x89,0x64,0x24,0x48
        };
        const BYTE object_layout[] = {
            0x48,0x8D,0x85,0xC0,0x07,0x00,0x00,0xC7,0x85,0x98,0x06,0x00,0x00,0x30,0x00,0x00,0x00,
            0x41,0xBE,0x18,0x00,0x00,0x00,0x48,0x89,0x85,0xB0,0x04,0x00,0x00,0x45,0x33,0xE4,
            0x66,0x44,0x89,0xB5,0xA8,0x04,0x00,0x00,0x48,0x8D,0x85,0xA8,0x04,0x00,0x00,
            0x66,0x44,0x89,0xB5,0xAA,0x04,0x00,0x00,0xC5,0xF9,0xEF,0xC0,
            0x48,0x89,0x85,0xA8,0x06,0x00,0x00,0x4C,0x89,0xA5,0xA0,0x06,0x00,0x00,
            0x44,0x89,0xA5,0xB0,0x06,0x00,0x00
        };
        const BYTE bookkeeping[] = {
            0x48,0x8B,0x05,0xE8,0x86,0x34,0xEB,0x48,0xB9,0x39,0x5E,0xF9,0x48,0x6F,0xB9,0x46,0xEC,
            0x48,0x33,0xC1,0x48,0xB9,0x39,0x4E,0xA9,0x48,0x44,0x39,0x02,0x60,
            0x48,0x0B,0xC1,0x48,0x89,0x05,0xC7,0x86,0x34,0xEB
        };
        const BYTE lock_input[] = {0x33,0xC0,0xF0,0x44,0x0F,0xB1,0x2D,0x01,0x61,0x89,0xEC};
        const BYTE unlock[] = {0x41,0x8B,0xC4,0x87,0x05,0x05,0xB2,0xB6,0xEE};
        const BYTE rejection[] = {0xE9,0x46,0x56,0xF9,0x1B};
        const BYTE next_component[] = {0x90,0xE9,0x81,0xA8,0x1F,0x17};
        const BYTE accepted_lock_value[] = {0x41,0xBD,0x01,0x00,0x00,0x00};
        struct proof_site { std::uintptr_t rva; const BYTE* bytes; SIZE_T length; };
        const proof_site sites[] = {
            {0x213C2DD9, status_bridge, sizeof(status_bridge)},
            {0x34B62B3, native_call, sizeof(native_call)},
            {0x213C2DBB, cleanup, sizeof(cleanup)},
            {0x213D0D33, object_arguments, sizeof(object_arguments)},
            {0x2159E724, object_layout, sizeof(object_layout)},
            {0x1F5E09F1, bookkeeping, sizeof(bookkeeping)},
            {0x1F43B7BC, lock_input, sizeof(lock_input)},
            {0x1D1666BA, unlock, sizeof(unlock)},
            {0x34BC0CF, rejection, sizeof(rejection)},
            {0x34BE096, next_component, sizeof(next_component)},
            {0x1F4AD289, accepted_lock_value, sizeof(accepted_lock_value)}
        };
        for (const auto& site : sites)
        {
            if (!matches(image + site.rva, site.bytes, site.length))
                return fail(-1, site.rva);
        }
        HMODULE pinned{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&RandgridMatchAdmission), &pinned))
            return fail(-2, 0x213C2DD9, static_cast<int>(GetLastError()));

        g_randgrid_match.image = image;
        g_randgrid_match.positive = positive;
        g_randgrid_match.failure = failure;
        g_randgrid_match.bookkeeping = image + 0x1F5E09F1;
        const auto target = reinterpret_cast<void*>(image + 0x213C2DD9);
        void* trampoline{};
        const auto created = MH_CreateHook(target, reinterpret_cast<void*>(&RandgridMatchAdmission), &trampoline);
        if (created != MH_OK)
            return fail(-3, 0x213C2DD9, static_cast<int>(created));
        g_randgrid_match.enabled = 1;
        g_randgrid_match.install_state = 1;
        const auto enabled = MH_EnableHook(target);
        if (enabled != MH_OK)
        {
            MH_RemoveHook(target);
            return fail(-4, 0x213C2DD9, static_cast<int>(enabled));
        }
        try
        {
            console::print("offline Randgrid match admission: exact missing-device case enabled after native cleanup; persistent local-match policy ready");
        }
        catch (...) {}
        return true;
    }
}
