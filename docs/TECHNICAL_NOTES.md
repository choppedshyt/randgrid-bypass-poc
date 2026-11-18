
# Technical notes ? exact-build offline path

## Native evidence

A genuine open of the device object \??\Randgrid returned 0xC0000034 (STATUS_OBJECT_NAME_NOT_FOUND). The main, recurring worker and match-start code used independent acceptance decisions after native cleanup. The bypass does not fabricate STATUS_SUCCESS for NtCreateFile, does not create a kernel handle and does not synthesize an IOCTL response.

The original startup has a native image-remapping phase; running proxy hooks before that phase completes caused earlier faults. The post-map gate synchronizes the bootstrap with an observed native cleanup boundary.

## Application-level admission

| Phase | Focused source | Relevant exact-build location |
| --- | --- | --- |
| Startup main | randgrid_offline.asm / .inl | status decision RVA 0x1EC630F4 |
| Background worker | randgrid_worker.asm / .inl | preserved stack status and acceptance flag |
| Local match | randgrid_match.asm / .inl | status decision RVA 0x213C2DD9 |
| Match bookkeeping | native continuation | RVA 0x1F5E09F1 |

The match handler tests a positive enable state, EDX == 0xC0000034, full 64-bit R12 == 0, and the null handle at RBP+0x438. For that state it sets R13D=1, repeats TEST EDX,EDX so defined branch flags match, and resumes resource-free native bookkeeping. Other cases keep their original positive or rejection paths.

The recurring worker controls only its own acceptance byte for that same missing-object status. The main policy has a one-use restoration step and verifies native bytes after disabling the detour. In all cases, specific native byte and state guards are required; unknown builds retain their original behavior.

## Constraints

These addresses, stack offsets and byte patterns identify one studied build. They are not generic offsets and must not be inferred for another executable. The PoC does not modify the kernel driver or the SCM service database, and the abandoned process-exit swallowing experiment is excluded.

A static path and a selected emulation run do not provide a universal proof about resource lifetimes, concurrent calls, client/server networking or online play.
