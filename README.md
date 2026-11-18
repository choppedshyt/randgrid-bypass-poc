# me vs randgrid.sys

<img width="447" height="447" alt="image" src="https://github.com/user-attachments/assets/fd8a4dde-ef72-49b8-8e07-5e7de946a1f0" />


THIS WILL ONLY WORK ON SEASON 2 RELOADED of MWIII feel free to pr support to other titles (CORTEZ, CER, SAT)

stripped [game.cpp and dllmain.cpp integration examples](examples/README.md)

https://drive.google.com/file/d/1t_rTaO35hiX_0YffMH65BkEn00We_TWn/view?usp=sharing

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
