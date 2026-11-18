# stripped host integration examples

These are full, shortened versions of the two host translation units, derived
from the actual validated MWIII S2 Reloaded/NineEx DLL integration. They explain
how the eight files in [../src](../src) are wired into the original host.

| File | What is shown | Deliberately omitted |
| --- | --- | --- |
| [dllmain.cpp](dllmain.cpp) | DLL attach, bootstrap worker, exact four-INL include order, post-map initialization, and call into the normal host bootstrap | XInput forwarding, crash diagnostics, version/build configuration, termination-guard experiment |
| [game.cpp](game.cpp) | The host's proxy::on_attach entry point after native image restoration, with an optional reference to its original Steam import stage | Steam emulator implementation, Lua, GSC, weapon/fastfile hooks, IAT detours, process cleanup code |

## startup call flow

1. Windows loads the host proxy DLL and calls DllMain with DLL_PROCESS_ATTACH.
2. DllMain starts a worker and immediately returns; MinHook is never installed
   under the loader lock.
3. MainThread arms postmap_gate::initialize. It does not run normal host setup
   until the protected image reaches the verified native restoration boundary.
4. The post-map callback calls randgrid_offline::initialize and
   OriginalBootstrap; OriginalBootstrap initializes the host console and calls
   proxy::on_attach from the stripped game.cpp example.
5. When the game's later restoration step completes,
   RandgridRestoreNativePredicate disables/restores its one-shot main policy
   and prepares the independent recurring worker and local-match policies.
   Those additional hooks are not installed inside game.cpp.

## use with a matching host

These examples are **illustrations, not standalone replacements**. The host
must already provide console.hpp, game.hpp, its proxy entry points and normal
game dependencies. Copy all eight components into its
diagnostics/offline_startup directory and use the exact MSVC/MASM project items
documented in [../integration](../integration).

Do not compile examples/dllmain.cpp alongside an existing dllmain.cpp: both
would define DllMain, OriginalBootstrap and MainThread. Likewise, use one
game.cpp defining proxy::on_attach. The definition-bearing INL files must be
included in exactly one translation unit, in the displayed order.

The real tested host also ran other import/setup stages from game.cpp; they are
not part of the Randgrid concept and are intentionally not copied. If the
matching host requires the original Steam import setup, define
RANDGRID_EXAMPLE_KEEP_HOST_STEAM_SETUP and provide its steam.hpp and
steam::Component_Load implementation. This option is host-only, not a
dependency of the bypass itself.

The default post-map gate must remain enabled for this restored executable;
the explicit NINEEX_POSTMAP_BOOTSTRAP diagnostic override is shown only to
reflect the original code, not as a recommended setting. Hook address/RVA and
native-status guards remain exact-build-specific and cannot be reused blindly.

These files were syntax-checked against the existing authorized host headers.
They were **not compiled into a new shipping DLL or tested as a replacement
for the full project's game.cpp/dllmain.cpp**. See the original historical
runtime evidence in [../docs/VALIDATION.md](../docs/VALIDATION.md).
