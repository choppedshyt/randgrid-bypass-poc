// Serialize this offline proxy bootstrap on the observed native post-map
// cleanup call. It does not change a native API result or suppress exceptions.
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstring>
#include "../../utils/minhook/include/MinHook.h"

namespace postmap_gate
{
    using free_function = LONG (NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG);
    using bootstrap_function = DWORD (WINAPI*)(void*);
    free_function original{};
    bootstrap_function bootstrap{};
    HMODULE proxy{};
    std::uintptr_t image{};
    volatile LONG state{};

    bool read_exact(const void* address, void* destination, SIZE_T bytes) noexcept
    {
        SIZE_T copied{};
        return ReadProcessMemory(GetCurrentProcess(), address, destination, bytes, &copied) && copied == bytes;
    }

    bool supported_boundary(std::uintptr_t caller) noexcept
    {
        const BYTE expected[] = {0x48,0x8B,0x8D,0xB0,0x05,0x00,0x00,0x48,0x33,0xCC,0xE8,0x3C,0xA9,0xE8,0x07};
        BYTE actual[sizeof(expected)]{};
        IMAGE_DOS_HEADER dos{};
        IMAGE_NT_HEADERS64 nt{};
        return read_exact(reinterpret_cast<void*>(caller), actual, sizeof(actual))
            && std::memcmp(actual, expected, sizeof(expected)) == 0
            && read_exact(reinterpret_cast<void*>(image), &dos, sizeof(dos))
            && dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0 && dos.e_lfanew < 0x1000
            && read_exact(reinterpret_cast<void*>(image + dos.e_lfanew), &nt, sizeof(nt))
            && nt.Signature == IMAGE_NT_SIGNATURE && nt.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64
            && nt.FileHeader.TimeDateStamp == 0x65DFAB1F
            && nt.OptionalHeader.SizeOfImage >= 0x21BF5600 && nt.OptionalHeader.SizeOfImage <= 0x21BF6000;
    }

    __declspec(noinline) LONG NTAPI after_free(HANDLE process, PVOID* base, PSIZE_T size, ULONG type)
    {
        const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        const bool selected = InterlockedCompareExchange(&state, 0, 0) == 1
            && process == reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1))
            && type == MEM_RELEASE && caller == image + 0x6D2775;
        // A cleanup failure does not undo the preceding mapper success; preserve
        // both the original arguments and its exact return status.
        const auto result = original(process, base, size, type);
        const auto native_last_error = GetLastError();
        if (process == reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)) && type == MEM_RELEASE)
        {
            randgrid_offline::native_stage_complete(caller);
            if (caller == image + 0x41E01DE && g_randgrid_offline.reserved == 1
                && g_randgrid_offline.install_state == 1
                && InterlockedCompareExchange(&state, 5, 3) == 3)
            {
                const auto target = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtFreeVirtualMemory"));
                const auto retired = target ? MH_DisableHook(target) : MH_ERROR_FUNCTION_NOT_FOUND;
                if (retired == MH_OK)
                {
                    FlushInstructionCache(GetCurrentProcess(), target, 32);
                    InterlockedExchange(&state, 6);
                }
                else
                {
                    InterlockedExchange(&state, 7);
                }
                try
                {
                    console::print(std::format("post-map gate: startup-only native hook retired={} status={}; relay retained for in-flight return",
                        retired == MH_OK, static_cast<int>(retired)), retired != MH_OK);
                }
                catch (...)
                {
                    // Logging must not undo the completed native operation.
                }
            }
        }
        if (selected && InterlockedCompareExchange(&state, 2, 1) == 1)
        {
            const auto saved_error = native_last_error;
            if (!supported_boundary(caller))
            {
                state = 4;
                console::print("post-map gate: native boundary/header verification failed; bootstrap not executed", true);
                SetLastError(saved_error);
                return result;
            }
            // Read-only diagnostic of the x64 PEB loader-lock field. This does not
            // acquire that lock or modify Windows loader state.
            const auto peb = static_cast<std::uintptr_t>(__readgsqword(0x60));
            RTL_CRITICAL_SECTION* loader_lock{};
            RTL_CRITICAL_SECTION lock_snapshot{};
            if (!read_exact(reinterpret_cast<void*>(peb + 0x110), &loader_lock, sizeof(loader_lock))
                || !loader_lock || !read_exact(loader_lock, &lock_snapshot, sizeof(lock_snapshot)))
            {
                state = 4;
                console::print("post-map gate: loader-lock state unavailable; bootstrap not executed", true);
                SetLastError(saved_error);
                return result;
            }
            const auto owner = reinterpret_cast<std::uintptr_t>(lock_snapshot.OwningThread);
            console::print(std::format("post-map gate: restored image boundary tid={} free_status=0x{:08X} loader_owner={} recursion={}",
                GetCurrentThreadId(), static_cast<DWORD>(result), owner, lock_snapshot.RecursionCount));
            if (owner == GetCurrentThreadId())
            {
                state = 4;
                console::print("post-map gate: caller owns loader lock; bootstrap remains deferred", true);
                SetLastError(saved_error);
                return result;
            }
            try
            {
                randgrid_offline::initialize();
                bootstrap(proxy);
                InterlockedExchange(&state, 3);
                console::print("post-map gate: bootstrap returned on native restoration thread");
            }
            catch (...)
            {
                InterlockedExchange(&state, 4);
                SetLastError(saved_error);
                throw;
            }
            SetLastError(saved_error);
        }
        SetLastError(native_last_error);
        return result;
    }

    bool initialize(HMODULE module, bootstrap_function function) noexcept
    {
        proxy = module;
        bootstrap = function;
        image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const auto target = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtFreeVirtualMemory"));
        const auto status = MH_Initialize();
        if (!target || !image || !function || (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED))
        {
            return false;
        }
        void* trampoline{};
        if (MH_CreateHook(target, reinterpret_cast<void*>(&after_free), &trampoline) != MH_OK)
        {
            return false;
        }
        original = reinterpret_cast<free_function>(trampoline);
        InterlockedExchange(&state, 1);
        if (MH_EnableHook(target) != MH_OK)
        {
            InterlockedExchange(&state, 4);
            MH_RemoveHook(target);
            return false;
        }
        return true;
    }
}
