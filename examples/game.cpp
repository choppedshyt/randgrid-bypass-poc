// Stripped integration example based on the validated MWIII/NineEx host.
//
// The important distinction: the Randgrid admission hooks are coordinated
// by examples/dllmain.cpp and the four INLs, not by a second hook in game.cpp.
// The actual host's other Steam, Lua, GSC, fastfile and import hooks are
// intentionally excluded. Keep any required host stages in your real build.

#include <windows.h>
#include <format>
#include <string>

// These interfaces are provided by the matching host project.
#include "console.hpp"
#include "game.hpp"

// Optional example of the original host's Steam import stage. It is NOT part
// of Randgrid. Define this only if your matching host requires that stage and
// still supplies steam.hpp and steam::Component_Load().
#if defined(RANDGRID_EXAMPLE_KEEP_HOST_STEAM_SETUP)
#include "steam.hpp"
#endif

void proxy::on_attach()
{
    // Called by OriginalBootstrap only after the native post-map gate has
    // selected the verified image-restoration boundary.
    const auto image = GetModuleHandleW(nullptr);
    if (!image)
    {
        console::print("startup: game image is unavailable", true);
        return;
    }

    console::print(std::format(
        "startup: normal host bootstrap resumed at image {}",
        static_cast<const void*>(image)));

    // Randgrid's initial policy was installed by postmap_gate::after_free.
    // The one-shot native restoration helper subsequently prepares the
    // recurring worker and local-match policies, once the game restores its
    // image. Do not install those hooks again inside proxy::on_attach().

#if defined(RANDGRID_EXAMPLE_KEEP_HOST_STEAM_SETUP)
    // Host-owned initialization retained as an optional illustration.
    // The original tested host called steam::Component_Load() here.
    steam::Component_Load();
#endif

    // Insert any OTHER existing host startup stages here in their original
    // order. They are not provided by this Randgrid-only PoC.
    console::print("startup: stripped host example completed");
}
