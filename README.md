# SkyRewrite

SkyRewrite is a native SkyOS app module that keeps a save-scoped journal of supported Skyrim activity and regional consequences. It records events such as discoveries, combat outcomes, theft, inventory changes, crafting, spell learning, and brawl results, and provides tools for reviewing or safely undoing eligible temporary bounty changes.

## Features

- Searchable, save-scoped World History for travel, discoveries, combat, quests, inventory, crafting, disenchanting, learned effects, spells, shouts, perks, and rule responses.
- Context-aware inventory and septim records that distinguish purchases, looting, storage, gifts, sales, drops, and confiscation when Skyrim provides enough evidence.
- Local Echo summaries and configurable repeated-theft or temporary bounty responses.
- Hold-level bounty view for live Skyrim crime balances and SkyRewrite's reversible temporary records.
- Separate dashboard preferences and active-save event history.
- Public versioned C ABI for native plugins to read saved world-history events and current Skyrim hold bounties.

SkyRewrite records only supported events for which it can resolve reliable game context; it does not guess event labels when the source is ambiguous.

## Requirements

- Windows x64 and Visual Studio 2022 with the C++ desktop workload.
- CMake 3.24 or newer.
- Git, for fetching CommonLibSSE-NG.
- vcpkg with `VCPKG_ROOT` set and the `x64-windows-static-md` triplet.
- Skyrim Special Edition / Anniversary Edition, SKSE, PrismaUI, and the matching SkyOS core at runtime.

## Build and package

Configure and build the Release preset from this directory:

```powershell
cmake --preset release
cmake --build --preset release --parallel
```

Create a 7-Zip archive from CPack's install manifest:

```powershell
cpack --config build/CPackConfig.cmake -G 7Z -C Release
```

The archive contains `SkyRewrite.dll`, its PrismaUI dashboard, SkyViewer settings module, SkyDocuments, branding, and the public API header and guide under Skyrim's `Data` layout. The CPack command requires a CMake installation with the 7Z generator available.

## SkyOS integration

`SkyOS.dll` owns the shared shell and loads `SkyRewrite.dll` as an app module. SkyRewrite supplies its dashboard, event listeners, save serialization, and exported app functions. Its SKSE entry checks for the core at data load and remains inactive if SkyOS is absent.

Installing this project alone does not provide SkyOS Core, SKSE, or PrismaUI. Install those runtime dependencies in the same Skyrim profile.

## Public API

The public header is `data/SkyRewriteAPIHeader/include/SkyRewriteAPI.h`. It loads the API from an already-loaded `SkyRewrite.dll` using `GetProcAddress`; no import library or C++ STL types cross the ABI. The read-only API provides saved journal event snapshots and current hold bounty balances. See `data/SKSE/Plugins/SkyOS/Mods/SkyRewriteAPIHeader/SkyDocuments/SkyRewriteAPI Guide.html` for the function reference, record formats, and integration examples.

## SkyDocuments

The SkyRewrite user guide, feature reference, and module notes are in `data/SKSE/Plugins/SkyOS/Mods/SkyRewrite/SkyDocuments/`. The separate API guide is in `data/SKSE/Plugins/SkyOS/Mods/SkyRewriteAPIHeader/SkyDocuments/`.

## Repository layout

```text
src/                                                    Native SKSE plugin, module entry, and exported API implementation
data/PrismaUI/views/SkyRewrite/                         SkyRewrite dashboard, app logo, and banner
data/SKSE/Plugins/SkyOS/Mods/SkyRewrite/                App manifest and user/feature/module guides
data/SKSE/Plugins/SkyOS/Mods/SkyViewer/SkyRewrite/      SkyViewer module settings and its page assets
data/SkyRewriteAPIHeader/                               Public C ABI header and README
data/SKSE/Plugins/SkyOS/Mods/SkyRewriteAPIHeader/      Branded API SkyDocuments guide
```

The event journal and reversible-action records are serialized into the active save's SKSE co-save. Dashboard preferences are stored locally by the SkyRewrite page and are separate from the save data.

