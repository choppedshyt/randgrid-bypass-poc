
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

These files are components for the matching authorized host project, not a standalone DLL. The original project supplies Windows SDK, MinHook, console/logging interfaces, x64 MSVC/MASM and generated build inputs. The integration diffs are based on a specific pre-PoC project snapshot and should not be applied blindly to another revision.

The RVAs, native byte checks and stack offsets are **not portable to other game versions**. No game executable, binary dump, driver, asset, PDB, compiled proxy DLL or complete host source tree is distributed here.

For tests, see tests/README.md. This repo does not include the private native image or any object/binary needed for the full validation harness.

## Previously observed result

The historical release was observed entering two local-host Invasion Team Deathmatch games, including a return to the frontend and a successful fresh local match. One earlier attempt returned to lobby with a separate DEV ERROR 6145 capacity condition; this PoC did not modify capacity or GSC checks. A later status-zero exit was reported by the operator after attaching x64dbg and is excluded from standalone runtime validation.

The historical DLL SHA-256 is 412890b9e929a229e9a7303f89cfd36b1569ac47566147ee0caece1bb5fae10b. The DLL itself is **not shipped**, nor is the historical result a claim that a new checkout was compiled and independently tested.

No physical second LAN peer, other maps/modes, arbitrary player counts or online play was validated. Use only on builds you are authorized to modify.

No open-source license has been selected for this repository; third-party rights remain with their respective holders.
