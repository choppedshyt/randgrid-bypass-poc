#include "randgrid_offline_state.hpp"
#include <windows.h>
#include <cstdio>

extern "C"
{
    randgrid_offline_state g_randgrid_worker = {{'R','G','W','R','K','0','0','1'}, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int WorkerProbePositive();
    int WorkerProbeFailure();
    int WorkerProbeInvoke(ULONG status, ULONG original_boolean, LONG enabled, void* output);
    bool restore_success = true;
    unsigned restore_calls{};
    bool RandgridRestoreWorkerPredicate() noexcept
    {
        ++restore_calls;
        g_randgrid_worker.enabled = 0;
        g_randgrid_worker.install_state = restore_success ? 3 : -6;
        return restore_success;
    }
}

int main()
{
    g_randgrid_worker.positive = reinterpret_cast<std::uintptr_t>(&WorkerProbePositive);
    g_randgrid_worker.failure = reinterpret_cast<std::uintptr_t>(&WorkerProbeFailure);
    g_randgrid_worker.bookkeeping = g_randgrid_worker.positive;
    struct test_case { ULONG status; ULONG boolean; LONG enabled; int route; ULONG final_boolean; };
    const test_case cases[] = {
        {0xC0000034,0,1,10,1}, {0xC0000034,0,0,20,0}, {0xC0000034,1,1,10,1},
        {0,0,1,20,0}, {0,1,1,10,1}, {0xC0000022,0,1,20,0},
        {0xC0000005,0,1,20,0}, {0xC0000034,0,2,20,0}, {1,2,1,10,2}
    };
    struct output_state { ULONG boolean; ULONG status; std::uint64_t r12; std::uint64_t r13; };
    for (unsigned index = 0; index != _countof(cases); ++index)
    {
        const auto& test = cases[index];
        output_state output{};
        SetLastError(0x5A17);
        const auto result = WorkerProbeInvoke(test.status, test.boolean, test.enabled, &output);
        const bool passed = result == test.route && output.boolean == test.final_boolean
            && output.status == test.status && output.r12 == test.status && output.r13 == test.boolean
            && GetLastError() == 0x5A17;
        std::printf("CASE %u status=%08lX original=%lu enabled=%ld route=%d result=%lu PASS=%d\n", index, test.status,
            test.boolean, test.enabled, result, output.boolean, passed);
        if (!passed)
        {
            return 1;
        }
    }
    for (unsigned repeat = 0; repeat != 16; ++repeat)
    {
        output_state output{};
        if (WorkerProbeInvoke(0xC0000034, 0, 1, &output) != 10 || output.boolean != 1
            || output.status != 0xC0000034 || output.r12 != 0xC0000034 || output.r13 != 0)
        {
            return 2;
        }
    }
    if (restore_calls != 0 || g_randgrid_worker.admissions != 17)
    {
        return 3;
    }
    std::puts("WORKER_POLICY_9_CASES_AND_16_REPEAT_INVOCATIONS_PASS_NO_API_HELPER");
    return 0;
}
