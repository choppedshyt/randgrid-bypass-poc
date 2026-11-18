
# Integration boundary

This folder contains three small **cumulative patches**, not an MWIII DLL source tree.

For complete but stripped-down illustrative game.cpp and dllmain.cpp files,
see [../examples](../examples/README.md). They explain the actual host startup
order without copying unrelated Steam/Lua/GSC/weapon systems or replacing the
original integration diffs.

- dllmain.cpp.patch shows the include order and the image-restoration startup gate.
- dll.vcxproj.patch adds x64 MASM and source items to the existing project.
- game.cpp.patch preserves the earlier startup safety adjustment, avoiding termination of unrelated running game instances.

Install the eight source components under diagnostics/offline_startup in a matching authorized host project. The host must already contain MinHook, console support, Windows SDK, and the required project build configuration.

The cumulative patches are based on the original pre-PoC source snapshot, not a universal game version. Check each hunk manually and verify runtime byte patterns before use. Do not reapply to an already integrated project. Line endings in the historical host and in these public patches may differ.

This repository is deliberately missing all other host source files, assets, fastfiles, compiled DLLs and private game binaries.
