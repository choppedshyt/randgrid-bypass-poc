#include "randgrid_offline_state.hpp"
#include <windows.h>
#include <cstdio>

extern "C"
{
    randgrid_offline_state g_randgrid_offline = {{'R','G','O','F','F','0','0','1'}, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int RandgridProbePositive();
    int RandgridProbeFailure();
    int RandgridProbeBookkeeping();
    int RandgridProbeInvoke(LONG status, LONG enabled, std::uint64_t r13, std::uint64_t* output);
    unsigned restore_calls{};
    bool restore_success = true;
    bool RandgridRestoreNativePredicate() noexcept
    {
        ++restore_calls;
        g_randgrid_offline.enabled = 0;
        g_randgrid_offline.install_state = restore_success ? 3 : -6;
        return restore_success;
    }
}

int main()
{
    g_randgrid_offline.positive = reinterpret_cast<std::uintptr_t>(&RandgridProbePositive);
    g_randgrid_offline.failure = reinterpret_cast<std::uintptr_t>(&RandgridProbeFailure);
    g_randgrid_offline.bookkeeping = reinterpret_cast<std::uintptr_t>(&RandgridProbeBookkeeping);
    struct test_case { ULONG status; LONG enabled; std::uint64_t r13; int expected; };
    const test_case cases[] = {
        {0xC0000034,1,0,30}, {0xC0000034,0,0,20}, {0xC0000034,1,1,20},
        {0,1,0,10}, {0x40000000,1,0,10}, {0x7FFFFFFF,1,0,10},
        {0x80000000,1,0,20}, {0xC0000022,1,0,20}, {0xC0000005,1,0,20},
        {0xFFFFFFFF,1,0,20}, {0xC0000034,2,0,20}, {1,0,0,10}
    };
    for (unsigned i = 0; i < _countof(cases); ++i)
    {
        std::uint64_t output[3]{};
        const auto& test = cases[i];
        SetLastError(0x5A17);
        const auto result = RandgridProbeInvoke(static_cast<LONG>(test.status), test.enabled, test.r13, output);
        const bool passed = result == test.expected && GetLastError() == 0x5A17
            && output[0] == (test.expected == 30 ? 1 : 0x1122334455667788ULL)
            && output[1] == test.r13 && output[2] == test.status;
        std::printf("CASE %u status=%08lX enabled=%ld r13=%llu route=%d PASS=%d\n", i, test.status, test.enabled, test.r13, result, passed);
        if (!passed)
        {
            return 1;
        }
    }
    if (g_randgrid_offline.admissions != 1 || static_cast<ULONG>(g_randgrid_offline.last_status) != 0xC0000034)
    {
        return 2;
    }
    restore_success = false;
    std::uint64_t failed_output[3]{};
    if (RandgridProbeInvoke(static_cast<LONG>(0xC0000034), 1, 0, failed_output) != 20
        || failed_output[0] != 0x1122334455667788ULL || g_randgrid_offline.admissions != 1 || restore_calls != 2)
    {
        return 3;
    }
    std::puts("RESTORATION_FAILURE_RETAINS_NATIVE_FAILURE_PASS");
    std::puts("RANDGRID_OFFLINE_POLICY_12_CASES_PASS");
    return 0;
}
