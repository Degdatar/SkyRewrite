# SkyRewrite API Header

The SkyRewrite API Header package contains a C-compatible header and a guide for SKSE plugin authors who want to read SkyRewrite's saved world-history events and current Skyrim hold bounties.

## Contents

- `include/SkyRewriteAPI.h` — versioned C ABI declarations and a `GetProcAddress` loader. No SkyRewrite import library is required.
- `Data/SKSE/Plugins/SkyOS/Mods/SkyRewriteAPIHeader/SkyDocuments/SkyRewriteAPI Guide.html` — the branded guide, indexed by SkyDocuments and containing API reference, integration steps, safety notes, and project ideas.

## Quick start

Include `SkyRewriteAPI.h`, obtain the loaded module with `GetModuleHandleW(L"SkyRewrite.dll")`, and call `SkyRewrite_LoadAPI`. The journal accessors return visible events newest first. Initialize each output structure's `structSize` before calling its `get...At` function.

The API is read-only. The bounty snapshot functions inspect Skyrim state and must be called on the game thread. See the HTML guide for a complete example and field descriptions.

Version 1 also publishes named event constants for equipment, skill/level changes, book reading, ingredient effects, follower changes, smithing, tanning, potions, enchanting, used materials, cooking, disenchanting (`SKYREWRITE_EVENT_ITEM_DISENCHANTED`, 44), and brawling (`SKYREWRITE_EVENT_BRAWL`, 45). Brawl records use stage `1` for a player win and `2` for a loss, with `identity` set to `SkyRewrite/Brawl/v1/won` or `SkyRewrite/Brawl/v1/lost`. These constants keep plugins from hard-coding numeric values. Other event-type values should be handled as unknown and ignored safely.

The runtime dependency is the SkyRewrite app plugin installed with SkyOS. The header package alone does not install or activate SkyRewrite.

This standalone SkyRewrite project includes the header and branded guide alongside the app runtime. Configure and build the project Release preset, then run `cpack --config build/CPackConfig.cmake -G 7Z -C Release` to package the project.

