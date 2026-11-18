#pragma once
#include <cstdint>
#include <cstddef>

struct alignas(16) randgrid_offline_state
{
    char magic[8];
    volatile std::int32_t enabled;
    volatile std::int32_t install_state;
    volatile std::uint64_t admissions;
    std::uintptr_t image;
    std::uintptr_t positive;
    std::uintptr_t failure;
    std::uintptr_t bookkeeping;
    volatile std::int32_t last_status;
    std::uint32_t reserved;
};
static_assert(sizeof(randgrid_offline_state) == 64);
static_assert(offsetof(randgrid_offline_state, admissions) == 16);
static_assert(offsetof(randgrid_offline_state, bookkeeping) == 48);
extern "C" randgrid_offline_state g_randgrid_offline;
extern "C" void RandgridOfflineAdmission();
