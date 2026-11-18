
# Historical validation and qualifications

The verified historical DLL had SHA-256:
412890b9e929a229e9a7303f89cfd36b1569ac47566147ee0caece1bb5fae10b

The previously assembled match hook passed a focused Unicorn/Capstone matrix of 4,096 input/flag cases, including register, SIMD, stack and defined flag checks. The test also exercised 16 consecutive eligible admissions. This emulation used an external native reference image and DLL not included in this repository.

At runtime, two local-host Invasion Team Deathmatch games were observed, with a deliberate return to the frontend and a subsequent successful Start Match. There were three total match admissions: the first attempt returned to the lobby with a separate DEV ERROR 6145 capacity condition and the following two reached gameplay. The successful attempts logged a configured capacity of 150; that is not a stress test of 150 players.

The later status-zero exit was reported by the operator to occur after x64dbg attachment and is excluded from standalone fix verification. The prior startup launch itself used no debugger.

The release retained the 316-symbol public DLL export contract and the same 12 embedded resource payloads; no DLL or its resource data are published here. No physical second LAN client, other maps or game modes, long-running network sessions, concurrent hook stress, or newer builds were tested. This source-only PoC does not constitute a newly compiled and installed release.
