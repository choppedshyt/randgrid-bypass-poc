// Stripped integration example based on the validated MWIII/NineEx host.
//
// This is the complete Randgrid-related DLL bootstrap, NOT a complete proxy
// implementation. The original host supplies console.hpp, game.hpp, MinHook,
// the x64 MASM modules, forwarded XInput exports and its other startup stages.
//
// Integrate this logic into the matching authorized host. Do not compile the
// example alongside the host's existing dllmain.cpp (duplicate DllMain).

#include <windows.h>
#include <format>

// These two headers are host-owned interfaces, not shipped with this PoC.
#include "console.hpp"
#include "game.hpp"

// Keep this precise order. The main policy calls worker::prepare and
// match::prepare after restoring the original native startup predicate.
// Include these definition-bearing INLs in exactly one translation unit.
#include "diagnostics/offline_startup/randgrid_worker.inl"
#include "diagnostics/offline_startup/randgrid_match.inl"
#include "diagnostics/offline_startup/randgrid_offline.inl"
#include "diagnostics/offline_startup/postmap_gate.inl"

DWORD WINAPI OriginalBootstrap(void* parameter)
{
    const auto module = static_cast<HMODULE>(parameter);

    // These calls are from the real host console interface. They run outside
    // DllMain after the native image-restoration boundary is reached.
    console::init_startup_log(module);
    console::init("randgrid offline poc");

    // In the real project this enters the host's normal import/setup stages.
    // It does NOT install a second Randgrid hook itself.
    proxy::on_attach();
    return 0;
}

DWORD WINAPI MainThread(void* parameter)
{
    const auto module = static_cast<HMODULE>(parameter);
    wchar_t setting[4]{};

    // Match the tested host's default: use post-map initialization unless the
    // operator explicitly selects another value. Bypassing this gate can make
    // the game's own image restoration overwrite early hooks.
    const auto count = GetEnvironmentVariableW(
        L"NINEEX_POSTMAP_BOOTSTRAP", setting, _countof(setting));
    const bool use_postmap = count == 0 || (count == 1 && setting[0] == L'1');

    if (use_postmap)
    {
        console::init_startup_log(module);
        const bool ready = postmap_gate::initialize(module, OriginalBootstrap);
        console::print(
            ready ? "randgrid: waiting for native image restoration"
                  : "randgrid: post-map gate failed; bootstrap not executed",
            !ready);
        return ready ? 0 : 95;
    }

    // Retained only to match the existing host's explicit diagnostic override.
    // This early route is NOT the validated default for the protected image.
    return OriginalBootstrap(parameter);
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        // Match the host's short loader-lock path: do not install MinHook,
        // initialize the console, or wait on the bootstrap thread in DllMain.
        DisableThreadLibraryCalls(module);
        const auto thread = CreateThread(
            nullptr, 0, MainThread, module, 0, nullptr);

        if (thread)
        {
            CloseHandle(thread);
        }
        else
        {
            OutputDebugStringA("randgrid: could not create bootstrap thread\n");
        }
    }

    // No NtTerminateProcess swallowing, global exit suppression or driver
    // service manipulation belongs in this example.
    return TRUE;
}
