// Exact-build offline component admission, enabled by default for this offline DLL.
// Set NINEEX_RANDGRID_OFFLINE_ADMISSION=0 to select the native dependency decision.
#include "randgrid_offline_state.hpp"
#include <windows.h>
#include <cstring>
#include "../../utils/minhook/include/MinHook.h"

extern "C"
{
    randgrid_offline_state g_randgrid_offline = {{'R','G','O','F','F','0','0','1'}, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

namespace randgrid_offline
{
    struct preserve_last_error
    {
        DWORD value = GetLastError();
        ~preserve_last_error() noexcept { SetLastError(value); }
    };
    constexpr BYTE native_predicate[] = {0x0F,0x88,0xC8,0x7D,0x5D,0xE5,0xE9,0xD6,0x20,0x5D,0xE5};

    bool matches(std::uintptr_t address, const BYTE* expected, SIZE_T length) noexcept
    {
        BYTE actual[32]{};
        SIZE_T read{};
        return length <= sizeof(actual)
            && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), actual, length, &read)
            && read == length && std::memcmp(actual, expected, length) == 0;
    }

    bool initialize() noexcept
    {
        wchar_t setting[4]{};
        const auto count = GetEnvironmentVariableW(L"NINEEX_RANDGRID_OFFLINE_ADMISSION", setting, _countof(setting));
        if (count != 0 && (count != 1 || setting[0] != L'1'))
        {
            console::print("offline Randgrid admission: disabled; native dependency decision retained");
            return false;
        }
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const BYTE status_test[] = {0x45,0x85,0xC0,0xE9,0x20,0xDF,0xA2,0x1A};
        const BYTE object_argument[] = {0x4C,0x8D,0x85,0xC0,0x07,0x00,0x00,0xE9,0x2D,0x19,0xCA,0x02};
        const BYTE native_call[] = {0xFF,0xD6,0x8B,0x53,0x04,0xE9,0x40,0x08,0x37,0x1D};
        const BYTE bookkeeping[] = {0xE9,0x7F,0x6A,0x5A,0x01};
        const BYTE cleanup[] = {0xF0,0x0F,0xBA,0x33,0x00,0xE9,0xD0,0xBC,0xC0,0xE9};
        const BYTE next_component[] = {0x0F,0x31,0x48,0xC1,0xE2,0x20,0xE9,0x1B,0x57,0x44,0x16};
        struct proof_site
        {
            std::uintptr_t rva;
            const BYTE* bytes;
            SIZE_T length;
        };
        const proof_site sites[] = {
            {0x1EC630F4, native_predicate, sizeof(native_predicate)}, {0x42351CC, status_test, sizeof(status_test)},
            {0x1593838, object_argument, sizeof(object_argument)}, {0x423519E, native_call, sizeof(native_call)},
            {0x423AAB0, bookkeeping, sizeof(bookkeeping)}, {0x1A6294F0, cleanup, sizeof(cleanup)},
            {0x423CDE6, next_component, sizeof(next_component)}
        };
        for (const auto& site : sites)
        {
            if (!matches(image + site.rva, site.bytes, site.length))
            {
                g_randgrid_offline.install_state = -1;
                console::print(std::format("offline Randgrid admission: exact runtime proof mismatch at RVA 0x{:X}; no patch installed", site.rva), true);
                return false;
            }
        }
        HMODULE pinned{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&RandgridOfflineAdmission), &pinned))
        {
            g_randgrid_offline.install_state = -2;
            return false;
        }
        g_randgrid_offline.image = image;
        g_randgrid_offline.positive = image + 0x42351D5;
        g_randgrid_offline.failure = image + 0x423AEC2;
        g_randgrid_offline.bookkeeping = image + 0x423AAB0;
        const auto target = reinterpret_cast<void*>(image + 0x1EC630F4);
        void* trampoline{};
        const auto created = MH_CreateHook(target, reinterpret_cast<void*>(&RandgridOfflineAdmission), &trampoline);
        if (created != MH_OK)
        {
            g_randgrid_offline.install_state = -3;
            console::print(std::format("offline Randgrid admission: hook creation failed ({})", static_cast<int>(created)), true);
            return false;
        }
        g_randgrid_offline.enabled = 1;
        const auto enabled = MH_EnableHook(target);
        if (enabled != MH_OK)
        {
            g_randgrid_offline.enabled = 0;
            g_randgrid_offline.install_state = -4;
            MH_RemoveHook(target);
            console::print(std::format("offline Randgrid admission: hook enable failed ({})", static_cast<int>(enabled)), true);
            return false;
        }
        g_randgrid_offline.install_state = 1;
        console::print("offline Randgrid admission: exact missing-device case enabled after native cleanup; kernel handles and replies remain unchanged");
        return true;
    }

    void native_stage_complete(std::uintptr_t caller) noexcept
    {
        const preserve_last_error restore_error;
        const auto image = g_randgrid_offline.image;
        // The captured guard initialization performs its own restore pass after
        // the image mapper. Observe only its native MEM_RELEASE return, once.
        if (!image || caller != image + 0x41E01DE
            || g_randgrid_offline.enabled != 1 || g_randgrid_offline.reserved != 0
            || g_randgrid_offline.admissions != 0
            || !matches(image + 0x1EC630F4, native_predicate, sizeof(native_predicate)))
        {
            return;
        }
        if (InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&g_randgrid_offline.install_state), 2, 1) != 1)
        {
            return;
        }
        // Recheck after acquiring publication ownership, so a racing caller
        // cannot repeat an already completed restoration transaction.
        if (g_randgrid_offline.reserved != 0 || g_randgrid_offline.admissions != 0
            || !matches(image + 0x1EC630F4, native_predicate, sizeof(native_predicate)))
        {
            g_randgrid_offline.install_state = 1;
            return;
        }
        g_randgrid_offline.reserved = 1;
        const auto target = reinterpret_cast<void*>(image + 0x1EC630F4);
        const auto disabled = MH_DisableHook(target);
        const auto enabled = disabled == MH_OK ? MH_EnableHook(target) : disabled;
        if (enabled == MH_OK)
        {
            g_randgrid_offline.install_state = 1;
            console::print(std::format("offline Randgrid admission: re-armed after native restore completion at caller RVA 0x{:X}", caller - image));
        }
        else
        {
            g_randgrid_offline.enabled = 0;
            g_randgrid_offline.install_state = -5;
            console::print(std::format("offline Randgrid admission: native restore re-arm failed ({}) at RVA 0x{:X}; original decision retained",
                static_cast<int>(enabled), caller - image), true);
        }
    }
}

