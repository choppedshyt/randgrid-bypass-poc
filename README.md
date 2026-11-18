
# Randgrid bypass PoC ? authorized offline build

This repository contains a narrowly scoped Randgrid presence-check proof of concept extracted from an authorized offline MWIII / NineEx experiment. It contains **only the focused source components, host integration diffs, selected tests and documentation**. It is not the MWIII DLL codebase, a downloadable game, a driver, or a complete buildable client.

## Mechanism

The studied game uses a native open of the Windows object named \??\Randgrid. Without that object, the actual system call returns 0xC0000034 (STATUS_OBJECT_NAME_NOT_FOUND). The PoC changes application-level acceptance paths **after authentic cleanup**, not the Windows call result.

- **Startup:** src/randgrid_offline.asm and .inl handle the initial missing-object decision and restore the overwritten native instruction stream after the one-off use.
- **Recurring worker:** src/randgrid_worker.asm and .inl handle subsequent checks using only an application-level Boolean while leaving native status codes intact.
- **System Link / local match:** src/randgrid_match.asm and .inl handle a separate check when a match starts, with explicit full-width register and null-handle gates. It resumes the native bookkeeping path without inventing a driver handle or IOCTL reply.
- **Bootstrap coordination:** src/postmap_gate.inl defers the proxy's existing startup work until native image restoration is complete. src/randgrid_offline_state.hpp provides the shared policy state layout.

The match continuation is specific to the analyzed build. Every unrecognized native byte sequence must retain its original decision. The earlier experiment that swallowed NtTerminateProcess(-1, 0) is **not** part of this PoC.

## Repository contents

- **src/**: eight Randgrid-only C++ / MASM component files.
- **integration/**: three cumulative patches showing the host integration points. These are not copies of the host files.
- **tests/source/**: native main/worker policy tests and the isolated Unicorn/Capstone match state-contract validator.
- **tests/receipts/**: small historical test receipts and a summary of the prior match validation.
- **docs/**: technical scope, setup expectations, and validation limitations.

## Dependencies and compatibility

This is **not a standalone DLL project**. To compile and integrate the eight components, you need the matching authorized MWIII/NineEx host source tree, its generated inputs and logging/console interfaces, and the following build dependencies:

### Required for the native C++/MASM integration

| Dependency | Why it is needed | Upstream / download |
| --- | --- | --- |
| **Windows x64 + Windows SDK** | Provides `windows.h`, Win32/NT data types, system import libraries and related headers. | [Windows SDK (Microsoft)](https://learn.microsoft.com/en-us/windows/apps/windows-sdk/) |
| **Visual Studio C++ Build Tools (MSVC)** | x64 `cl.exe`/`link.exe` and a **C++20** standard library (including `std::format` used by the host integration). Use the **Desktop development with C++** workload. | [Visual Studio downloads](https://visualstudio.microsoft.com/downloads/) / [C++ workload components](https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=visualstudio) |
| **Microsoft MASM x64 (`ml64.exe`)** | Assembles `randgrid_offline.asm`, `randgrid_worker.asm` and `randgrid_match.asm`. Installed with the Visual Studio C++ toolchain, not separately. | [MASM for x64 documentation](https://learn.microsoft.com/en-us/cpp/assembler/masm/masm-for-x64-ml64-exe?view=msvc-170) |
| **MSBuild** | Processes the host `.vcxproj` and its `masm.props`/`masm.targets` integration. Installed through Visual Studio/Build Tools. | [dotnet/msbuild on GitHub](https://github.com/dotnet/msbuild) |
| **MinHook** | Supplies `MinHook.h` and the compiled `MH_Initialize`, `MH_CreateHook`, `MH_EnableHook` and `MH_DisableHook` implementations. | [TsudaKageyu/minhook on GitHub](https://github.com/TsudaKageyu/minhook) |

The source files expect MinHook at `utils/minhook/include/MinHook.h` **relative to the original host project**, so clone/vendor and **build/link** MinHook as the host expects; just copying its header is insufficient. [Microsoft vcpkg](https://github.com/microsoft/vcpkg) can optionally acquire MinHook, but using it requires adapting the existing host's include/link configuration.

**Not supplied here:** the original host DLL project (`dllmain.cpp`, `game.cpp`, the full `.vcxproj`, console support, generated headers/inputs, proxy exports and pre-existing third-party libraries). For example, the historical project patch still lists `discord_game_sdk.dll.lib`: that is a **host-project dependency**, *not* a new dependency of these Randgrid source components. Discord's [legacy Game SDK documentation repository](https://github.com/discord/discord-api-docs/blob/main/developers/developer-tools/game-sdk.mdx) describes the SDK; it has been archived. The original host must satisfy its own build/link dependencies independently. The MSVC compiler, MASM and Windows SDK are Microsoft-provided toolchain components rather than standalone upstream GitHub source repositories.

### Required only to run the isolated Python validator

| Dependency | Purpose | GitHub repository |
| --- | --- | --- |
| **Python 3 + pip** | Runs `tests/source/validate_randgrid_match.py`. | [python/cpython](https://github.com/python/cpython) |
| **Capstone** (`capstone`) | Decodes and verifies assembled x64 instructions. | [capstone-engine/capstone](https://github.com/capstone-engine/capstone) |
| **Unicorn** (`unicorn`) | Emulates the hook in isolated x64 memory and checks state preservation. | [unicorn-engine/unicorn](https://github.com/unicorn-engine/unicorn) |
| **pefile** (`pefile`) | Inspects matching PE/COFF inputs and linked DLL identity. | [erocarrera/pefile](https://github.com/erocarrera/pefile) |

Install the Python packages separately from the native DLL build:

```powershell
python -m pip install capstone unicorn pefile
```

The validator also needs **a matching assembled COFF `.obj`, the matching built DLL, and an authorized native reference image**; these are test inputs, not pip packages, and are intentionally excluded from this repository. See [tests/README.md](tests/README.md) for arguments and scope. IDA Pro and x64dbg are **not required** to build the components or run the validator.

The RVAs, native byte checks and stack offsets are **not portable to other game versions**. No game executable, binary dump, driver, asset, PDB, compiled proxy DLL or complete host source tree is distributed here.

## Previously observed result

The historical release was observed entering two local-host Invasion Team Deathmatch games, including a return to the frontend and a successful fresh local match. One earlier attempt returned to lobby with a separate DEV ERROR 6145 capacity condition; this PoC did not modify capacity or GSC checks. A later status-zero exit was reported by the operator after attaching x64dbg and is excluded from standalone runtime validation.

The historical DLL SHA-256 is 412890b9e929a229e9a7303f89cfd36b1569ac47566147ee0caece1bb5fae10b. The DLL itself is **not shipped**, nor is the historical result a claim that a new checkout was compiled and independently tested.

No physical second LAN peer, other maps/modes, arbitrary player counts or online play was validated. Use only on builds you are authorized to modify.

No open-source license has been selected for this repository; third-party rights remain with their respective holders.
