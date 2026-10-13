# Prior art

Outside work on the three USA Spyro executables; a reference names addresses, our measurement decides behaviour.

Our targets: Spyro 1 `SCUS_942.28` SHA-1 `84e3728ab94720d0873e2514adf4aade4935e0c5`; Spyro 2 `SCUS_944.25` SHA-1
`42633ff8cf1b43c49c5fe23d00eca6eb2703828b`; Spyro 3 `SCUS_944.67` (rev 0) SHA-1 `31ad35fd03539910b5b3d9309ae52f5acaf3612e`.
Address checks ran `decomp_pipeline.py --function-at` on our images (2026-10-13). `docs/references.md` keeps how the
vendored references are used; this table is the inventory.

| project | URL | kind | license | target build | build match (addresses checked) | what we may take | notes |
|---|---|---|---|---|---|---|---|
| TheMobyCollective/spyro-1 | https://github.com/TheMobyCollective/spyro-1 | matching decomp (splat, gcc 2.7.2) | CC0-1.0 | Spyro 1 `SCUS_942.28` | matched: `sha256sum.txt` `PSX.EXE` equals our SHA-256 `a533d75c...`; `0x80022A2C`, `0x8004F000` present | vendor under license containment | Submodule `external/spyro-1`, primary reference. `progress.md` 254/270 matched on 2026-10-13. Builds 37 code overlays into `WAD.WAD`. |
| theMagicalKarp/open-spyro | https://github.com/theMagicalKarp/open-spyro | matching decomp | CC0-1.0 | Spyro 1 `SCUS_942.28` | matched: same SHA-1 (2026-08-12) | vendor under license containment | Submodule `external/open-spyro`. |
| TheMobyCollective/spyro-2 | https://github.com/TheMobyCollective/spyro-2 | matching decomp (splat), EXE plus WAD overlays | CC0-1.0 | Spyro 2 `SCUS_944.25` | matched: `preparation/scus_944.25.yaml` sha1 equals ours; `func_8001B140` (matched C), `func_800156FC`, `func_80011E9C`, `func_8005251C`, `func_800520CC` listed; its split points `0x800159C8` and `0x8001B2A8` equal our Ghidra body ends of `0x800156FC` and `0x8001B140`; `0x80043858` is its `mobydraw` start and a Ghidra entry (811 insns) | vendor under license containment | Active (2026-10-10), 71/330 EXE functions matched. Mostly `func_` names. |
| TheMobyCollective/spyro-3 | https://github.com/TheMobyCollective/spyro-3 | matching decomp (splat), EXE plus encrypted WAD overlays | CC0-1.0 | Spyro 3 `SCUS_944.67` rev 0 | matched: `preparation/scus_944.67.yaml` sha1 equals ours; `main` segment starts at our game main `0x8001200C` (Ghidra, 12 insns); `func_8001E638` tagged Draw (our draw, 349 insns); `func_80055400` (our update), `func_8002AB38` (our boot prefix, 178 insns), `func_8002A834`, `func_8002A99C`, `func_800503F8`, `func_80050504` listed | vendor under license containment | Active (2026-10-13), 164/337 EXE functions matched. README notes modchip and anti-crack protection and encrypted overlays, relevant to the `jal 0x80074DEC` outside resident text. |
| wafflewizard1/SpyroGhidra | https://github.com/wafflewizard1/SpyroGhidra | Ghidra `.gzf` plus exported C, Spyro 1, 2, 3 1.0 NTSC | none | Spyro 2 NTSC, Spyro 3 1.0 NTSC | matched: Spyro 2 export has `FUN_80011e9c`, `FUN_80043858`, `FUN_80023bb4`; Spyro 3 export has `FUN_8002a834` and calls `FUN_8002ab38` and `FUN_8005d35c(0x100,0x78)` | names only | Dormant since 2024. Never import the `.gzf`; read the `.c` text only. |
| marcovoc/spyro2-decomp | https://github.com/marcovoc/spyro2-decomp | matching decomp | CC0-1.0 | Spyro 2 PAL `SCES_021.04` | mismatched: none of `0x8001B140`, `0x800156FC`, `0x80043858`, `0x80011E9C` in `symbol_addrs.txt`; different SHA-256 | nothing | Wrong region. |
| celophi/spyro-decompilation | https://github.com/celophi/spyro-decompilation | C reimplementation placed in extra RAM | none | Spyro 1 | not checked | nothing | Dormant since 2024-01. |
| abjeni/spyro-decompiler | https://github.com/abjeni/spyro-decompiler | whole-game MIPS-to-C decompiler with SDL3 host | GPL-3.0 | Spyro 1 (disc `.bin`, no hash stated) | not checked: no addresses or hash published | nothing | Active (2026-09-22). |
| wafflewizard1/Spyro-3-Recompiled | https://github.com/wafflewizard1/Spyro-3-Recompiled | static recompilation (psxrecomp) | none | Spyro 3 `SCUS-94467` | not checked: serial only | nothing | |
| FranklyGD/Spyro-Scope | https://github.com/FranklyGD/Spyro-Scope | live RAM and data viewer | GPL-3.0 | Spyro 1 NTSC-U, Spyro 2 NTSC, Spyro 3 NTSC v1.1 | not checked; Spyro 3 v1.1 is not our rev 0 | evidence only | Data-format reference. |

Not found (2026-10-13): a public OpenPete (Spyro 1 PC port) repository; any Spyro title on decomp.dev; named symbol maps
for Spyro 2 or 3 beyond the TheMobyCollective splits and SpyroGhidra. Searched GitHub repositories (titles, serials,
"decomp", "ghidra", "recomp"), decomp.dev, and the web.
