# SkyRewrite

## Features

- Records supported world, combat, inventory, quest, location, and reputation activity in a searchable, save-scoped journal. History includes player brawl wins and losses with the opponent when available, learned spells, observed shout and perk unlocks, skill and level increases, books read, learned ingredient effects, follower changes, and discovered map markers with FormIDs and available cell/skill-tree details. Brawl losses are recorded separately and do not count as wins.
- Uses available source, destination, activation, and menu context to distinguish pickups, purchases, looting, NPC gifts, sales, storage on mannequins or in displays, drops, and guard confiscation. Unclear inventory moves retain a neutral transfer label instead of being guessed.
- Tracks gold gained and spent with the change amount and current septim balance. Supported purchases are distinguished from container looting when the barter menu and matching merchant stock context identify the transaction.
- Records supported food, potion, and ingredient consumption. Potion records use the resolved potion name when available; eating an ingredient records each newly learned effect when Skyrim exposes it.
- Records smithing, tanning, potion making, enchanting, disenchanting, and cooking results with the output or disenchanted item and available workstation, house/interior, and hold names. Materials removed for a recognized crafting result are recorded as used for that output rather than dropped.
- Persuasion, intimidation, and bribery rows require a matching player-selected topic and Skyrim's explicit dialogue subtype; unrelated dialogue text is ignored.
- Summarizes regional activity and local pressure, with configurable event retention and live history update delay in the SkyViewer module.
- Live history update delay accepts 250–30000 ms (default 3000 ms); SkyRewrite requests a new history snapshot only when the journal changes and the World history tab is visible, then waits this delay after the latest action.
- World history renders the newest 100 matching rows first. Use “Load older entries” to bring more rows into view; changing the journal refreshes the latest batch without rebuilding hidden dashboards.
- Optional Local Echo records a consequence when the player revisits a sufficiently active place. Its notice, temporary nonviolent bounty, eligible victim type, and repeated-theft bounty rules can be tuned in the SkyViewer module.
- The matching-hold guard dispatch option attempts a guard spawn when SkyRewrite applies a bounty; the outcome is recorded and Skyrim still controls detection and arrest behavior.
- Adaptive guard-confidence changes are currently suspended for stability. Its module switch does not change guard confidence.
- Eligible temporary bounty records can be reviewed or safely undone when their balance checks pass.

## Changelog

- **Independent dashboard:** SkyRewrite owns its dashboard, settings form, refresh behavior, and native event handlers. SkyOS keeps only the Home and side-menu launch entries and the iframe message bridge.
- **Settings:** Preferences are stored by the SkyRewrite dashboard in its own browser storage. World history and reversible-action records remain serialized into the active save.
- **Action history:** New action records are save-scoped. Spell learning uses Skyrim's learned-spell event; perk/shout additions are detected from player-form snapshots taken on relevant skill-menu, quest, location, and map-discovery events. Crafting actions use Skyrim's crafted-output events and recognized workstation context; disenchantment is recorded when the removed item and learned enchantment can be identified. Brawl outcomes use Skyrim's brawl quest result and distinguish player wins from losses.

## Requirements

- Skyrim Special Edition / Anniversary Edition Latest Version (supported runtime).
- SKSE Latest Version.
- PrismaUI Latest Version.
- SkyOS Latest Version.

## Files and saved data

- Dashboard and settings: `Data/PrismaUI/views/SkyRewrite/`.
- SkyDocuments guide: `Data/SKSE/Plugins/SkyOS/Mods/SkyRewrite/SkyDocuments/`.
- Per-user dashboard settings: PrismaUI local storage under `skyrewrite-settings-v1`.
- Journal and recorded reversible-action state: the active save's SKSE co-save. Settings and save history are kept separately.