extern "C" bool RandgridRestoreNativePredicate() noexcept
{
    const randgrid_offline::preserve_last_error restore_error;
    if (InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&g_randgrid_offline.install_state), 4, 1) != 1)
    {
        return false;
    }
    g_randgrid_offline.enabled = 0;
    const auto target = reinterpret_cast<void*>(g_randgrid_offline.image + 0x1EC630F4);
    const auto disabled = MH_DisableHook(target);
    // Keep the relay and trampoline allocated while this invocation uses them.
    // Flush and verify the original instruction stream before continuing.
    const bool restored = disabled == MH_OK
        && FlushInstructionCache(GetCurrentProcess(), target, sizeof(randgrid_offline::native_predicate))
        && randgrid_offline::matches(reinterpret_cast<std::uintptr_t>(target),
            randgrid_offline::native_predicate, sizeof(randgrid_offline::native_predicate));
    const bool worker_ready = restored && randgrid_worker::prepare(g_randgrid_offline.image);
    // The match check is independent. A failed exact-build proof leaves that
    // decision native and logged without changing the established startup policy.
    if (restored)
        randgrid_match::prepare(g_randgrid_offline.image);
    g_randgrid_offline.install_state = !restored ? -6 : worker_ready ? 3 : -7;
    return restored && worker_ready;
}
