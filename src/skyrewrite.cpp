#include "pch.h"
#include "SkyRewriteAPI.h"

#include <RE/C/Calendar.h>
#include <RE/C/CraftingMenu.h>
#include <RE/A/ActorValueInfo.h>
#include <RE/A/AlchemyMenu.h>
#include <RE/B/BGSPerk.h>
#include <RE/B/BGSSkillPerkTreeNode.h>
#include <RE/B/BGSLocation.h>
#include <RE/B/BooksRead.h>
#include <RE/E/Effect.h>
#include <RE/E/EffectSetting.h>
#include <RE/E/EnchantConstructMenu.h>
#include <RE/E/EnchantmentItem.h>
#include <RE/E/ExtraTeleport.h>
#include <RE/L/LocationDiscovery.h>
#include <RE/L/LevelIncrease.h>
#include <RE/E/ExtraMapMarker.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/ScriptEventSourceHolder.h>
#include <RE/S/SkillIncrease.h>
#include <RE/S/SpellsLearned.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESActivateEvent.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESDeathEvent.h>
#include <RE/T/TESEnterBleedoutEvent.h>
#include <RE/T/TESActorLocationChangeEvent.h>
#include <RE/I/ItemRemoveReason.h>
#include <RE/I/IngredientItem.h>
#include <RE/I/InventoryEntryData.h>
#include <RE/I/ItemCrafted.h>
#include <RE/T/TESContainerChangedEvent.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESFurniture.h>
#include <RE/T/TESFurnitureEvent.h>
#include <RE/T/TESWorldSpace.h>
#include <RE/T/TESForm.h>
#include <RE/F/FormTraits.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESNPC.h>
#include <RE/B/BarterMenu.h>
#include <RE/C/ContainerMenu.h>
#include <RE/D/DialogueMenu.h>
#include <RE/E/ExtraFactionChanges.h>
#include <RE/L/LockpickingMenu.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/S/StatsMenu.h>
#include <RE/A/AlchemyItem.h>
#include <RE/T/TESQuest.h>
#include <RE/T/TESQuestStageEvent.h>
#include <RE/T/TESQuestStartStopEvent.h>
#include <RE/T/TESLockChangedEvent.h>
#include <RE/T/TESLoadGameEvent.h>
#include <RE/T/TESObjectBOOK.h>
#include <RE/T/TESTopicInfo.h>
#include <RE/T/TESTopicInfoEvent.h>
#include <RE/T/TESTopic.h>
#include <RE/T/TESObjectDOOR.h>
#include <RE/T/TESEquipEvent.h>
#include <RE/T/TESShout.h>
#include <RE/G/GameSettingCollection.h>
#include <RE/S/Setting.h>
#include <RE/S/SkyrimVM.h>
#include <RE/I/IVirtualMachine.h>
#include <RE/I/IStackCallbackFunctor.h>
#include <RE/F/FunctionArguments.h>
#include <RE/P/PackUnpack.h>
#include <RE/V/Variable.h>
#include <RE/A/Actor.h>
#include <RE/A/ActorValueList.h>
#include <RE/P/ProcessLists.h>
#include <RE/S/SendHUDMessage.h>
#include <RE/U/UI.h>

// PrismaUI_API.h includes Windows.h. Keep it after CommonLib's RE headers so
// Windows interlocked-operation macros do not rewrite REX::W32 declarations.
#include "skyrewrite.h"

#include <algorithm>
#include <charconv>
#include <array>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace SkyOS {
namespace {
    constexpr std::uint32_t kPluginID = 'SKRW';
    constexpr std::uint32_t kRecordType = 'EVNT';
    constexpr std::uint32_t kRecordVersion = 5;
    constexpr std::uint32_t kMaxEvents = 2048;
    constexpr std::uint32_t kEventEquipped = 30;
    constexpr std::uint32_t kEventUnequipped = 31;
    constexpr std::uint32_t kEventSkillIncreased = 32;
    constexpr std::uint32_t kEventLevelIncreased = 33;
    constexpr std::uint32_t kEventBookRead = 34;
    constexpr std::uint32_t kEventIngredientEffectLearned = 35;
    constexpr std::uint32_t kEventFollowerStarted = 36;
    constexpr std::uint32_t kEventFollowerStopped = 37;
    constexpr std::uint32_t kEventItemSmithed = 38;
    constexpr std::uint32_t kEventItemTanned = 39;
    constexpr std::uint32_t kEventPotionCrafted = 40;
    constexpr std::uint32_t kEventItemEnchanted = 41;
    constexpr std::uint32_t kEventCraftMaterialUsed = 42;
    constexpr std::uint32_t kEventItemCooked = 43;
    constexpr std::uint32_t kEventItemDisenchanted = 44;
    constexpr std::uint32_t kEventBrawl = 45;
    constexpr RE::FormID kVanillaBrawlQuestFormID = 0x00047AE6;
    constexpr std::uint16_t kVictimHostile = 1U << 0;
    constexpr std::uint16_t kVictimGuard = 1U << 1;
    constexpr std::uint16_t kVictimEssential = 1U << 2;
    constexpr std::uint16_t kVictimTeammate = 1U << 3;
    constexpr std::uint16_t kVictimBandit = 1U << 4;
    constexpr std::uint16_t kVictimDragon = 1U << 5;
    constexpr std::uint16_t kVictimForsworn = 1U << 6;
    constexpr std::uint16_t kVictimVampire = 1U << 7;
    constexpr std::uint16_t kVictimWolf = 1U << 8;
    constexpr std::uint16_t kVictimBear = 1U << 9;
    constexpr std::uint16_t kVictimSpider = 1U << 10;
    constexpr std::uint16_t kVictimMonster = 1U << 11;
    constexpr std::uint16_t kVictimAllHostile = 1U << 12;
    constexpr std::uint16_t kVictimOtherHostile = 1U << 13;
    constexpr std::uint16_t kVictimCivilian = 1U << 14;
    constexpr std::uint16_t kDispositionKnown = 1U << 15;

#pragma pack(push, 1)
    struct LegacyEventRecord {
        std::uint32_t type{};  // 1 = player-related death, 2 = quest stage reached
        float gameDay{-1.0F};
        std::uint32_t formID{};
        std::uint16_t stage{};
        char subject[64]{};
        char detail[128]{};
    };
    struct EventRecord {
        std::uint32_t type{};
        float gameDay{-1.0F};
        std::uint32_t formID{};
        std::uint32_t locationFormID{};
        std::uint32_t baseFormID{};
        char formPlugin[64]{};
        char locationPlugin[64]{};
        char basePlugin[64]{};
        std::uint16_t stage{};
        char subject[128]{};
        char detail[128]{};
        char location[64]{};
        char identity[96]{};
    };
    struct Version2EventRecord {
        std::uint32_t type{};
        float gameDay{-1.0F};
        std::uint32_t formID{};
        std::uint32_t locationFormID{};
        std::uint16_t stage{};
        char subject[64]{};
        char detail[128]{};
        char location[64]{};
        char identity[96]{};
    };
    struct Version3EventRecord {
        std::uint32_t type{};
        float gameDay{-1.0F};
        std::uint32_t formID{};
        std::uint32_t locationFormID{};
        std::uint32_t baseFormID{};
        std::uint16_t stage{};
        char subject[64]{};
        char detail[128]{};
        char location[64]{};
        char identity[96]{};
    };
    struct Version4EventRecord {
        std::uint32_t type{};
        float gameDay{-1.0F};
        std::uint32_t formID{};
        std::uint32_t locationFormID{};
        std::uint32_t baseFormID{};
        char formPlugin[64]{};
        char locationPlugin[64]{};
        char basePlugin[64]{};
        std::uint16_t stage{};
        char subject[64]{};
        char detail[128]{};
        char location[64]{};
        char identity[96]{};
    };
#pragma pack(pop)

    void AddEvent(EventRecord event);
    void NotifyDashboardHistoryChanged();

    std::mutex g_eventsMutex;
    std::mutex g_brawlStateMutex;
    std::atomic_bool g_dashboardHistoryNoticeQueued{};
    std::vector<EventRecord> g_events;
    bool g_brawlActive{};
    EventRecord g_brawlOpponent{};
    std::atomic<std::uint32_t> g_sourceEventsCapturedThisSession{ 0 };
    std::atomic<std::uint32_t> g_deathEventsSeenThisSession{ 0 };
    std::atomic<std::uint32_t> g_playerKillsCapturedThisSession{ 0 };
    std::atomic_bool g_localEchoEnabled{ false };
    std::atomic_bool g_localEchoNotificationEnabled{ true };
    std::atomic_bool g_adaptiveGuardResponseEnabled{ false };
    std::atomic_bool g_temporaryBountyEnabled{ false };
    std::atomic<std::uint32_t> g_temporaryBountyAmount{ 25 };
    std::atomic<float> g_temporaryBountyDuration{ 1.0F };
    std::atomic_bool g_temporaryBountyPressureScaling{ false };
    // 0 = non-hostile victims, 1 = hostile victims, 2 = either/both.
    std::atomic<std::uint32_t> g_bountyVictimMode{ 0 };
    std::atomic_bool g_theftBountyEnabled{ false };
    std::atomic<std::uint32_t> g_theftBountyAmount{ 25 };
    std::atomic<std::uint32_t> g_theftBountyThreshold{ 3 };
    struct PlayerItemRemovalContext
    {
        RE::Actor* actor{};
        RE::FormID baseObj{};
        RE::ITEM_REMOVE_REASON reason{ RE::ITEM_REMOVE_REASON::kRemove };
    };
    struct RecentPlayerItemRemoval
    {
        std::int64_t quantity{};
        RE::ITEM_REMOVE_REASON reason{ RE::ITEM_REMOVE_REASON::kRemove };
        std::int64_t capturedAtMs{};
    };
    using ActorRemoveItemFunction = RE::ObjectRefHandle (*)(RE::Actor*, RE::TESBoundObject*, std::int32_t,
        RE::ITEM_REMOVE_REASON, RE::ExtraDataList*, RE::TESObjectREFR*, const RE::NiPoint3*, const RE::NiPoint3*);
    using EnchantDisenchantCallback = RE::CraftingSubMenus::EnchantConstructMenu::EnchantMenuDisenchantCallback;
    using EnchantDisenchantRunFunction = void (*)(EnchantDisenchantCallback*, std::uint8_t);
    using AlchemyCraftCallback = RE::CraftingSubMenus::CraftingSubMenus::AlchemyMenu::CraftItemCallback;
    using AlchemyCraftRunFunction = void (*)(AlchemyCraftCallback*, std::uint8_t);
    thread_local PlayerItemRemovalContext g_playerItemRemovalContext{};
    std::mutex g_recentPlayerItemRemovalsMutex;
    std::unordered_map<RE::FormID, std::vector<RecentPlayerItemRemoval>> g_recentPlayerItemRemovals;
    std::mutex g_recentConsumableUsesMutex;
    std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> g_recentConsumableUses;
    std::mutex g_craftingMaterialsMutex;
    std::vector<EventRecord> g_pendingCraftMaterials;
    struct RecentCraftedAction {
        EventRecord record{};
        char placeLabel[96]{};
        std::chrono::steady_clock::time_point recordedAt{};
        bool valid{};
    };
    RecentCraftedAction g_recentCraftedAction{};
    ActorRemoveItemFunction g_originalPlayerRemoveItem{};
    bool g_playerRemoveItemHookInstalled{};
    EnchantDisenchantRunFunction g_originalDisenchantRun{};
    bool g_disenchantCallbackHookInstalled{};
    std::atomic<RE::FormID> g_activeDisenchantItemFormID{ 0 };
    std::atomic_bool g_activeDisenchantCallback{};
    std::atomic<std::int64_t> g_recentConfirmedDisenchantAtMs{};
    AlchemyCraftRunFunction g_originalAlchemyCraftRun{};
    bool g_alchemyCraftCallbackHookInstalled{};
    std::mutex g_recentAlchemyCraftNameMutex;
    std::array<char, 128> g_recentAlchemyCraftName{};
    std::int64_t g_recentAlchemyCraftNameAtMs{};
    std::atomic<std::int64_t> g_recentCraftedPotionEventAtMs{};
    std::atomic<RE::FormID> g_recentCraftedPotionFormID{};
    std::unordered_map<RE::FormID, std::uint8_t> g_murderReactions;
    std::atomic<RE::FormID> g_activeLockpickTarget{};
    std::atomic<RE::FormID> g_activeCraftingWorkbench{};
    std::atomic<std::int64_t> g_lockpickMenuOpenedAtMs{};
    std::atomic<std::int64_t> g_lockpickMenuClosedAtMs{};
    std::atomic_bool g_dispatchGuardOnBounty{ true };
    std::atomic<std::uint32_t> g_localEchoKillThreshold{ 3 };
    std::atomic<float> g_localEchoLookbackDays{ 7.0F };
    std::atomic<float> g_localEchoCooldownDays{ 7.0F };
    std::atomic<std::uint32_t> g_journalRetention{ 2048 };
    std::mutex g_unlockSnapshotMutex;
    std::unordered_set<RE::FormID> g_knownPlayerShouts;
    std::unordered_set<RE::FormID> g_knownPlayerPerks;
    bool g_unlockSnapshotInitialized{};
    std::mutex g_skillSnapshotMutex;
    std::unordered_map<std::uint32_t, float> g_playerSkillValues;
    std::uint16_t g_lastPlayerLevel{};
    std::mutex g_ingredientEffectsMutex;
    std::unordered_map<RE::FormID, std::uint16_t> g_knownIngredientEffects;
    std::mutex g_followerStateMutex;
    std::unordered_map<RE::FormID, bool> g_playerFollowerStates;

    void CopyText(char* target, std::size_t capacity, const char* text)
    {
        if (!target || !capacity) return;
        target[0] = '\0';
        if (text) std::snprintf(target, capacity, "%s", text);
    }

    void CopyFormPlugin(char* target, std::size_t capacity, const RE::TESForm* form)
    {
        if (!form || !target || !capacity) return;
        auto* file = form->GetDescriptionOwnerFile();
        if (!file) file = form->GetFile();
        if (!file) return;
        const auto filename = file->GetFilename();
        if (!filename.empty()) std::snprintf(target, capacity, "%.*s", static_cast<int>(capacity - 1), filename.data());
    }

    void AddLocation(EventRecord& record, RE::PlayerCharacter* player)
    {
        auto* cell = player ? player->GetParentCell() : nullptr;
        auto* location = cell ? cell->GetLocation() : nullptr;
        if (!location) location = player ? player->GetCurrentLocation() : nullptr;
        if (location) {
            record.locationFormID = location->GetFormID();
            CopyFormPlugin(record.locationPlugin, sizeof(record.locationPlugin), location);
            const auto* name = location->GetName();
            if (!name || !*name) name = location->GetFormEditorID();
            if (name && *name) {
                CopyText(record.location, sizeof(record.location), name);
                return;
            }
        }

        if (cell) {
            record.locationFormID = cell->GetFormID();
            CopyFormPlugin(record.locationPlugin, sizeof(record.locationPlugin), cell);
            const auto* name = cell->GetName();
            if (!name || !*name) name = cell->GetFormEditorID();
            if (name && *name) {
                std::snprintf(record.location, sizeof(record.location), "Cell: %.58s", name);
                return;
            }
        }

        auto* worldspace = player ? player->GetWorldspace() : nullptr;
        if (worldspace) {
            record.locationFormID = worldspace->GetFormID();
            CopyFormPlugin(record.locationPlugin, sizeof(record.locationPlugin), worldspace);
            const auto* name = worldspace->GetName();
            if (!name || !*name) name = worldspace->GetFormEditorID();
            if (name && *name) {
                std::snprintf(record.location, sizeof(record.location), "World: %.56s", name);
                return;
            }
        }
        CopyText(record.location, sizeof(record.location), "Location unavailable");
    }

    float CurrentGameDay()
    {
        auto* calendar = RE::Calendar::GetSingleton();
        return calendar ? calendar->GetDaysPassed() : -1.0F;
    }

    const char* CellDisplayName(const RE::TESObjectCELL* cell)
    {
        if (!cell) return nullptr;
        const auto* name = cell->GetName();
        if (!name || !*name) name = cell->GetFormEditorID();
        return name && *name ? name : nullptr;
    }

    void NotifySkyCounterLocationDiscovery(RE::TESForm* source, const char* discoveredName)
    {
        if (!discoveredName || !*discoveredName) {
            SKSE::log::warn("SkyRewrite location discovery cannot notify SkyCounterUI: sourceFormResolved={}, markerName='{}'",
                source != nullptr, discoveredName ? discoveredName : "");
            return;
        }
        struct LocationNotice
        {
            std::uint32_t formID{};
            std::uint32_t localFormID{};
            char plugin[260]{};
            char name[256]{};
        } notice;
        notice.formID = source ? source->GetFormID() : 0;
        notice.localFormID = source ? source->GetLocalFormID() : 0;
        if (const auto* file = source ? source->GetFile() : nullptr; file && !file->GetFilename().empty()) {
            const auto filename = file->GetFilename();
            std::snprintf(notice.plugin, sizeof(notice.plugin), "%.*s",
                static_cast<int>(sizeof(notice.plugin) - 1), filename.data());
        } else {
            std::snprintf(notice.plugin, sizeof(notice.plugin), "%s", "Unknown plugin");
        }
        std::snprintf(notice.name, sizeof(notice.name), "%s", discoveredName);
        auto* messaging = SKSE::GetMessagingInterface();
        constexpr std::uint32_t kSkyRewriteLocationDiscovered = 'SRLD';
        if (messaging && messaging->Dispatch(kSkyRewriteLocationDiscovered, &notice,
                static_cast<std::uint32_t>(sizeof(notice)), "SkyCounter UI")) {
            SKSE::log::info("SkyRewrite sent SkyCounterUI discovery '{}': sourceFormID={:08X}, localFormID={:08X}, plugin='{}'",
                notice.name, notice.formID, notice.localFormID, notice.plugin);
        } else {
            SKSE::log::warn("SkyRewrite could not dispatch discovery '{}' to SkyCounter UI: sourceFormID={:08X}, localFormID={:08X}, messaging={}, recipient='SkyCounter UI'",
                notice.name, notice.formID, notice.localFormID, messaging != nullptr);
        }
    }

    const char* ReferenceDisplayName(const RE::TESForm* form)
    {
        if (!form) return nullptr;
        auto* mutableForm = const_cast<RE::TESForm*>(form);
        auto* reference = mutableForm->As<RE::TESObjectREFR>();
        if (reference) {
            const auto* displayName = reference->GetDisplayFullName();
            if (displayName && *displayName) return displayName;

            if (auto* actor = reference->As<RE::Actor>()) {
                if (const auto* actorBase = actor->GetActorBase()) {
                    const auto* actorName = actorBase->GetName();
                    if (actorName && *actorName) return actorName;
                    const auto* actorEditorID = actorBase->GetFormEditorID();
                    if (actorEditorID && *actorEditorID) return actorEditorID;
                }
            }

            if (const auto* base = reference->GetObjectReference()) {
                const auto* baseName = base->GetName();
                if (baseName && *baseName) return baseName;
                const auto* baseEditorID = base->GetFormEditorID();
                if (baseEditorID && *baseEditorID) return baseEditorID;
            }
        }

        const auto* name = form->GetName();
        if (name && *name) return name;
        const auto* editorID = form->GetFormEditorID();
        return editorID && *editorID ? editorID : nullptr;
    }

    const char* DisplayNameOrFormID(const RE::TESForm* form, const char* fallbackLabel,
        char* buffer, std::size_t capacity)
    {
        if (const auto* name = ReferenceDisplayName(form)) return name;
        if (!form || !buffer || capacity == 0) return fallbackLabel;
        std::snprintf(buffer, capacity, "%s FormID %08X", fallbackLabel ? fallbackLabel : "Reference", form->GetFormID());
        return buffer;
    }

    RE::ActorValueInfo* FindPerkSkillTree(const RE::BGSPerk* perk)
    {
        if (!perk) return nullptr;
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return nullptr;
        for (auto* skill : dataHandler->GetFormArray<RE::ActorValueInfo>()) {
            if (!skill || skill->type != RE::ActorValueInfo::ActorValueType::kSkill || !skill->perkTree) continue;
            std::vector<RE::BGSSkillPerkTreeNode*> pending{ skill->perkTree };
            std::unordered_set<RE::BGSSkillPerkTreeNode*> visited;
            while (!pending.empty()) {
                auto* node = pending.back();
                pending.pop_back();
                if (!node || !visited.insert(node).second) continue;
                if (node->perk == perk) return skill;
                for (auto* child : node->children) pending.push_back(child);
            }
        }
        return nullptr;
    }

    void RecordPlayerSpellLearned(RE::SpellItem* spell, RE::PlayerCharacter* player)
    {
        if (!spell || !player) return;
        EventRecord record{};
        record.type = 25;
        record.gameDay = CurrentGameDay();
        record.formID = spell->GetFormID();
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), spell);
        AddLocation(record, player);
        const auto* name = spell->GetName();
        std::string learnedSpellName;
        const auto isUnknownName = [](const char* value) {
            if (!value || !*value) return true;
            std::string normalized(value);
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return normalized == "unknown" || normalized == "unknown spell" || normalized.rfind("spell tome:", 0) == 0;
        };
        if (!isUnknownName(name)) learnedSpellName = name;

        // Some spell-tome mods expose a SpellItem whose display name is the
        // literal "Unknown" and whose book does not set TeachesSpell(). The
        // read or inventory event still carries the tome's visible title,
        // so use the most recent spell-tome action to name this event.
        if (learnedSpellName.empty()) {
            std::scoped_lock lock(g_eventsMutex);
            for (auto it = g_events.rbegin(); it != g_events.rend(); ++it) {
                constexpr std::string_view kSpellTomePrefix = "Spell Tome:";
                const std::string_view title(it->subject);
                const bool isSpellTome = title.size() >= kSpellTomePrefix.size() &&
                    std::equal(kSpellTomePrefix.begin(), kSpellTomePrefix.end(), title.begin(),
                        [](unsigned char left, unsigned char right) { return std::tolower(left) == std::tolower(right); });
                const bool canNameSpell = it->type == kEventBookRead || it->type == kEventEquipped || it->type == 4;
                if (!canNameSpell || !isSpellTome) continue;
                if (record.gameDay >= 0.0F && it->gameDay >= 0.0F && std::abs(record.gameDay - it->gameDay) > 0.1F) continue;
                learnedSpellName = it->subject;
                if (learnedSpellName.size() >= kSpellTomePrefix.size()) {
                    std::string prefixText = learnedSpellName.substr(0, kSpellTomePrefix.size());
                    std::transform(prefixText.begin(), prefixText.end(), prefixText.begin(), [](unsigned char ch) {
                        return static_cast<char>(std::tolower(ch));
                    });
                    if (prefixText == "spell tome:") learnedSpellName.erase(0, kSpellTomePrefix.size());
                }
                const auto firstCharacter = learnedSpellName.find_first_not_of(" \t");
                if (firstCharacter == std::string::npos) learnedSpellName.clear();
                else learnedSpellName.erase(0, firstCharacter);
                while (!learnedSpellName.empty() && std::isspace(static_cast<unsigned char>(learnedSpellName.back()))) {
                    learnedSpellName.pop_back();
                }
                if (!learnedSpellName.empty()) break;
            }
        }

        if (learnedSpellName.empty()) {
            if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
                for (auto* tome : dataHandler->GetFormArray<RE::TESObjectBOOK>()) {
                    if (!tome || !tome->TeachesSpell()) continue;
                    auto* tomeSpell = tome->GetSpell();
                    if (!tomeSpell || tomeSpell->GetFormID() != spell->GetFormID()) continue;
                    if (const auto* tomeName = ReferenceDisplayName(tome); tomeName && *tomeName) {
                        learnedSpellName = tomeName;
                        constexpr std::string_view kSpellTomePrefix = "Spell Tome:";
                        if (learnedSpellName.size() >= kSpellTomePrefix.size()) {
                            std::string prefixText = learnedSpellName.substr(0, kSpellTomePrefix.size());
                            std::transform(prefixText.begin(), prefixText.end(), prefixText.begin(), [](unsigned char ch) {
                                return static_cast<char>(std::tolower(ch));
                            });
                            if (prefixText == "spell tome:") {
                                learnedSpellName.erase(0, kSpellTomePrefix.size());
                                const auto firstCharacter = learnedSpellName.find_first_not_of(" \t");
                                if (firstCharacter == std::string::npos) learnedSpellName.clear();
                                else learnedSpellName.erase(0, firstCharacter);
                            }
                        }
                    }
                    break;
                }
            }
        }
        if (learnedSpellName.empty()) {
            if (const auto* fallbackName = ReferenceDisplayName(spell); !isUnknownName(fallbackName)) {
                learnedSpellName = fallbackName;
            }
        }
        CopyText(record.subject, sizeof(record.subject), learnedSpellName.empty() ? "Unknown spell" : learnedSpellName.c_str());
        CopyText(record.identity, sizeof(record.identity), spell->GetFormEditorID());
        CopyText(record.detail, sizeof(record.detail), "Learned");
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void RecordPlayerShoutLearned(RE::TESShout* shout, RE::PlayerCharacter* player)
    {
        if (!shout || !player) return;
        EventRecord record{};
        record.type = 27;
        record.gameDay = CurrentGameDay();
        record.formID = shout->GetFormID();
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), shout);
        AddLocation(record, player);
        const auto* name = ReferenceDisplayName(shout);
        CopyText(record.subject, sizeof(record.subject), name ? name : "Unknown shout");
        CopyText(record.identity, sizeof(record.identity), shout->GetFormEditorID());
        CopyText(record.detail, sizeof(record.detail), "Learned shout");
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void RecordPlayerPerkUnlocked(RE::BGSPerk* perk, RE::PlayerCharacter* player)
    {
        if (!perk || !player) return;
        EventRecord record{};
        record.type = 28;
        record.gameDay = CurrentGameDay();
        record.formID = perk->GetFormID();
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), perk);
        AddLocation(record, player);
        const auto* name = ReferenceDisplayName(perk);
        CopyText(record.subject, sizeof(record.subject), name ? name : "Unknown perk");
        CopyText(record.identity, sizeof(record.identity), perk->GetFormEditorID());
        auto* skill = FindPerkSkillTree(perk);
        if (skill) {
            record.baseFormID = skill->GetFormID();
            CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), skill);
            const auto* skillName = ReferenceDisplayName(skill);
            std::snprintf(record.detail, sizeof(record.detail), "Skill tree: %.45s", skillName ? skillName : "Unknown skill");
        } else {
            CopyText(record.detail, sizeof(record.detail), "Skill tree unavailable in loaded perk data");
        }
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void CaptureNewPlayerShoutsAndPerks(RE::PlayerCharacter* player)
    {
        if (!player) return;
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return;
        std::vector<RE::TESShout*> shouts;
        std::vector<RE::BGSPerk*> perks;
        for (auto* shout : dataHandler->GetFormArray<RE::TESShout>()) {
            if (shout && player->HasShout(shout)) shouts.push_back(shout);
        }
        for (auto* perk : dataHandler->GetFormArray<RE::BGSPerk>()) {
            if (perk && player->HasPerk(perk)) perks.push_back(perk);
        }

        std::vector<RE::TESShout*> newShouts;
        std::vector<RE::BGSPerk*> newPerks;
        {
            std::scoped_lock lock(g_unlockSnapshotMutex);
            if (!g_unlockSnapshotInitialized) {
                for (auto* shout : shouts) g_knownPlayerShouts.insert(shout->GetFormID());
                for (auto* perk : perks) g_knownPlayerPerks.insert(perk->GetFormID());
                g_unlockSnapshotInitialized = true;
                return;
            }
            for (auto* shout : shouts) {
                if (g_knownPlayerShouts.insert(shout->GetFormID()).second) newShouts.push_back(shout);
            }
            for (auto* perk : perks) {
                if (g_knownPlayerPerks.insert(perk->GetFormID()).second) newPerks.push_back(perk);
            }
        }
        for (auto* shout : newShouts) RecordPlayerShoutLearned(shout, player);
        for (auto* perk : newPerks) RecordPlayerPerkUnlocked(perk, player);
    }

    void RecordPlayerSkillIncrease(RE::ActorValue actorValue, RE::PlayerCharacter* player)
    {
        if (!player) return;
        const auto actorValueIndex = static_cast<std::int32_t>(actorValue);
        if (actorValueIndex < 0 || actorValueIndex >= static_cast<std::int32_t>(RE::ActorValue::kTotal)) return;
        auto* info = RE::ActorValueList::GetActorValueInfo(actorValue);
        if (!info || info->type != RE::ActorValueInfo::ActorValueType::kSkill) return;
        const auto actorValueKey = static_cast<std::uint32_t>(actorValueIndex);
        auto* actorValueOwner = player->AsActorValueOwner();
        if (!actorValueOwner) return;
        const float currentValue = actorValueOwner->GetActorValue(actorValue);
        float amount = 1.0F;
        {
            std::scoped_lock lock(g_skillSnapshotMutex);
            const auto found = g_playerSkillValues.find(actorValueKey);
            if (found != g_playerSkillValues.end()) {
                const float delta = currentValue - found->second;
                if (std::isfinite(delta) && delta > 0.0F) amount = delta;
                found->second = currentValue;
            } else {
                g_playerSkillValues.emplace(actorValueKey, currentValue);
            }
        }

        const auto* skillName = RE::ActorValueList::GetActorValueName(actorValue);
        if (!skillName || !*skillName) skillName = info->GetName();
        EventRecord record{};
        record.type = kEventSkillIncreased;
        record.gameDay = CurrentGameDay();
        record.formID = info ? info->GetFormID() : 0;
        if (info) CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), info);
        AddLocation(record, player);
        CopyText(record.subject, sizeof(record.subject), skillName && *skillName ? skillName : "Unknown skill");
        record.stage = static_cast<std::uint16_t>(std::clamp(std::lround(amount), 1L, static_cast<long>(std::numeric_limits<std::uint16_t>::max())));
        std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/SkillIncrease/v1/%u", actorValueKey);
        std::snprintf(record.detail, sizeof(record.detail), "Increased by %.1f; current skill level %.1f", static_cast<double>(amount), static_cast<double>(currentValue));
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void SchedulePlayerSkillIncrease(const RE::SkillIncrease::Event* event)
    {
        if (!event || !event->player) return;
        const auto actorValue = event->actorValue;
        const auto playerFormID = event->player->GetFormID();
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        tasks->AddTask([actorValue, playerFormID] {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || player->GetFormID() != playerFormID) return;
            RecordPlayerSkillIncrease(actorValue, player);
        });
    }

    void RecordPlayerLevelIncrease(const RE::LevelIncrease::Event* event, RE::PlayerCharacter* player)
    {
        if (!event || !player || event->player != player) return;
        const auto currentLevel = event->newLevel ? event->newLevel : static_cast<std::uint16_t>(player->GetLevel());
        std::uint16_t previousLevel{};
        {
            std::scoped_lock lock(g_skillSnapshotMutex);
            previousLevel = g_lastPlayerLevel;
            g_lastPlayerLevel = currentLevel;
        }
        const auto amount = static_cast<std::uint16_t>(previousLevel && currentLevel > previousLevel ? currentLevel - previousLevel : 1);
        EventRecord record{};
        record.type = kEventLevelIncreased;
        record.gameDay = CurrentGameDay();
        record.formID = player->GetFormID();
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), player);
        AddLocation(record, player);
        CopyText(record.subject, sizeof(record.subject), "Player level");
        record.stage = amount;
        CopyText(record.identity, sizeof(record.identity), "SkyRewrite/LevelIncrease/v1");
        std::snprintf(record.detail, sizeof(record.detail), "Level increased by %u; current player level %u",
            static_cast<unsigned>(amount), static_cast<unsigned>(currentLevel));
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void SnapshotKnownIngredientEffects()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return;
        std::unordered_map<RE::FormID, std::uint16_t> known;
        for (auto* ingredient : dataHandler->GetFormArray<RE::IngredientItem>()) {
            if (ingredient) known.emplace(ingredient->GetFormID(), ingredient->gamedata.knownEffectFlags);
        }
        std::scoped_lock lock(g_ingredientEffectsMutex);
        g_knownIngredientEffects = std::move(known);
    }

    void CaptureNewIngredientEffects(RE::FormID ingredientID)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* ingredient = RE::TESForm::LookupByID<RE::IngredientItem>(ingredientID);
        if (!player || !ingredient) return;
        const auto currentFlags = ingredient->gamedata.knownEffectFlags;
        std::uint16_t newFlags{};
        {
            std::scoped_lock lock(g_ingredientEffectsMutex);
            const auto found = g_knownIngredientEffects.find(ingredientID);
            if (found == g_knownIngredientEffects.end()) {
                g_knownIngredientEffects.emplace(ingredientID, currentFlags);
                return;
            }
            newFlags = static_cast<std::uint16_t>(currentFlags & ~found->second);
            found->second = currentFlags;
        }
        if (!newFlags) return;

        for (std::size_t index = 0; index < ingredient->effects.size() && index < 16; ++index) {
            if (!(newFlags & (static_cast<std::uint16_t>(1U) << index))) continue;
            auto* effect = ingredient->effects[index];
            auto* setting = effect ? effect->baseEffect : nullptr;
            if (!setting) continue;
            EventRecord record{};
            record.type = kEventIngredientEffectLearned;
            record.gameDay = CurrentGameDay();
            record.formID = setting->GetFormID();
            record.baseFormID = ingredient->GetFormID();
            CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), setting);
            CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), ingredient);
            AddLocation(record, player);
            const auto* effectName = ReferenceDisplayName(setting);
            const auto* ingredientName = ReferenceDisplayName(ingredient);
            CopyText(record.subject, sizeof(record.subject), effectName ? effectName : "Unknown effect");
            std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/IngredientEffect/v1/%08X/%08X", ingredientID, setting->GetFormID());
            std::snprintf(record.detail, sizeof(record.detail), "Learned after eating: %.72s", ingredientName ? ingredientName : "Unknown ingredient");
            AddEvent(record);
            g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void ScheduleIngredientEffectCapture(RE::FormID ingredientID)
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (tasks && ingredientID) tasks->AddTask([ingredientID] {
            auto* followup = SKSE::GetTaskInterface();
            if (followup) followup->AddTask([ingredientID] { CaptureNewIngredientEffects(ingredientID); });
        });
    }

    void ScheduleAllIngredientEffectCapture()
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        tasks->AddTask([] {
            auto* followup = SKSE::GetTaskInterface();
            if (!followup) return;
            followup->AddTask([] {
                auto* dataHandler = RE::TESDataHandler::GetSingleton();
                if (!dataHandler) return;
                for (auto* ingredient : dataHandler->GetFormArray<RE::IngredientItem>()) {
                    if (ingredient) CaptureNewIngredientEffects(ingredient->GetFormID());
                }
            });
        });
    }

    void RecordBookRead(const RE::BooksRead::Event* event, RE::PlayerCharacter* player)
    {
        if (!event || !event->book || !player) return;
        EventRecord record{};
        record.type = kEventBookRead;
        record.gameDay = CurrentGameDay();
        record.formID = event->book->GetFormID();
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), event->book);
        AddLocation(record, player);
        const auto* name = ReferenceDisplayName(event->book);
        CopyText(record.subject, sizeof(record.subject), name ? name : "Unknown book");
        CopyText(record.identity, sizeof(record.identity), event->book->GetFormEditorID());
        CopyText(record.detail, sizeof(record.detail), event->skillBook ? "Read skill book" : "Read book or note");
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void RememberRecentConsumableUse(RE::FormID formID)
    {
        if (!formID) return;
        const auto now = std::chrono::steady_clock::now();
        constexpr auto kRecentUseLifetime = std::chrono::seconds(2);
        std::scoped_lock lock(g_recentConsumableUsesMutex);
        for (auto it = g_recentConsumableUses.begin(); it != g_recentConsumableUses.end();) {
            if (now - it->second > kRecentUseLifetime) it = g_recentConsumableUses.erase(it);
            else ++it;
        }
        g_recentConsumableUses[formID] = now;
    }

    bool ConsumeRecentConsumableUse(RE::FormID formID)
    {
        if (!formID) return false;
        constexpr auto kRecentUseLifetime = std::chrono::seconds(2);
        std::scoped_lock lock(g_recentConsumableUsesMutex);
        const auto now = std::chrono::steady_clock::now();
        for (auto it = g_recentConsumableUses.begin(); it != g_recentConsumableUses.end();) {
            if (now < it->second || now - it->second > kRecentUseLifetime) it = g_recentConsumableUses.erase(it);
            else ++it;
        }
        const auto found = g_recentConsumableUses.find(formID);
        if (found == g_recentConsumableUses.end()) return false;
        return now >= found->second && now - found->second <= kRecentUseLifetime;
    }

    void RecordEquipmentChange(const RE::TESEquipEvent* event, RE::PlayerCharacter* player)
    {
        if (!event || !player || event->actor.get() != player || !event->baseObject) return;
        auto* item = RE::TESForm::LookupByID(event->baseObject);
        if (!item) return;
        if (auto* tome = item->As<RE::TESObjectBOOK>(); tome && tome->TeachesSpell()) return;
        const auto* alchemyItem = item->As<RE::AlchemyItem>();
        const bool consumable = (alchemyItem && !alchemyItem->IsPoison()) || item->As<RE::IngredientItem>();
        if (consumable) {
            if (event->equipped) RememberRecentConsumableUse(event->baseObject);
            return;
        }
        EventRecord record{};
        record.type = event->equipped ? kEventEquipped : kEventUnequipped;
        record.gameDay = CurrentGameDay();
        record.formID = event->baseObject;
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), item);
        AddLocation(record, player);
        const auto* name = ReferenceDisplayName(item);
        CopyText(record.subject, sizeof(record.subject), name ? name : "Unknown item");
        std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/Equipment/v1/%s/%08X", event->equipped ? "equipped" : "unequipped", event->baseObject);
        std::snprintf(record.detail, sizeof(record.detail), "%s item (FormID %08X)", event->equipped ? "Equipped" : "Unequipped", event->baseObject);
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void RecordFollowerState(RE::Actor* npc, RE::PlayerCharacter* player, bool captureInitialRecruitment = false)
    {
        if (!npc || !player || npc == player || npc->IsDead()) return;
        const bool isFollowing = npc->IsPlayerTeammate();
        bool changed = false;
        bool started = false;
        {
            std::scoped_lock lock(g_followerStateMutex);
            const auto found = g_playerFollowerStates.find(npc->GetFormID());
            if (found == g_playerFollowerStates.end()) {
                g_playerFollowerStates.emplace(npc->GetFormID(), isFollowing);
                // Actors can enter the loaded area while already following the
                // player. Only dialogue-driven checks can identify an initial
                // true state as a new recruitment rather than a late load.
                changed = isFollowing && captureInitialRecruitment;
                started = isFollowing;
            } else if (found->second != isFollowing) {
                found->second = isFollowing;
                changed = true;
                started = isFollowing;
            }
        }
        if (!changed) return;
        EventRecord record{};
        record.type = started ? kEventFollowerStarted : kEventFollowerStopped;
        record.gameDay = CurrentGameDay();
        record.formID = npc->GetFormID();
        auto* base = npc->GetActorBase();
        record.baseFormID = base ? base->GetFormID() : 0;
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), npc);
        CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), base);
        AddLocation(record, player);
        const auto* name = ReferenceDisplayName(npc);
        CopyText(record.subject, sizeof(record.subject), name ? name : "Unknown NPC");
        std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/Follower/v1/%08X/%s", npc->GetFormID(), started ? "started" : "stopped");
        CopyText(record.detail, sizeof(record.detail), started ? "Started following the player" : "Stopped following the player");
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void ScheduleFollowerStateCheck(RE::Actor* npc)
    {
        if (!npc) return;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        const RE::ActorHandle handle = npc->GetHandle();
        const auto formID = npc->GetFormID();
        tasks->AddTask([handle, formID] {
            auto* followup = SKSE::GetTaskInterface();
            if (!followup) return;
            followup->AddTask([handle, formID] {
                auto actorRef = handle.get();
                auto* actor = actorRef ? actorRef.get() : RE::TESForm::LookupByID<RE::Actor>(formID);
                RecordFollowerState(actor, RE::PlayerCharacter::GetSingleton(), true);
            });
        });
    }

    void SnapshotLoadedFollowerStates()
    {
        auto* processes = RE::ProcessLists::GetSingleton();
        if (!processes) return;
        std::unordered_map<RE::FormID, bool> snapshot;
        for (auto* actors : { &processes->highActorHandles, &processes->lowActorHandles,
                &processes->middleHighActorHandles, &processes->middleLowActorHandles }) {
            for (const auto& handle : *actors) {
                auto actorRef = handle.get();
                auto* actor = actorRef.get();
                if (actor && actor != RE::PlayerCharacter::GetSingleton()) {
                    snapshot[actor->GetFormID()] = actor->IsPlayerTeammate();
                }
            }
        }
        std::scoped_lock lock(g_followerStateMutex);
        g_playerFollowerStates = std::move(snapshot);
    }

    RE::BGSLocation* ResolveDiscoveredLocation(const char* markerName, RE::PlayerCharacter* player)
    {
        if (!player) return nullptr;
        auto* currentLocation = player->GetCurrentLocation();
        if (!markerName || !*markerName) return currentLocation;
        const auto matches = [markerName](RE::BGSLocation* location) {
            const auto* name = ReferenceDisplayName(location);
            return name && _stricmp(name, markerName) == 0;
        };
        for (auto* location = currentLocation; location; location = location->parentLoc) {
            if (matches(location)) return location;
        }
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (dataHandler) {
            for (auto* location : dataHandler->GetFormArray<RE::BGSLocation>()) {
                if (matches(location)) return location;
            }
        }
        // The discovery event supplies a localized map-marker name, not a
        // location form. Skyrim may not have caught the active location up
        // yet, so use the current location as the same conservative fallback
        // used by SkyCollection instead of dropping the SkyCounterUI notice.
        if (currentLocation) {
            SKSE::log::warn("SkyRewrite could not match discovered marker '{}' to a location form; using current location '{}' ({:08X}) for SkyCounterUI",
                markerName, currentLocation->GetName() ? currentLocation->GetName() : "(unnamed)", currentLocation->GetFormID());
        }
        return currentLocation;
    }

    void RecordDiscoveredMapMarker(const RE::LocationDiscovery::Event* event, RE::PlayerCharacter* player)
    {
        if (!event || !event->mapMarkerData || !player) return;
        const auto markerType = event->mapMarkerData->type.get();
        if (markerType == RE::MARKER_TYPE::kNone || static_cast<std::uint32_t>(markerType) >=
            static_cast<std::uint32_t>(RE::MARKER_TYPE::kTotalLocationTypes)) return;
        const auto* markerName = event->mapMarkerData->locationName.GetFullName();
        if (!markerName || !*markerName) return;

        EventRecord record{};
        record.type = 29;
        record.gameDay = CurrentGameDay();
        auto* cell = player->GetParentCell();
        auto* location = ResolveDiscoveredLocation(markerName, player);
        auto* screenshotSource = location ? static_cast<RE::TESForm*>(location) : static_cast<RE::TESForm*>(cell);
        if (!location) {
            SKSE::log::warn("SkyRewrite could not resolve discovered marker '{}' to a location form; sending SkyCounterUI the current cell as a capture key (cellFormID={:08X})",
                markerName, cell ? cell->GetFormID() : 0);
        }
        NotifySkyCounterLocationDiscovery(screenshotSource, markerName);
        record.formID = location ? location->GetFormID() : 0;
        record.baseFormID = cell ? cell->GetFormID() : 0;
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), location);
        CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), cell);
        AddLocation(record, player);
        CopyText(record.subject, sizeof(record.subject), markerName);
        CopyText(record.identity, sizeof(record.identity), location ? location->GetFormEditorID() : nullptr);
        const auto* cellName = CellDisplayName(cell);
        std::snprintf(record.detail, sizeof(record.detail), "Cell: %.70s", cellName ? cellName : "unavailable");
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    std::string MurderReactionStateIdentity(RE::FormID actorID)
    {
        char identity[64]{};
        std::snprintf(identity, sizeof(identity), "SkyRewrite/MurderReaction/v1/%08X", actorID);
        return identity;
    }

    bool IsMurderReactionState(const EventRecord& event);
    bool IsProtectedWorldResponseEvent(std::size_t index);

    void SaveMurderReactionState(RE::FormID actorID, std::uint8_t flags)
    {
        if (!actorID || !flags) return;
        const auto identity = MurderReactionStateIdentity(actorID);
        std::scoped_lock lock(g_eventsMutex);
        const auto existing = std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            return IsMurderReactionState(event) && std::string_view(event.identity) == identity;
        });
        if (existing != g_events.end()) {
            existing->stage = flags;
            existing->gameDay = CurrentGameDay();
            return;
        }
        const auto retention = g_journalRetention.load(std::memory_order_relaxed);
        while (g_events.size() >= retention) {
            auto removable = g_events.end();
            for (auto it = g_events.begin(); it != g_events.end(); ++it) {
                const auto index = static_cast<std::size_t>(it - g_events.begin());
                if (!IsProtectedWorldResponseEvent(index)) { removable = it; break; }
            }
            if (removable == g_events.end()) break;
            g_events.erase(removable);
        }
        EventRecord state{};
        state.type = 14;
        state.gameDay = CurrentGameDay();
        state.formID = actorID;
        state.stage = flags;
        CopyText(state.subject, sizeof(state.subject), "Murder response actor");
        CopyText(state.identity, sizeof(state.identity), identity.c_str());
        CopyText(state.detail, sizeof(state.detail), "Temporary NPC response state for murder reputation");
        g_events.push_back(state);
    }

    void RemoveMurderReactionState(RE::FormID actorID)
    {
        const auto identity = MurderReactionStateIdentity(actorID);
        std::scoped_lock lock(g_eventsMutex);
        std::erase_if(g_events, [&](const EventRecord& event) {
            return IsMurderReactionState(event) && std::string_view(event.identity) == identity;
        });
    }

    bool IsTemporaryBountyApplied(const EventRecord& event)
    {
        return event.type == 6 && std::string_view(event.identity).find("SkyRewrite/TemporaryBounty/v1/") == 0;
    }

    bool IsGuardConfidenceApplied(const EventRecord& event)
    {
        return event.type == 8 && std::string_view(event.identity).find("SkyRewrite/GuardConfidence/v1/") == 0;
    }

    bool IsRegionalBanditState(const EventRecord& event)
    {
        return event.type == 14 && std::string_view(event.identity).find("SkyRewrite/RegionalBanditState/v1/") == 0;
    }

    bool IsRegionalBanditHoldKeyState(const EventRecord& event)
    {
        return event.type == 14 && std::string_view(event.identity).find("SkyRewrite/RegionalBanditHoldKey/v1/") == 0;
    }

    bool IsRegionalCreatureState(const EventRecord& event)
    {
        return event.type == 14 && std::string_view(event.identity).find("SkyRewrite/RegionalCreatureState/v1/") == 0;
    }

    bool IsMurderReactionState(const EventRecord& event)
    {
        return event.type == 14 && std::string_view(event.identity).find("SkyRewrite/MurderReaction/v1/") == 0;
    }

    bool IsReputationState(const EventRecord& event)
    {
        return event.type == 16 && (std::string_view(event.identity).find("SkyRewrite/ReputationFavor/v1/") == 0 ||
            std::string_view(event.identity) == "SkyRewrite/ReputationGift/v1/last");
    }

    bool IsReputationUniqueGiftState(const EventRecord& event)
    {
        return event.type == 16 && std::string_view(event.identity).find("SkyRewrite/ReputationUniqueGift/v1/") == 0;
    }

    bool IsRegionalShopRobberyState(const EventRecord& event)
    {
        return event.type == 14 && std::string_view(event.identity).find("SkyRewrite/RegionalShopRobbery/v1/") == 0;
    }

    bool IsInternalWorldState(const EventRecord& event)
    {
        return IsRegionalBanditState(event) || IsRegionalBanditHoldKeyState(event) || IsRegionalCreatureState(event) ||
            IsMurderReactionState(event) || IsReputationState(event) || IsReputationUniqueGiftState(event) || IsRegionalShopRobberyState(event);
    }

    std::string RegionalHoldKey(std::string_view text)
    {
        std::string normalized;
        normalized.reserve(text.size());
        for (const auto ch : text) {
            const auto lowered = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (std::isalnum(static_cast<unsigned char>(lowered))) normalized.push_back(lowered);
        }
        constexpr std::array<std::pair<std::string_view, std::string_view>, 10> holds{{
            {"eastmarch", "eastmarch"}, {"windhelm", "eastmarch"},
            {"falkreath", "falkreath"},
            {"haafingar", "haafingar"}, {"solitude", "haafingar"},
            {"hjaalmarch", "hjaalmarch"}, {"morthal", "hjaalmarch"},
            {"thepale", "pale"}, {"dawnstar", "pale"}, {"pale", "pale"}
        }};
        for (const auto& [needle, key] : holds) if (normalized.find(needle) != std::string::npos) return std::string(key);
        constexpr std::array<std::pair<std::string_view, std::string_view>, 10> remainingHolds{{
            {"thereach", "reach"}, {"markarth", "reach"}, {"reach", "reach"},
            {"therift", "rift"}, {"riften", "rift"}, {"rift", "rift"},
            {"whiterun", "whiterun"}, {"winterhold", "winterhold"},
            {"solstheim", "solstheim"}, {"ravenrock", "solstheim"}
        }};
        for (const auto& [needle, key] : remainingHolds) if (normalized.find(needle) != std::string::npos) return std::string(key);
        return {};
    }

    std::string RegionalHoldKeyForLocation(RE::BGSLocation* location)
    {
        for (auto* current = location; current; current = current->parentLoc) {
            if (const auto* editorID = current->GetFormEditorID(); editorID && *editorID) {
                if (auto key = RegionalHoldKey(editorID); !key.empty()) return key;
            }
            if (const auto* name = current->GetName(); name && *name) {
                if (auto key = RegionalHoldKey(name); !key.empty()) return key;
            }
        }
        return {};
    }

    const char* RegionalHoldLabel(std::string_view key)
    {
        if (key == "eastmarch") return "Eastmarch";
        if (key == "falkreath") return "Falkreath";
        if (key == "haafingar") return "Haafingar";
        if (key == "hjaalmarch") return "Hjaalmarch";
        if (key == "pale") return "The Pale";
        if (key == "reach") return "The Reach";
        if (key == "rift") return "The Rift";
        if (key == "whiterun") return "Whiterun";
        if (key == "winterhold") return "Winterhold";
        if (key == "solstheim") return "Solstheim";
        return "All holds";
    }

    std::string LowerAscii(std::string_view text)
    {
        std::string lowered;
        lowered.reserve(text.size());
        for (const auto ch : text) lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        return lowered;
    }

    std::uint16_t InferVictimCategories(std::string_view subject, std::string_view identity)
    {
        auto text = LowerAscii(subject);
        text.push_back(' ');
        text += LowerAscii(identity);
        std::uint16_t flags{};
        const auto has = [&](std::initializer_list<std::string_view> needles) {
            return std::any_of(needles.begin(), needles.end(), [&](std::string_view needle) { return text.find(needle) != std::string::npos; });
        };
        if (has({"bandit", "marauder"})) flags |= kVictimBandit;
        if (has({"dragon"}) && !has({"dragon priest", "dragonpriest"})) flags |= kVictimDragon | kVictimMonster;
        if (has({"forsworn"})) flags |= kVictimForsworn;
        if (has({"vampire"})) flags |= kVictimVampire;
        if (has({"wolf"})) flags |= kVictimWolf | kVictimMonster;
        if (has({"bear"})) flags |= kVictimBear | kVictimMonster;
        if (has({"spider"})) flags |= kVictimSpider | kVictimMonster;
        if (has({"golden saint", "goldensaint", "dark seducer", "darkseducer"})) flags |= kVictimAllHostile;
        if (has({"sabre cat", "sabrecat", "troll", "giant", "draugr", "falmer", "dragon priest", "dragonpriest", "skeleton", "hagraven", "dwarven", "dwemer", "centurion", "skeever", "spriggan", "atronach", "dremora", "gargoyle", "death hound", "deathhound", "ash spawn", "ashspawn", "ash hopper", "ashhopper", "ash guardian", "seeker", "lurker", "riekling", "netch", "werewolf", "werebear", "chaurus", "ice wraith", "icewraith", "wispmother", "wisp mother", "wisp", "ghost", "shade", "boneman", "bone man", "mistman", "mist man", "wrathman", "wrath man", "magic anomaly", "magical anomaly", "mudcrab", "slaughterfish", "horker", "mammoth", "ballista"})) flags |= kVictimMonster;
        if (flags & (kVictimBandit | kVictimDragon | kVictimForsworn | kVictimVampire | kVictimWolf | kVictimBear | kVictimSpider | kVictimMonster)) flags |= kVictimAllHostile;
        return flags;
    }

    std::string SpecificVictimCategory(std::string_view subject, std::string_view identity)
    {
        auto text = LowerAscii(subject);
        text.push_back(' ');
        text += LowerAscii(identity);
        if (text.find("golden saint") != std::string::npos || text.find("goldensaint") != std::string::npos) return "goldensaint";
        if (text.find("dark seducer") != std::string::npos || text.find("darkseducer") != std::string::npos) return "darkseducer";
        if (text.find("saint") != std::string::npos && text.find("bandit") != std::string::npos) return "saintsbandit";
        if (text.find("seducer") != std::string::npos && text.find("bandit") != std::string::npos) return "seducersbandit";
        if (text.find("bandit") != std::string::npos || text.find("marauder") != std::string::npos) return "bandit";
        if (text.find("forsworn") != std::string::npos) return "forsworn";
        if (text.find("vampire") != std::string::npos) return "vampire";
        if (text.find("thalmor justiciar") != std::string::npos || text.find("thalmorjusticiar") != std::string::npos ||
            text.find("thalmore justiciar") != std::string::npos || text.find("thalmorejusticiar") != std::string::npos) return "thalmorjusticiar";
        if (text.find("thalmor") != std::string::npos || text.find("thalmore") != std::string::npos) return "thalmor";
        if (text.find("silver hand") != std::string::npos || text.find("silverhand") != std::string::npos) return "silverhand";
        if (text.find("penitus oculatus") != std::string::npos || text.find("penitusoculatus") != std::string::npos) return "penitusoculatus";
        if (text.find("dawnguard") != std::string::npos) return "dawnguard";
        if (text.find("vigilant") != std::string::npos) return "vigilant";
        if (text.find("imperial soldier") != std::string::npos || text.find("imperial captain") != std::string::npos || text.find("imperial legion") != std::string::npos) return "imperialsoldier";
        if (text.find("stormcloak") != std::string::npos || text.find("storm cloak") != std::string::npos) return "stormcloaksoldier";
        if (text.find("hired thug") != std::string::npos || text.find("hiredthug") != std::string::npos) return "hiredthug";
        if (text.find("mercenary") != std::string::npos) return "mercenary";
        if (text.find("assassin") != std::string::npos) return "assassin";
        if (text.find("necromancer") != std::string::npos) return "necromancer";
        if (text.find("pyromancer") != std::string::npos) return "pyromancer";
        if (text.find("cryomancer") != std::string::npos) return "cryomancer";
        if (text.find("electromancer") != std::string::npos) return "electromancer";
        if (text.find("conjurer") != std::string::npos) return "conjurer";
        if (text.find("warlock") != std::string::npos || text.find("rogue") != std::string::npos) return "roguewarlock";
        if (text.find("afflicted") != std::string::npos) return "afflicted";
        if (text.find("glenmoril") != std::string::npos || text.find("witch") != std::string::npos) return "witch";
        if (text.find("dragon cultist") != std::string::npos || text.find("dragoncultist") != std::string::npos) return "dragoncultist";
        if (text.find("mythic dawn") != std::string::npos) return "mythicdawncultist";
        if (text.find("miraak cultist") != std::string::npos || text.find("cultist of miraak") != std::string::npos) return "miraakcultist";
        if (text.find("cultist") != std::string::npos) return "cultist";
        if (text.find("reaver") != std::string::npos) return "reaver";
        if (text.find("riekling") != std::string::npos) return "riekling";
        if (text.find("chaurus") != std::string::npos) return "chaurus";
        if (text.find("ice wraith") != std::string::npos) return "icewraith";
        if (text.find("magic anomaly") != std::string::npos || text.find("magical anomaly") != std::string::npos) return "magicanomaly";
        if (text.find("spriggan") != std::string::npos) return "spriggan";
        if (text.find("troll") != std::string::npos) return "troll";
        if (text.find("werewolf") != std::string::npos || text.find("werebear") != std::string::npos) return "werewolf";
        if (text.find("wispmother") != std::string::npos || text.find("wisp mother") != std::string::npos) return "wispmother";
        if (text.find("wisp") != std::string::npos) return "wisp";
        if (text.find("gargoyle") != std::string::npos) return "gargoyle";
        if (text.find("death hound") != std::string::npos || text.find("deathhound") != std::string::npos) return "deathhound";
        if (text.find("ash spawn") != std::string::npos || text.find("ashspawn") != std::string::npos) return "ashspawn";
        if (text.find("ash hopper") != std::string::npos || text.find("ashhopper") != std::string::npos) return "ashhopper";
        if (text.find("ash guardian") != std::string::npos || text.find("ashguardian") != std::string::npos) return "ashguardian";
        if (text.find("netch") != std::string::npos) return "netch";
        if (text.find("dragon priest") != std::string::npos || text.find("dragonpriest") != std::string::npos) return "dragonpriest";
        if (text.find("boneman") != std::string::npos || text.find("bone man") != std::string::npos) return "boneman";
        if (text.find("mistman") != std::string::npos || text.find("mist man") != std::string::npos) return "mistman";
        if (text.find("wrathman") != std::string::npos || text.find("wrath man") != std::string::npos) return "wrathman";
        if (text.find("ghost") != std::string::npos || text.find("shade") != std::string::npos) return "ghost";
        if (text.find("giant") != std::string::npos) return "giant";
        if (text.find("sabre cat") != std::string::npos || text.find("sabrecat") != std::string::npos) return "sabrecat";
        if (text.find("skeever") != std::string::npos) return "skeever";
        if (text.find("slaughterfish") != std::string::npos) return "slaughterfish";
        if (text.find("mudcrab") != std::string::npos) return "mudcrab";
        if (text.find("horker") != std::string::npos) return "horker";
        if (text.find("mammoth") != std::string::npos) return "mammoth";
        if (text.find("dwarven ballista") != std::string::npos || text.find("dwemer ballista") != std::string::npos) return "dwarvenballista";
        if (text.find("dremora") != std::string::npos) return "dremora";
        if (text.find("atronach") != std::string::npos) return "atronach";
        if (text.find("centurion") != std::string::npos) return "dwarvencenturion";
        if (text.find("sphere guardian") != std::string::npos || text.find("dwarven sphere") != std::string::npos ||
            text.find("dwarvensphere") != std::string::npos) return "dwarvensphere";
        if (text.find("spider guardian") != std::string::npos || text.find("dwarven spider") != std::string::npos ||
            text.find("dwarvenspider") != std::string::npos) return "dwarvenspider";
        if (text.find("spider") != std::string::npos) return "spider";
        if (text.find("dragon priest") != std::string::npos || text.find("dragonpriest") != std::string::npos) return "dragonpriest";
        if (text.find("dragon") != std::string::npos) return "dragon";
        if (text.find("draugr") != std::string::npos) return "draugr";
        if (text.find("falmer") != std::string::npos) return "falmer";
        if (text.find("skeleton") != std::string::npos) return "skeleton";
        if (text.find("hagraven") != std::string::npos) return "hagraven";
        if (text.find("spriggan") != std::string::npos) return "spriggan";
        if (text.find("wolf") != std::string::npos) return "wolf";
        if (text.find("bear") != std::string::npos) return "bear";
        if (text.find("dwarven") != std::string::npos || text.find("dwemer") != std::string::npos ||
            text.find("centurion") != std::string::npos || text.find("sphere guardian") != std::string::npos ||
            text.find("spider guardian") != std::string::npos || text.find("ballista") != std::string::npos) return "dwarven";
        return {};
    }

    bool EventMatchesVictimCategory(const EventRecord& event, std::uint16_t category)
    {
        if (category == kVictimAllHostile) {
            if (event.stage & kDispositionKnown) return (event.stage & kVictimHostile) != 0;
            if (event.stage & category) return true;
            return (InferVictimCategories(event.subject, event.identity) & category) != 0;
        }
        if (event.stage & category) return true;
        const auto inferred = InferVictimCategories(event.subject, event.identity);
        if (category == kVictimOtherHostile) {
            return (event.stage & kVictimOtherHostile) != 0 ||
                (((event.stage & kDispositionKnown) && (event.stage & kVictimHostile)) &&
                    !(event.stage & (kVictimBandit | kVictimDragon | kVictimForsworn | kVictimVampire | kVictimWolf | kVictimBear | kVictimSpider | kVictimMonster)));
        }
        return (inferred & category) != 0;
    }

    bool HasGuardConfidenceResolution(std::size_t index)
    {
        if (index >= g_events.size() || !IsGuardConfidenceApplied(g_events[index])) return false;
        return std::any_of(g_events.begin() + static_cast<std::ptrdiff_t>(index + 1), g_events.end(), [&](const EventRecord& event) {
            return event.type == 9 && std::string_view(event.identity) == g_events[index].identity;
        });
    }

    bool HasActiveGuardConfidence(const RE::Actor* actor)
    {
        if (!actor) return false;
        std::scoped_lock lock(g_eventsMutex);
        for (std::size_t i = 0; i < g_events.size(); ++i) {
            if (IsGuardConfidenceApplied(g_events[i]) && g_events[i].formID == actor->GetFormID() && !HasGuardConfidenceResolution(i)) return true;
        }
        return false;
    }

    float GuardConfidenceDuration(const EventRecord& event)
    {
        const auto* separator = std::strrchr(event.identity, '/');
        float duration = 1.0F;
        if (!separator || std::sscanf(separator + 1, "%f", &duration) != 1 || !std::isfinite(duration)) return 1.0F;
        return std::clamp(duration, 0.5F, 7.0F);
    }

    bool HasBountyResolution(std::size_t index);

    bool IsProtectedWorldResponseEvent(std::size_t index)
    {
        if (index >= g_events.size()) return false;
        if (g_events[index].type == 19) return true;
        if (IsInternalWorldState(g_events[index])) return true;
        if (IsTemporaryBountyApplied(g_events[index])) return !HasBountyResolution(index);
        if (IsGuardConfidenceApplied(g_events[index])) return !HasGuardConfidenceResolution(index);
        if (g_events[index].type == 7) {
            return std::any_of(g_events.begin(), g_events.end(), [&](const EventRecord& applied) {
                return IsTemporaryBountyApplied(applied) && std::string_view(applied.identity) == g_events[index].identity;
            });
        }
        if (g_events[index].type == 9) {
            return std::any_of(g_events.begin(), g_events.end(), [&](const EventRecord& applied) {
                return IsGuardConfidenceApplied(applied) && std::string_view(applied.identity) == g_events[index].identity;
            });
        }
        if (g_events[index].type == 13) {
            return std::any_of(g_events.begin(), g_events.end(), [&](const EventRecord& applied) {
                return IsTemporaryBountyApplied(applied) && std::string_view(applied.identity) == g_events[index].identity;
            });
        }
        return false;
    }

    float TemporaryBountyDuration(const EventRecord& event)
    {
        const std::string_view identity(event.identity);
        if (std::count(identity.begin(), identity.end(), '/') < 5) return 1.0F;
        const auto* separator = std::strrchr(event.identity, '/');
        float duration = 1.0F;
        if (!separator || std::sscanf(separator + 1, "%f", &duration) != 1 || !std::isfinite(duration)) return 1.0F;
        return std::clamp(duration, 0.5F, 7.0F);
    }

    RE::TESFaction* ResolveCurrentHoldCrimeFaction(RE::PlayerCharacter* player, RE::BGSLocation* location)
    {
        // Resolve from the crime location first. PlayerCharacter::GetCrimeFaction
        // can still refer to a different hold (or the last crime context), which
        // would apply or suppress the bounty against the wrong jurisdiction.
        for (auto* current = location; current; current = current->parentLoc) {
            if (current->unreportedCrimeFaction) return current->unreportedCrimeFaction;
        }
        if (player) {
            if (auto* faction = player->GetCrimeFaction()) return faction;
        }
        return nullptr;
    }

    RE::BGSLocation* LookupLocation(RE::FormID formID)
    {
        auto* form = formID ? RE::TESForm::LookupByID(formID) : nullptr;
        return form && form->Is(RE::BGSLocation::FORMTYPE) ? static_cast<RE::BGSLocation*>(form) : nullptr;
    }

    std::string RegionalHoldKeyForEvent(const EventRecord& event)
    {
        if (auto key = RegionalHoldKeyForLocation(LookupLocation(event.locationFormID)); !key.empty()) return key;
        return RegionalHoldKey(event.location);
    }

    enum class GuardDispatchResult { dispatched, partiallyDispatched, noMatchingGuard, spawnFailed };

    GuardDispatchResult SpawnHoldGuardResponse(RE::PlayerCharacter* player, RE::TESFaction* faction,
        std::uint32_t requestedGuards = 1, std::uint32_t* spawnedGuards = nullptr)
    {
        if (spawnedGuards) *spawnedGuards = 0;
        if (!player || !faction) return GuardDispatchResult::noMatchingGuard;
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) return GuardDispatchResult::noMatchingGuard;

        // Prefer a concrete guard base already loaded for this hold. NPC
        // templates in the form array often lack the runtime crime-faction
        // assignment used by placed guards, and are unreliable PlaceAtMe bases.
        std::vector<std::pair<RE::TESNPC*, int>> candidates;
        if (auto* processes = RE::ProcessLists::GetSingleton()) {
            processes->ForAllActors([&](RE::Actor* actor) {
                if (!actor || actor == player || actor->IsDead() || !actor->IsGuard()) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                auto* base = actor->GetActorBase();
                if ((actor->GetCrimeFaction() == faction || (base && base->crimeFaction == faction)) && base &&
                    std::none_of(candidates.begin(), candidates.end(), [&](const auto& item) { return item.first == base; })) {
                    candidates.emplace_back(base, 2);
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
        std::string factionName = faction->GetFormEditorID() ? faction->GetFormEditorID() : "";
        std::transform(factionName.begin(), factionName.end(), factionName.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::string holdName = factionName;
        constexpr std::string_view kCrimeFactionPrefix = "crimefaction";
        if (holdName.rfind(kCrimeFactionPrefix, 0) == 0) holdName.erase(0, kCrimeFactionPrefix.size());
        for (auto* npc : data->GetFormArray<RE::TESNPC>()) {
            if (!npc) continue;
            const auto* editorID = npc->GetFormEditorID();
            const auto* displayName = npc->GetName();
            std::string candidate = editorID ? editorID : "";
            if (displayName && *displayName) {
                if (!candidate.empty()) candidate.push_back(' ');
                candidate += displayName;
            }
            std::transform(candidate.begin(), candidate.end(), candidate.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (candidate.find("guard") == std::string::npos) continue;
            // Guard bases often list the hold in their faction membership
            // rather than TESNPC::crimeFaction. Match either faction membership
            // or an explicit hold name, preferring the latter for this spawn.
            const bool namesCurrentHold = !holdName.empty() && candidate.find(holdName) != std::string::npos;
            if (npc->crimeFaction != faction && !npc->IsInFaction(faction) && !namesCurrentHold) continue;
            if (std::none_of(candidates.begin(), candidates.end(), [&](const auto& item) { return item.first == npc; })) {
                candidates.emplace_back(npc, namesCurrentHold ? 1 : 0);
            }
        }
        if (candidates.empty()) return GuardDispatchResult::noMatchingGuard;

        std::stable_sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
            return left.second > right.second;
        });
        requestedGuards = std::max(1U, requestedGuards);
        std::uint32_t spawned = 0;
        while (spawned < requestedGuards) {
            bool createdGuard = false;
            for (const auto& [guardBase, priority] : candidates) {
                // Use Skyrim's default non-persistent placement first. A persistent
                // fallback helps with NPC bases that require a saved reference.
                auto placed = player->PlaceObjectAtMe(guardBase, false);
                if (!placed) placed = player->PlaceObjectAtMe(guardBase, true);
                if (!placed) {
                    SKSE::log::warn("SkyRewrite could not create a guard reference from base {:08X} (priority {})", guardBase->GetFormID(), priority);
                    continue;
                }
                auto* guard = placed->As<RE::Actor>();
                if (!guard) {
                    SKSE::log::warn("SkyRewrite created reference {:08X} from guard base {:08X}, but it did not resolve as an Actor",
                        placed->GetFormID(), guardBase->GetFormID());
                    continue;
                }
                guard->SetTemporary();
                guard->MoveToNearestNavmesh(96.0F);
                auto* factionChanges = guard->extraList.GetByType<RE::ExtraFactionChanges>();
                if (!factionChanges) {
                    factionChanges = RE::BSExtraData::Create<RE::ExtraFactionChanges>();
                    if (factionChanges) guard->extraList.Add(factionChanges);
                }
                if (factionChanges) {
                    factionChanges->crimeFaction = faction;
                    factionChanges->removeCrimeFaction = false;
                }
                guard->AddToFaction(faction, 0);
                guard->EvaluatePackage(true, true);
                ++spawned;
                createdGuard = true;
                break;
            }
            if (!createdGuard) break;
        }
        if (spawnedGuards) *spawnedGuards = spawned;
        if (spawned == requestedGuards) return GuardDispatchResult::dispatched;
        return spawned ? GuardDispatchResult::partiallyDispatched : GuardDispatchResult::spawnFailed;
    }

    bool ReferenceWasStolenFrom(RE::PlayerCharacter* player, RE::TESObjectREFR* target)
    {
        if (!player || !target) return false;
        // Looting a dead actor is not a theft in Skyrim, even if the corpse
        // still reports its former owner.
        if (target->IsActor() && static_cast<RE::Actor*>(target)->IsDead()) return false;

        // Container-change notifications can arrive after the source's item
        // ownership has already been detached. Preserve an explicit owner
        // check as a fallback for owned containers and pickpocket targets.
        auto* owner = target->GetOwner();
        if (owner == player || owner == player->GetActorBase()) return false;
        if (owner && owner->IsActor() && static_cast<RE::Actor*>(owner)->IsDead()) return false;
        return player->WouldBeStealing(target) || owner != nullptr;
    }

    struct RecentStealContext
    {
        RE::FormID sourceReferenceID{};
        bool wouldBeStealing{};
        std::chrono::steady_clock::time_point capturedAt{};
    };

    std::mutex g_recentStealContextMutex;
    RecentStealContext g_recentStealContext;
    std::atomic<RE::FormID> g_recentStorageTargetID{};
    std::atomic<std::int64_t> g_recentStorageTargetAtMs{};
    struct GuardDialogueSnapshot
    {
        RE::FormID crimeFactionID{};
        std::uint32_t crimeGold{};
        std::int32_t playerGold{};
        bool playerGoldAvailable{};
        std::chrono::steady_clock::time_point capturedAt{};
    };
    std::mutex g_guardDialogueMutex;
    std::unordered_map<RE::FormID, GuardDialogueSnapshot> g_guardDialogueSnapshots;
    struct PlayerDialogueSelection
    {
        RE::FormID topicInfoID{};
        std::chrono::steady_clock::time_point capturedAt{};
    };
    std::mutex g_playerDialogueMutex;
    std::unordered_map<RE::FormID, PlayerDialogueSelection> g_playerDialogueSelections;
    struct RecentConsumptionNotification
    {
        RE::FormID locationFormID{};
        long long quantity{};
        std::chrono::steady_clock::time_point capturedAt{};
    };
    std::mutex g_recentConsumptionMutex;
    std::unordered_map<RE::FormID, RecentConsumptionNotification> g_recentConsumptionNotifications;
    struct RecentConsumableDrop
    {
        RE::FormID locationFormID{};
        std::uint16_t quantity{};
        std::chrono::steady_clock::time_point capturedAt{};
    };
    std::mutex g_recentConsumableDropsMutex;
    std::unordered_map<RE::FormID, RecentConsumableDrop> g_recentConsumableDrops;
    std::atomic_bool g_barterMenuOpen{ false };
    std::atomic<std::int64_t> g_lastBarterMenuCloseMs{ 0 };
    std::atomic<RE::FormID> g_recentBarterVendorFormID{ 0 };
    std::atomic<RE::FormID> g_recentDialogueSpeakerFormID{ 0 };
    std::atomic<std::int64_t> g_recentDialogueSpeakerAtMs{ 0 };

    bool IsDuplicateConsumptionNotification(RE::FormID formID, RE::FormID locationFormID, long long quantity)
    {
        if (!formID || quantity <= 0) return false;
        const auto now = std::chrono::steady_clock::now();
        constexpr auto kDuplicateWindow = std::chrono::milliseconds(250);
        constexpr auto kStaleWindow = std::chrono::seconds(2);
        std::scoped_lock lock(g_recentConsumptionMutex);
        for (auto it = g_recentConsumptionNotifications.begin(); it != g_recentConsumptionNotifications.end();) {
            if (now - it->second.capturedAt > kStaleWindow) it = g_recentConsumptionNotifications.erase(it);
            else ++it;
        }
        const auto found = g_recentConsumptionNotifications.find(formID);
        if (found != g_recentConsumptionNotifications.end() &&
            found->second.locationFormID == locationFormID && found->second.quantity == quantity &&
            now - found->second.capturedAt <= kDuplicateWindow) {
            SKSE::log::warn("SkyRewrite ignored duplicate consumption notification for {:08X} (quantity {}, location {:08X})",
                formID, quantity, locationFormID);
            return true;
        }
        if (g_recentConsumptionNotifications.size() >= 128 && found == g_recentConsumptionNotifications.end()) {
            g_recentConsumptionNotifications.clear();
        }
        g_recentConsumptionNotifications[formID] = { locationFormID, quantity, now };
        return false;
    }

    bool IsPairedDropAfterConsumption(RE::FormID formID, RE::FormID locationFormID, long long quantity)
    {
        if (!formID || quantity <= 0) return false;
        const auto now = std::chrono::steady_clock::now();
        constexpr auto kPairedRemovalWindow = std::chrono::seconds(2);
        std::scoped_lock lock(g_recentConsumptionMutex);
        const auto found = g_recentConsumptionNotifications.find(formID);
        if (found == g_recentConsumptionNotifications.end()) return false;
        const bool paired = found->second.locationFormID == locationFormID &&
            found->second.quantity == quantity && now >= found->second.capturedAt &&
            now - found->second.capturedAt <= kPairedRemovalWindow;
        if (paired) {
            SKSE::log::warn("SkyRewrite ignored a paired drop notification after consuming {:08X} (quantity {}, location {:08X})",
                formID, quantity, locationFormID);
        }
        return paired;
    }

    std::int64_t SteadyClockMilliseconds()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::string RecentAlchemyCraftName()
    {
        constexpr std::int64_t kRecentCraftNameLifetimeMs = 2000;
        std::scoped_lock lock(g_recentAlchemyCraftNameMutex);
        if (!g_recentAlchemyCraftName[0] ||
            SteadyClockMilliseconds() - g_recentAlchemyCraftNameAtMs > kRecentCraftNameLifetimeMs) return {};
        return g_recentAlchemyCraftName.data();
    }

    bool ContainsInsensitive(const char* text, std::string_view needle)
    {
        if (!text || needle.empty()) return false;
        const std::string_view value(text);
        if (needle.size() > value.size()) return false;
        return std::search(value.begin(), value.end(), needle.begin(), needle.end(), [](unsigned char left, unsigned char right) {
            return std::tolower(left) == std::tolower(right);
        }) != value.end();
    }

    bool IsPlaceholderAlchemyName(const char* name)
    {
        if (!name || !*name) return true;
        return _stricmp(name, "Potion") == 0 || _stricmp(name, "Poison") == 0 ||
            ContainsInsensitive(name, "unknown potion") || ContainsInsensitive(name, "unknown poison") ||
            ContainsInsensitive(name, "unknown item") || ContainsInsensitive(name, "item formid") ||
            ContainsInsensitive(name, "unknown effect") || ContainsInsensitive(name, "potion formid") ||
            ContainsInsensitive(name, "poison formid") || ContainsInsensitive(name, "effect name unavailable");
    }

    bool IsPlaceholderAlchemyEffectName(const char* name)
    {
        return !name || !*name || ContainsInsensitive(name, "unknown") ||
            ContainsInsensitive(name, "effect formid");
    }

    const char* AlchemyDisplayName(RE::AlchemyItem* alchemyItem, char* fallback, std::size_t capacity)
    {
        if (!alchemyItem || !fallback || !capacity) return nullptr;
        if (const auto* fullName = alchemyItem->GetFullName(); !IsPlaceholderAlchemyName(fullName)) return fullName;

        const char* effectName = nullptr;
        if (auto* primaryEffect = alchemyItem->GetCostliestEffectItem(); primaryEffect && primaryEffect->baseEffect) {
            auto* primaryBase = primaryEffect->baseEffect;
            const auto* candidate = ReferenceDisplayName(primaryBase);
            if (!IsPlaceholderAlchemyEffectName(candidate)) effectName = candidate;
        }
        if (!effectName) {
            for (auto* effect : alchemyItem->effects) {
                if (!effect || !effect->baseEffect) continue;
                const auto* candidate = ReferenceDisplayName(effect->baseEffect);
                if (!IsPlaceholderAlchemyEffectName(candidate)) {
                    effectName = candidate;
                    break;
                }
            }
        }
        if (effectName) {
            std::snprintf(fallback, capacity, "%s of %s",
                alchemyItem->IsPoison() ? "Poison" : "Potion", effectName);
            return fallback;
        }

        const auto* editorID = alchemyItem->GetFormEditorID();
        if (!IsPlaceholderAlchemyName(editorID)) return editorID;
        std::snprintf(fallback, capacity, "%s (effect name unavailable)",
            alchemyItem->IsPoison() ? "Poison" : "Potion");
        return fallback;
    }

    const char* ItemDisplayName(RE::TESForm* item, char* fallback, std::size_t capacity)
    {
        if (!item) return nullptr;
        if (auto* alchemyItem = item->As<RE::AlchemyItem>()) {
            return AlchemyDisplayName(alchemyItem, fallback, capacity);
        }
        return ReferenceDisplayName(item);
    }

    bool IsGoldForm(const RE::TESForm* form)
    {
        if (!form) return false;
        if (form->IsGold() || form->GetFormID() == 0x0000000F) return true;
        const auto* rawName = form->GetName();
        if (!rawName) return false;
        return ContainsInsensitive(rawName, "gold") || ContainsInsensitive(rawName, "septim");
    }

    bool IsHiddenGoldInventoryEvent(const EventRecord& event)
    {
        if (event.type != 4 || std::string_view(event.identity) != "SkyRewrite/InventoryAction/v1/dropped") return false;
        if (event.formID == 0x0000000F) return true;
        if (auto* form = RE::TESForm::LookupByID(event.formID); form && IsGoldForm(form)) return true;
        std::string name(event.subject);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return name == "gold" || name == "septim" || name == "septims";
    }

    RE::TESQuest* FindRunningNumbersJob()
    {
        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler) return nullptr;
        for (auto* quest : handler->GetFormArray<RE::TESQuest>()) {
            if (!quest || !quest->IsRunning()) continue;
            const auto* name = quest->GetName();
            const auto* editorID = quest->GetFormEditorID();
            if (ContainsInsensitive(name, "The Numbers Job") || ContainsInsensitive(editorID, "TGRFO")) return quest;
        }
        return nullptr;
    }

    bool IsBusinessLedger(RE::TESObjectREFR* target)
    {
        if (!target) return false;
        auto* base = target->GetObjectReference();
        return ContainsInsensitive(target->GetName(), "ledger") ||
            ContainsInsensitive(base ? base->GetName() : nullptr, "ledger") ||
            ContainsInsensitive(base ? base->GetFormEditorID() : nullptr, "ledger");
    }

    RE::Actor* ActiveBarterVendor()
    {
        auto vendorReference = RE::Actor::LookupByHandle(RE::BarterMenu::GetTargetRefHandle());
        auto* vendor = vendorReference.get();
        return vendor && vendor->GetVendorFaction() ? vendor : nullptr;
    }

    void RememberActiveBarterVendor()
    {
        if (auto* vendor = ActiveBarterVendor()) {
            g_recentBarterVendorFormID.store(vendor->GetFormID(), std::memory_order_release);
        }
    }

    RE::Actor* InventoryCounterparty(const RE::TESContainerChangedEvent* event, bool gained)
    {
        if (!event) return nullptr;
        const auto otherContainer = gained ? event->oldContainer : event->newContainer;
        auto* otherForm = otherContainer ? RE::TESForm::LookupByID(otherContainer) : nullptr;
        auto* otherActor = otherForm ? otherForm->As<RE::Actor>() : nullptr;
        auto* ui = RE::UI::GetSingleton();
        const bool barterMenuOpen = ui ? ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)
            : g_barterMenuOpen.load(std::memory_order_acquire);
        const auto closedAt = g_lastBarterMenuCloseMs.load(std::memory_order_acquire);
        const auto now = SteadyClockMilliseconds();
        const bool recentlyClosed = closedAt && now >= closedAt && now - closedAt <= 2000;
        if (!barterMenuOpen && !recentlyClosed) return nullptr;

        if (otherActor && otherActor->GetVendorFaction()) {
            g_recentBarterVendorFormID.store(otherActor->GetFormID(), std::memory_order_release);
            return otherActor;
        }

        // Skyrim can report a merchant's stock chest as the inventory
        // counterparty instead of the vendor actor. Resolve the active or
        // just-closed vendor, then require an exact match with that faction's
        // merchant container before calling this a purchase or sale.
        auto* vendor = barterMenuOpen ? ActiveBarterVendor() : nullptr;
        if (vendor) {
            g_recentBarterVendorFormID.store(vendor->GetFormID(), std::memory_order_release);
        } else if (recentlyClosed) {
            const auto vendorFormID = g_recentBarterVendorFormID.load(std::memory_order_acquire);
            vendor = vendorFormID ? RE::TESForm::LookupByID<RE::Actor>(vendorFormID) : nullptr;
        }
        auto* vendorFaction = vendor ? vendor->GetVendorFaction() : nullptr;
        auto* merchantContainer = vendorFaction ? vendorFaction->vendorData.merchantContainer : nullptr;
        if (merchantContainer && merchantContainer->GetFormID() == otherContainer) return vendor;
        return nullptr;
    }

    RE::TESObjectREFR* InventoryContainerReference(RE::FormID formID)
    {
        auto* form = formID ? RE::TESForm::LookupByID(formID) : nullptr;
        return form ? form->As<RE::TESObjectREFR>() : nullptr;
    }

    void RememberActivatedTheftTarget(const RE::TESActivateEvent* event)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* activator = event ? event->actionRef.get() : nullptr;
        auto* target = event ? event->objectActivated.get() : nullptr;
        if (!player || activator != player || !target) return;

        RecentStealContext context{};
        context.sourceReferenceID = target->GetFormID();
        // Skyrim's activation event is the last point where the target still
        // carries the ownership state that drives the menu's red "Steal" label.
        context.wouldBeStealing = ReferenceWasStolenFrom(player, target);
        context.capturedAt = std::chrono::steady_clock::now();
        std::scoped_lock lock(g_recentStealContextMutex);
        g_recentStealContext = context;
    }

    bool IsLikelyDisplayStorage(const RE::TESObjectREFR* reference)
    {
        if (!reference) return false;
        auto* mutableReference = const_cast<RE::TESObjectREFR*>(reference);
        const auto* name = ReferenceDisplayName(reference);
        const auto* base = mutableReference->GetObjectReference();
        const auto* editorID = base ? base->GetFormEditorID() : nullptr;
        static constexpr std::array<std::string_view, 10> markers{
            "rack", "mannequin", "manikin", "display", "case", "shelf", "armorstand", "armor stand", "weaponrack", "weapon rack"
        };
        return std::any_of(markers.begin(), markers.end(), [&](std::string_view marker) {
            return ContainsInsensitive(name, marker) || ContainsInsensitive(editorID, marker);
        });
    }

    bool IsMannequinActor(const RE::Actor* actor)
    {
        if (!actor) return false;
        const auto* name = ReferenceDisplayName(actor);
        const auto* base = actor->GetActorBase();
        const auto* editorID = base ? base->GetFormEditorID() : nullptr;
        return ContainsInsensitive(name, "mannequin") || ContainsInsensitive(name, "manikin") ||
            ContainsInsensitive(editorID, "mannequin") || ContainsInsensitive(editorID, "manikin");
    }

    bool IsMannequinStorage(const RE::TESObjectREFR* reference)
    {
        if (!reference) return false;
        auto* mutableReference = const_cast<RE::TESObjectREFR*>(reference);
        const auto* name = ReferenceDisplayName(reference);
        const auto* base = mutableReference->GetObjectReference();
        const auto* editorID = base ? base->GetFormEditorID() : nullptr;
        return ContainsInsensitive(name, "mannequin") || ContainsInsensitive(name, "manikin") ||
            ContainsInsensitive(editorID, "mannequin") || ContainsInsensitive(editorID, "manikin");
    }

    const char* StorageFixtureName(const RE::TESObjectREFR* reference,
        char* buffer, std::size_t capacity)
    {
        if (!reference) return "Display";
        auto* mutableReference = const_cast<RE::TESObjectREFR*>(reference);
        const auto* name = ReferenceDisplayName(reference);
        const auto* base = mutableReference->GetObjectReference();
        const auto* editorID = base ? base->GetFormEditorID() : nullptr;
        // Some activator references expose a generic "Weapon Rack" display
        // name even when their owning base form identifies a display case.
        // Prefer the specific fixture kind before falling back to that label.
        if (ContainsInsensitive(name, "display case") || ContainsInsensitive(name, "displaycase") ||
            ContainsInsensitive(editorID, "displaycase") || ContainsInsensitive(editorID, "display_case")) {
            return "Display Case";
        }
        if (ContainsInsensitive(name, "shelf") || ContainsInsensitive(editorID, "shelf")) return "Shelf";
        if (ContainsInsensitive(name, "weapon rack") || ContainsInsensitive(name, "weaponrack") ||
            ContainsInsensitive(editorID, "weaponrack") || ContainsInsensitive(editorID, "weapon_rack")) {
            return "Weapon Rack";
        }
        if (ContainsInsensitive(name, "armor rack") || ContainsInsensitive(name, "armorrack") ||
            ContainsInsensitive(editorID, "armorrack") || ContainsInsensitive(editorID, "armor_rack")) {
            return "Armor Rack";
        }
        return DisplayNameOrFormID(reference, "Display", buffer, capacity);
    }

    void RememberActivatedStorageTarget(const RE::TESActivateEvent* event)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* activator = event ? event->actionRef.get() : nullptr;
        auto* target = event ? event->objectActivated.get() : nullptr;
        if (!player || activator != player || !target ||
            (!target->HasContainer() && !IsLikelyDisplayStorage(target))) return;
        g_recentStorageTargetID.store(target->GetFormID(), std::memory_order_release);
        g_recentStorageTargetAtMs.store(SteadyClockMilliseconds(), std::memory_order_release);
    }

    RE::TESObjectREFR* RecentActivatedStorageTarget()
    {
        const auto targetID = g_recentStorageTargetID.load(std::memory_order_acquire);
        const auto activatedAt = g_recentStorageTargetAtMs.load(std::memory_order_acquire);
        const auto now = SteadyClockMilliseconds();
        if (!targetID || !activatedAt || now < activatedAt) return nullptr;
        if (now - activatedAt > 15000) {
            auto* ui = RE::UI::GetSingleton();
            if (!ui || !ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) return nullptr;
        }
        return InventoryContainerReference(targetID);
    }

    bool MatchesRecentlyActivatedTheftTarget(const RE::TESContainerChangedEvent* event,
        const RE::TESObjectREFR* sourceReference)
    {
        if (!event || !sourceReference) return false;
        std::scoped_lock lock(g_recentStealContextMutex);
        const auto& context = g_recentStealContext;
        if (!context.wouldBeStealing || context.sourceReferenceID == 0) return false;
        const auto age = std::chrono::steady_clock::now() - context.capturedAt;

        // Containers are tied to their exact source reference. Loose objects
        // are tied to their exact world reference and a short pickup window;
        // matching only their shared base form causes false theft on drops.
        if (sourceReference->HasContainer() && age <= std::chrono::minutes(10) &&
            event->oldContainer == context.sourceReferenceID &&
            sourceReference->GetFormID() == context.sourceReferenceID) return true;
        if (!sourceReference->HasContainer() && event->oldContainer == 0 &&
            sourceReference->GetFormID() == context.sourceReferenceID && age <= std::chrono::seconds(5)) return true;
        return false;
    }

    bool IsPickpocketInventorySource(RE::PlayerCharacter* player, RE::FormID sourceFormID)
    {
        if (!player || !sourceFormID || sourceFormID == player->GetFormID()) return false;
        auto* ui = RE::UI::GetSingleton();
        if (!ui || !ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) ||
            RE::ContainerMenu::GetContainerMode() != RE::ContainerMenu::ContainerMode::kPickpocket) return false;
        auto* sourceForm = RE::TESForm::LookupByID(sourceFormID);
        auto* sourceActor = sourceForm && sourceForm->Is(RE::Actor::FORMTYPE) ? static_cast<RE::Actor*>(sourceForm) : nullptr;
        // Actor-origin transfers are theft only when Skyrim says the active
        // ContainerMenu is in pickpocket mode. Barter transfers also originate
        // from living actors, but purchasing from a vendor is not theft.
        return sourceActor && sourceActor != player && !sourceActor->IsDead();
    }

    std::uint32_t GuardCountFromDetail(const char* detail)
    {
        if (!detail) return 0;
        const std::string_view text(detail);
        const auto marker = text.find("guards=");
        if (marker == std::string_view::npos) return text.find("spawned") != std::string_view::npos ? 1U : 0U;
        std::uint32_t spawned = 0;
        std::uint32_t requested = 0;
        return std::sscanf(detail + marker + 7, "%u/%u", &spawned, &requested) == 2 ? std::min(spawned, requested) : 0U;
    }

    enum class BountyApplyResult { applied, appliedWithoutGuard, noCrimeFaction, balanceNotApplied };

    BountyApplyResult ApplyTemporaryHoldBounty(RE::PlayerCharacter* player, RE::BGSLocation* location, float now,
        std::uint32_t amount, float durationDays, std::string_view cause, std::uint32_t pressure = 0)
    {
        if (!player || now < 0.0F) return BountyApplyResult::noCrimeFaction;
        auto* faction = ResolveCurrentHoldCrimeFaction(player, location);
        if (!faction) return BountyApplyResult::noCrimeFaction;
        const auto before = player->GetNonViolentCrimeGoldValue(faction);

        amount = std::clamp(amount, 5U, 100U);
        durationDays = std::clamp(std::isfinite(durationDays) ? durationDays : 1.0F, 0.5F, 7.0F);
        if (before > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) - amount) return BountyApplyResult::balanceNotApplied;
        player->ModCrimeGoldValue(faction, false, static_cast<std::int32_t>(amount));
        if (player->GetNonViolentCrimeGoldValue(faction) != before + amount) return BountyApplyResult::balanceNotApplied;

        const bool dispatchEnabled = g_dispatchGuardOnBounty.load(std::memory_order_relaxed);
        std::uint32_t guardsSpawned = 0;
        constexpr std::uint32_t kGuardsPerCrime = 2;
        const auto guardResult = dispatchEnabled ? SpawnHoldGuardResponse(player, faction, kGuardsPerCrime, &guardsSpawned) : GuardDispatchResult::noMatchingGuard;
        const bool guardDispatched = dispatchEnabled && guardResult == GuardDispatchResult::dispatched;
        EventRecord bounty{};
        bounty.type = 6;
        bounty.gameDay = now;
        bounty.formID = faction->GetFormID();
        bounty.locationFormID = location ? location->GetFormID() : 0;
        bounty.stage = static_cast<std::uint16_t>(amount);
        bounty.baseFormID = before;
        CopyFormPlugin(bounty.formPlugin, sizeof(bounty.formPlugin), faction);
        CopyFormPlugin(bounty.locationPlugin, sizeof(bounty.locationPlugin), location);
        const auto* locationName = location ? location->GetName() : nullptr;
        if (!locationName || !*locationName) locationName = location ? location->GetFormEditorID() : nullptr;
        CopyText(bounty.subject, sizeof(bounty.subject), locationName && *locationName ? locationName : "Unknown hold");
        CopyText(bounty.location, sizeof(bounty.location), locationName && *locationName ? locationName : "Location unavailable");
        const auto identityToken = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
        std::snprintf(bounty.identity, sizeof(bounty.identity), "SkyRewrite/TemporaryBounty/v1/%016llX/%08X/%08X/%.1f",
            identityToken, faction->GetFormID(), before, static_cast<double>(durationDays));
        const char* dispatchStatus = !dispatchEnabled ? "; dispatch disabled" :
            guardResult == GuardDispatchResult::noMatchingGuard ? "; no matching guard" :
            guardResult == GuardDispatchResult::spawnFailed ? "; spawn failed" : "";
        std::snprintf(bounty.detail, sizeof(bounty.detail), "Bounty +%u; %.1f days; cause %.*s; pressure %u; guards=%u/%u%s",
            amount, static_cast<double>(durationDays), static_cast<int>(std::min<std::size_t>(cause.size(), 24)), cause.data(), pressure, guardsSpawned, kGuardsPerCrime, dispatchStatus);
        AddEvent(bounty);
        return guardDispatched ? BountyApplyResult::applied : BountyApplyResult::appliedWithoutGuard;
    }

    void ApplyCrimeBounty(RE::PlayerCharacter* player, const EventRecord& crime, std::string_view crimeKind)
    {
        if (!player) return;
        constexpr std::uint32_t kCrimeBounty = 5;
        auto* location = LookupLocation(crime.locationFormID);
        if (!location) location = player->GetCurrentLocation();
        auto* faction = ResolveCurrentHoldCrimeFaction(player, location);
        const auto before = faction ? player->GetNonViolentCrimeGoldValue(faction) : 0U;
        bool applied = false;
        bool dispatchEnabled = false;
        std::uint32_t guardsSpawned = 0;
        GuardDispatchResult guardResult = GuardDispatchResult::noMatchingGuard;
        if (faction && before <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) - kCrimeBounty) {
            player->ModCrimeGoldValue(faction, false, static_cast<std::int32_t>(kCrimeBounty));
            applied = player->GetNonViolentCrimeGoldValue(faction) == before + kCrimeBounty;
            if (applied) {
                dispatchEnabled = g_dispatchGuardOnBounty.load(std::memory_order_relaxed);
                if (dispatchEnabled) guardResult = SpawnHoldGuardResponse(player, faction, 1, &guardsSpawned);
            }
        }

        EventRecord outcome{};
        outcome.type = 26;
        outcome.gameDay = crime.gameDay;
        outcome.formID = faction ? faction->GetFormID() : crime.formID;
        outcome.locationFormID = crime.locationFormID;
        outcome.baseFormID = before;
        outcome.stage = applied ? static_cast<std::uint16_t>(kCrimeBounty) : 0;
        CopyFormPlugin(outcome.formPlugin, sizeof(outcome.formPlugin), faction ? static_cast<RE::TESForm*>(faction) : RE::TESForm::LookupByID(crime.formID));
        CopyFormPlugin(outcome.locationPlugin, sizeof(outcome.locationPlugin), location);
        CopyText(outcome.subject, sizeof(outcome.subject), crime.subject[0] ? crime.subject : "Recorded crime");
        CopyText(outcome.location, sizeof(outcome.location), crime.location[0] ? crime.location : "Location unavailable");
        const auto token = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
        std::snprintf(outcome.identity, sizeof(outcome.identity), "SkyRewrite/CrimeBounty/v1/%016llX", token);
        if (applied) {
            const auto after = player->GetNonViolentCrimeGoldValue(faction);
            const char* dispatchStatus = !dispatchEnabled ? "; dispatch disabled" :
                guardResult == GuardDispatchResult::noMatchingGuard ? "; no matching guard" :
                guardResult == GuardDispatchResult::spawnFailed ? "; spawn failed" : "";
            std::snprintf(outcome.detail, sizeof(outcome.detail), "Crime bounty +%u for %.*s; %u->%u; guards=%u/1%s",
                kCrimeBounty, static_cast<int>(std::min<std::size_t>(crimeKind.size(), 24)), crimeKind.data(), before, after, guardsSpawned, dispatchStatus);
            SKSE::log::info("SkyRewrite applied {} septim nonviolent crime bounty for {} in location {:08X}; guards spawned/requested {}/{}{}",
                kCrimeBounty, crimeKind, crime.locationFormID, guardsSpawned, dispatchEnabled ? 1U : 0U, dispatchStatus);
        } else {
            CopyText(outcome.detail, sizeof(outcome.detail), faction ? "Crime bounty failed: Skyrim did not change the nonviolent bounty balance" : "Crime bounty failed: no hold crime faction was found for this location");
            SKSE::log::warn("SkyRewrite could not apply crime bounty for {} in location {:08X}: {}",
                crimeKind, crime.locationFormID, faction ? "nonviolent bounty balance did not change" : "hold crime faction unavailable");
        }
        AddEvent(outcome);
    }

    bool BountyVictimModeAllows(std::size_t totalKills, std::size_t hostileKills, std::size_t nonHostileKills)
    {
        switch (g_bountyVictimMode.load(std::memory_order_relaxed)) {
        case 1: return hostileKills > 0;
        // "Both" means either kind of victim, including older journal entries
        // whose disposition predates the saved hostility flags.
        case 2: return totalKills > 0;
        default: return nonHostileKills > 0;
        }
    }

    class RelationshipRankReadCallback final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        RelationshipRankReadCallback(RE::Actor* subject, RE::Actor* player, std::int32_t desiredRank) :
            _subject(subject), _player(player), _desiredRank(std::clamp(desiredRank, 1, 4))
        {}

        void operator()(RE::BSScript::Variable result) override
        {
            if (!result.IsInt() || result.GetSInt() >= _desiredRank) return;
            auto subject = _subject.get();
            auto player = _player.get();
            if (!subject || !player) return;
            auto* vm = RE::SkyrimVM::GetSingleton();
            if (!vm) return;
            auto& runtime = vm->GetVMRuntimeData();
            if (!runtime.impl) return;
            RE::VMTypeID actorType{};
            if (!runtime.impl->GetTypeIDForScriptObject(RE::BSFixedString("Actor"), actorType)) return;
            const auto handle = runtime.handlePolicy.GetHandleForObject(actorType, subject.get());
            if (handle == runtime.handlePolicy.EmptyHandle()) return;
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> ignored;
            auto* args = RE::MakeFunctionArguments(static_cast<RE::Actor*>(player.get()), static_cast<std::int32_t>(_desiredRank));
            runtime.impl->DispatchMethodCall2(handle, RE::BSFixedString("Actor"), RE::BSFixedString("SetRelationshipRank"), args, ignored);
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        RE::ActorHandle _subject;
        RE::ActorHandle _player;
        std::int32_t _desiredRank;
    };

    void ImproveRelationshipRank(RE::Actor* subject, RE::Actor* player, std::int32_t desiredRank)
    {
        if (!subject || !player || subject == player) return;
        auto* vm = RE::SkyrimVM::GetSingleton();
        if (!vm) return;
        auto& runtime = vm->GetVMRuntimeData();
        if (!runtime.impl) return;
        RE::VMTypeID actorType{};
        if (!runtime.impl->GetTypeIDForScriptObject(RE::BSFixedString("Actor"), actorType)) return;
        const auto handle = runtime.handlePolicy.GetHandleForObject(actorType, subject);
        if (handle == runtime.handlePolicy.EmptyHandle()) return;
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result(new RelationshipRankReadCallback(subject, player, desiredRank));
        runtime.impl->DispatchMethodCall2(handle, RE::BSFixedString("Actor"), RE::BSFixedString("GetRelationshipRank"), RE::MakeFunctionArguments(static_cast<RE::Actor*>(player)), result);
    }

    std::uint32_t CountVictimEvents(std::uint32_t flag, std::uint32_t locationFormID)
    {
        std::scoped_lock lock(g_eventsMutex);
        if (flag == kVictimBandit && !locationFormID) {
            const auto state = std::find_if(g_events.begin(), g_events.end(), [](const EventRecord& event) {
                return IsRegionalBanditState(event) && std::string_view(event.identity) == "SkyRewrite/RegionalBanditState/v1/global";
            });
            if (state != g_events.end()) return state->stage;
        }
        return static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" &&
            EventMatchesVictimCategory(event, static_cast<std::uint16_t>(flag)) && (!locationFormID || event.locationFormID == locationFormID);
        }));
    }

    std::uint32_t CountTheftEvents(std::uint32_t locationFormID, float now, float lookbackDays)
    {
        std::scoped_lock lock(g_eventsMutex);
        return static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            return event.type == 10 && event.locationFormID == locationFormID && event.gameDay >= 0.0F &&
                now >= event.gameDay && now - event.gameDay < lookbackDays;
        }));
    }

    bool HasTheftBountyOutcome(std::uint32_t locationFormID, std::uint32_t theftCount)
    {
        const auto pressureToken = "pressure " + std::to_string(theftCount) + ";";
        const auto skippedIdentity = "SkyRewrite/TheftBountySkipped/v1/" + [&] {
            char suffix[32]{};
            std::snprintf(suffix, sizeof(suffix), "%08X/%u", locationFormID, theftCount);
            return std::string(suffix);
        }();
        std::scoped_lock lock(g_eventsMutex);
        return std::any_of(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            if (event.locationFormID != locationFormID) return false;
            if (event.type == 6) {
                return std::string_view(event.detail).find("cause theft threshold") != std::string_view::npos &&
                    std::string_view(event.detail).find(std::string_view(pressureToken)) != std::string_view::npos;
            }
            return event.type == 11 && std::string_view(event.identity) == std::string_view(skippedIdentity);
        });
    }

    void AttemptTheftBountyResponse(RE::PlayerCharacter* player, std::uint32_t locationFormID,
        RE::BGSLocation* location, float now)
    {
        if (!player || !g_theftBountyEnabled.load(std::memory_order_relaxed)) return;
        const auto threshold = g_theftBountyThreshold.load(std::memory_order_relaxed);
        if (!threshold) return;
        const auto theftCount = CountTheftEvents(locationFormID, now, g_localEchoLookbackDays.load(std::memory_order_relaxed));
        if (theftCount < threshold || theftCount % threshold != 0 || HasTheftBountyOutcome(locationFormID, theftCount)) return;

        if (!location) location = LookupLocation(locationFormID);
        if (!location) location = player->GetCurrentLocation();
        const auto amount = g_theftBountyAmount.load(std::memory_order_relaxed);
        SKSE::log::info("SkyRewrite theft bounty threshold reached: location {:08X}, {} thefts, threshold {}, amount {}", locationFormID, theftCount, threshold, amount);
        const auto result = ApplyTemporaryHoldBounty(player, location, now, amount,
            g_temporaryBountyDuration.load(std::memory_order_relaxed), "theft threshold", theftCount);
        if (result == BountyApplyResult::applied || result == BountyApplyResult::appliedWithoutGuard) {
            char message[160]{};
            std::snprintf(message, sizeof(message), "SkyRewrite: Theft response added a %u septim hold bounty after %u thefts.", amount, theftCount);
            RE::SendHUDMessage::ShowHUDMessage(message, nullptr, true);
            return;
        }

        EventRecord skipped{};
        skipped.type = 11;
        skipped.gameDay = now;
        skipped.formID = locationFormID;
        skipped.locationFormID = locationFormID;
        skipped.stage = static_cast<std::uint16_t>(std::min(theftCount, static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max())));
        CopyFormPlugin(skipped.formPlugin, sizeof(skipped.formPlugin), location);
        CopyFormPlugin(skipped.locationPlugin, sizeof(skipped.locationPlugin), location);
        const auto* name = location ? location->GetName() : nullptr;
        if (!name || !*name) name = location ? location->GetFormEditorID() : nullptr;
        CopyText(skipped.subject, sizeof(skipped.subject), name && *name ? name : "Unknown location");
        CopyText(skipped.location, sizeof(skipped.location), skipped.subject);
        std::snprintf(skipped.identity, sizeof(skipped.identity), "SkyRewrite/TheftBountySkipped/v1/%08X/%u", locationFormID, theftCount);
        const char* reason = result == BountyApplyResult::noCrimeFaction ? "no hold crime faction" : "Skyrim did not change the balance";
        std::snprintf(skipped.detail, sizeof(skipped.detail), "Theft bounty threshold reached after %u thefts; skipped: %s", theftCount, reason);
        AddEvent(skipped);
    }

    RE::TESFaction* CrimeFactionForLocation(std::uint32_t locationFormID)
    {
        return ResolveCurrentHoldCrimeFaction(nullptr, LookupLocation(locationFormID));
    }

    RE::TESFaction* CrimeFactionForHoldKey(std::string_view holdKey)
    {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data || holdKey.empty()) return nullptr;

        for (auto* faction : data->GetFormArray<RE::TESFaction>()) {
            if (!faction) continue;
            const auto* editorID = faction->GetFormEditorID();
            if (!editorID || !*editorID) continue;
            const std::string_view editor(editorID);
            constexpr std::string_view crimeFactionPrefix = "CrimeFaction";
            if (editor.size() < crimeFactionPrefix.size() ||
                LowerAscii(editor.substr(0, crimeFactionPrefix.size())) != LowerAscii(crimeFactionPrefix)) continue;
            const auto key = RegionalHoldKey(editor.substr(crimeFactionPrefix.size()));
            if (key == holdKey) return faction;
        }

        // Some plugins use nonstandard crime-faction editor IDs. Resolve the
        // jurisdiction through loaded locations as a fallback.
        for (auto* candidate : data->GetFormArray<RE::BGSLocation>()) {
            if (RegionalHoldKeyForLocation(candidate) != holdKey) continue;
            for (auto* current = candidate; current; current = current->parentLoc) {
                if (auto* crimeFaction = current->unreportedCrimeFaction) return crimeFaction;
            }
        }
        return nullptr;
    }

    std::string RegionalHoldKeyForPlayer(RE::PlayerCharacter* player, RE::BGSLocation* location)
    {
        if (auto key = RegionalHoldKeyForLocation(location); !key.empty()) return key;
        if (player) {
            if (auto* worldspace = player->GetWorldspace()) {
                if (const auto* name = worldspace->GetName(); name && *name) {
                    if (auto key = RegionalHoldKey(name); !key.empty()) return key;
                }
                if (const auto* editorID = worldspace->GetFormEditorID(); editorID && *editorID) {
                    if (auto key = RegionalHoldKey(editorID); !key.empty()) return key;
                }
            }
        }
        auto* faction = ResolveCurrentHoldCrimeFaction(player, location);
        const auto* name = faction ? faction->GetName() : nullptr;
        const auto* editorID = faction ? faction->GetFormEditorID() : nullptr;
        return RegionalHoldKey(editorID && *editorID ? editorID : (name ? name : ""));
    }

    std::uint32_t CountVictimEventsInFaction(std::uint32_t flag, RE::TESFaction* faction)
    {
        if (!faction) return 0;
        std::scoped_lock lock(g_eventsMutex);
        if (flag == kVictimBandit) {
            const auto state = std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return IsRegionalBanditState(event) && event.formID == faction->GetFormID();
            });
            if (state != g_events.end()) return state->stage;
        }
        return static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            if (event.type != 1 || std::string_view(event.detail) != "Player killed this actor" || !EventMatchesVictimCategory(event, static_cast<std::uint16_t>(flag))) return false;
            auto* locationFaction = CrimeFactionForLocation(event.locationFormID);
            return locationFaction == faction;
        }));
    }

    std::string RegionalBanditStateIdentity(std::uint32_t holdFormID)
    {
        char identity[80]{};
        if (holdFormID) std::snprintf(identity, sizeof(identity), "SkyRewrite/RegionalBanditState/v1/hold/%08X", holdFormID);
        else CopyText(identity, sizeof(identity), "SkyRewrite/RegionalBanditState/v1/global");
        return identity;
    }

    void UpdateRegionalBanditState(const EventRecord& death, RE::PlayerCharacter* player)
    {
        auto* holdFaction = ResolveCurrentHoldCrimeFaction(player, player ? player->GetCurrentLocation() : nullptr);
        if (!holdFaction) holdFaction = CrimeFactionForLocation(death.locationFormID);
        const auto holdFormID = holdFaction ? holdFaction->GetFormID() : 0U;
        const auto holdIdentity = RegionalBanditStateIdentity(holdFormID);
        const auto globalIdentity = RegionalBanditStateIdentity(0);
        std::scoped_lock lock(g_eventsMutex);

        const auto findState = [&](const std::string& identity) {
            return std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return IsRegionalBanditState(event) && std::string_view(event.identity) == identity;
            });
        };
        const auto isBanditDeath = [](const EventRecord& event) {
            return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" && EventMatchesVictimCategory(event, kVictimBandit);
        };
        const auto maximum = static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max());
        const auto globalState = findState(globalIdentity);
        const auto globalPriorDeaths = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), isBanditDeath));
        const auto globalTotal = globalState != g_events.end()
            ? std::min(static_cast<std::uint32_t>(globalState->stage) + 1U, maximum)
            : std::min(globalPriorDeaths + 1U, maximum);

        std::uint32_t holdTotal = 0;
        if (holdFormID) {
            const auto holdState = findState(holdIdentity);
            const auto holdPriorDeaths = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return isBanditDeath(event) && CrimeFactionForLocation(event.locationFormID) == holdFaction;
            }));
            holdTotal = holdState != g_events.end()
                ? std::min(static_cast<std::uint32_t>(holdState->stage) + 1U, maximum)
                : std::min(holdPriorDeaths + 1U, maximum);
        }

        const auto writeState = [&](const std::string& identity, std::uint32_t stateFormID, std::uint32_t stateLocationID,
                                    const RE::TESForm* ownerForm, const char* label, std::uint32_t total) {
            auto state = findState(identity);
            if (state != g_events.end()) {
                state->stage = static_cast<std::uint16_t>(total);
                state->gameDay = death.gameDay;
                if (label && *label) {
                    CopyText(state->subject, sizeof(state->subject), label);
                    CopyText(state->location, sizeof(state->location), label);
                }
                return;
            }

            const auto retention = g_journalRetention.load(std::memory_order_relaxed);
            while (g_events.size() >= retention) {
                auto removable = g_events.end();
                for (auto it = g_events.begin(); it != g_events.end(); ++it) {
                    const auto index = static_cast<std::size_t>(it - g_events.begin());
                    if (!IsProtectedWorldResponseEvent(index)) { removable = it; break; }
                }
                if (removable == g_events.end()) break;
                g_events.erase(removable);
            }

            EventRecord record{};
            record.type = 14;
            record.gameDay = death.gameDay;
            record.formID = stateFormID;
            record.locationFormID = stateLocationID;
            record.stage = static_cast<std::uint16_t>(total);
            CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), ownerForm);
            CopyFormPlugin(record.locationPlugin, sizeof(record.locationPlugin), ownerForm);
            CopyText(record.subject, sizeof(record.subject), label);
            CopyText(record.location, sizeof(record.location), label);
            CopyText(record.identity, sizeof(record.identity), identity.c_str());
            CopyText(record.detail, sizeof(record.detail), stateFormID ? "Cumulative bandit defeats in this hold" : "Cumulative bandit defeats across Skyrim");
            g_events.push_back(record);
        };

        const auto* holdName = holdFaction ? holdFaction->GetName() : nullptr;
        if (holdFaction && (!holdName || !*holdName)) holdName = holdFaction->GetFormEditorID();
        writeState(globalIdentity, 0, 0, nullptr, "All holds", globalTotal);
        if (holdFormID) {
            writeState(holdIdentity, holdFormID, holdFormID, holdFaction, holdName && *holdName ? holdName : "Unknown hold", holdTotal);
        }
    }

    void UpdateRegionalBanditHoldKeyState(const EventRecord& death, RE::PlayerCharacter* player)
    {
        auto* location = player ? player->GetCurrentLocation() : nullptr;
        if (!location) location = LookupLocation(death.locationFormID);
        const auto holdKey = RegionalHoldKeyForPlayer(player, location);
        if (holdKey.empty()) return;

        const auto identity = "SkyRewrite/RegionalBanditHoldKey/v1/" + holdKey;
        const auto maximum = static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max());
        std::scoped_lock lock(g_eventsMutex);
        const auto saved = std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            return IsRegionalBanditHoldKeyState(event) && std::string_view(event.identity) == identity;
        });
        const auto priorKills = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" &&
                EventMatchesVictimCategory(event, kVictimBandit) && RegionalHoldKeyForEvent(event) == holdKey;
        }));
        const auto total = saved != g_events.end()
            ? std::min(static_cast<std::uint32_t>(saved->stage) + 1U, maximum)
            : std::min(priorKills + 1U, maximum);
        if (saved != g_events.end()) {
            saved->stage = static_cast<std::uint16_t>(total);
            saved->gameDay = death.gameDay;
            CopyText(saved->subject, sizeof(saved->subject), RegionalHoldLabel(holdKey));
            CopyText(saved->location, sizeof(saved->location), holdKey.c_str());
            return;
        }
        const auto retention = g_journalRetention.load(std::memory_order_relaxed);
        while (g_events.size() >= retention) {
            auto removable = std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                const auto index = static_cast<std::size_t>(&event - g_events.data());
                return !IsProtectedWorldResponseEvent(index);
            });
            if (removable == g_events.end()) break;
            g_events.erase(removable);
        }
        EventRecord state{};
        state.type = 14;
        state.gameDay = death.gameDay;
        state.stage = static_cast<std::uint16_t>(total);
        CopyText(state.subject, sizeof(state.subject), RegionalHoldLabel(holdKey));
        CopyText(state.location, sizeof(state.location), holdKey.c_str());
        CopyText(state.identity, sizeof(state.identity), identity.c_str());
        CopyText(state.detail, sizeof(state.detail), "Cumulative bandit defeats in this region");
        g_events.push_back(state);
    }

    std::string RegionalCreatureStateIdentity(std::string_view holdKey, std::string_view category)
    {
        if (holdKey.empty()) return "SkyRewrite/RegionalCreatureState/v1/global/" + std::string(category);
        return "SkyRewrite/RegionalCreatureState/v1/hold/" + std::string(holdKey) + "/" + std::string(category);
    }

    void UpdateRegionalCreatureStates(const EventRecord& death, RE::PlayerCharacter* player)
    {
        struct Category { std::uint16_t flag; const char* key; const char* label; };
        static constexpr std::array<Category, 12> categories{{
            {kVictimBandit, "bandit", "bandit defeats"},
            {kVictimDragon, "dragon", "dragon defeats"},
            {kVictimForsworn, "forsworn", "Forsworn defeats"},
            {kVictimVampire, "vampire", "vampire defeats"},
            {kVictimWolf, "wolf", "wolf defeats"},
            {kVictimBear, "bear", "bear defeats"},
            {kVictimSpider, "spider", "spider defeats"},
            {kVictimMonster, "monster", "hostile monster defeats"},
            {kVictimAllHostile, "allhostile", "hostile defeats"},
            {kVictimOtherHostile, "otherhostile", "other hostile defeats"},
            {kVictimCivilian, "civilians", "non-hostile residents killed"},
            {kVictimCivilian, "murders", "murders committed"}
        }};
        auto* currentLocation = player ? player->GetCurrentLocation() : nullptr;
        auto* holdFaction = ResolveCurrentHoldCrimeFaction(player, currentLocation);
        if (!holdFaction) holdFaction = CrimeFactionForLocation(death.locationFormID);
        const auto* factionEditorID = holdFaction ? holdFaction->GetFormEditorID() : nullptr;
        const auto* factionName = holdFaction ? holdFaction->GetName() : nullptr;
        auto holdKey = RegionalHoldKeyForPlayer(player, currentLocation);
        if (holdKey.empty()) holdKey = RegionalHoldKeyForLocation(LookupLocation(death.locationFormID));
        if (holdKey.empty()) holdKey = RegionalHoldKey(factionEditorID && *factionEditorID ? factionEditorID : (factionName ? factionName : ""));
        const auto holdFormID = holdFaction && !holdKey.empty() ? holdFaction->GetFormID() : 0U;
        const auto* holdLabel = RegionalHoldLabel(holdKey);
        const auto maximum = static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max());

        std::scoped_lock lock(g_eventsMutex);
        const auto findState = [&](const std::string& identity) {
            return std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return IsRegionalCreatureState(event) && std::string_view(event.identity) == identity;
            });
        };
        const auto writeState = [&](const std::string& identity, std::uint32_t factionID, const char* key,
                                    const char* label, const char* detail, std::uint32_t total) {
            auto state = findState(identity);
            if (state != g_events.end()) {
                state->stage = static_cast<std::uint16_t>(total);
                state->gameDay = death.gameDay;
                CopyText(state->subject, sizeof(state->subject), label);
                CopyText(state->location, sizeof(state->location), key);
                return;
            }
            const auto retention = g_journalRetention.load(std::memory_order_relaxed);
            while (g_events.size() >= retention) {
                auto removable = g_events.end();
                for (auto it = g_events.begin(); it != g_events.end(); ++it) {
                    const auto index = static_cast<std::size_t>(it - g_events.begin());
                    if (!IsProtectedWorldResponseEvent(index)) { removable = it; break; }
                }
                if (removable == g_events.end()) break;
                g_events.erase(removable);
            }
            EventRecord saved{};
            saved.type = 14;
            saved.gameDay = death.gameDay;
            saved.formID = factionID;
            saved.locationFormID = factionID;
            saved.stage = static_cast<std::uint16_t>(total);
            CopyFormPlugin(saved.formPlugin, sizeof(saved.formPlugin), holdFaction);
            CopyFormPlugin(saved.locationPlugin, sizeof(saved.locationPlugin), holdFaction);
            CopyText(saved.subject, sizeof(saved.subject), label);
            CopyText(saved.location, sizeof(saved.location), key);
            CopyText(saved.identity, sizeof(saved.identity), identity.c_str());
            CopyText(saved.detail, sizeof(saved.detail), detail);
            g_events.push_back(saved);
        };

        for (const auto& category : categories) {
            if (!(death.stage & category.flag)) continue;
            const auto globalIdentity = RegionalCreatureStateIdentity({}, category.key);
            const auto globalState = findState(globalIdentity);
            const auto priorGlobal = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" && EventMatchesVictimCategory(event, category.flag);
            }));
            const auto globalTotal = globalState != g_events.end()
                ? std::min(static_cast<std::uint32_t>(globalState->stage) + 1U, maximum)
                : std::min(priorGlobal + 1U, maximum);
            writeState(globalIdentity, 0, "all", "All holds", category.label, globalTotal);

            if (!holdKey.empty()) {
                const auto holdIdentity = RegionalCreatureStateIdentity(holdKey, category.key);
                const auto holdState = findState(holdIdentity);
                const auto priorHold = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                    return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" &&
                        EventMatchesVictimCategory(event, category.flag) && RegionalHoldKeyForEvent(event) == holdKey;
                }));
                const auto holdTotal = holdState != g_events.end()
                    ? std::min(static_cast<std::uint32_t>(holdState->stage) + 1U, maximum)
                    : std::min(priorHold + 1U, maximum);
                writeState(holdIdentity, holdFormID, holdKey.c_str(), holdLabel, category.label, holdTotal);
            }
        }

        if ((death.stage & kDispositionKnown) && !(death.stage & kVictimHostile) && !(death.stage & kVictimTeammate)) {
            constexpr std::string_view category = "nonhostile";
            const auto matches = [](const EventRecord& event) {
                return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" &&
                    (event.stage & kDispositionKnown) && !(event.stage & kVictimHostile) && !(event.stage & kVictimTeammate);
            };
            const auto globalIdentity = RegionalCreatureStateIdentity({}, category);
            const auto globalState = findState(globalIdentity);
            const auto priorGlobal = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), matches));
            const auto globalTotal = globalState != g_events.end()
                ? std::min(static_cast<std::uint32_t>(globalState->stage) + 1U, maximum)
                : std::min(priorGlobal + 1U, maximum);
            writeState(globalIdentity, 0, "all", "All holds", "non-hostile kills", globalTotal);
            if (!holdKey.empty()) {
                const auto holdIdentity = RegionalCreatureStateIdentity(holdKey, category);
                const auto holdState = findState(holdIdentity);
                const auto priorHold = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                    return matches(event) && RegionalHoldKeyForEvent(event) == holdKey;
                }));
                const auto holdTotal = holdState != g_events.end()
                    ? std::min(static_cast<std::uint32_t>(holdState->stage) + 1U, maximum)
                    : std::min(priorHold + 1U, maximum);
                writeState(holdIdentity, holdFormID, holdKey.c_str(), holdLabel, "non-hostile kills", holdTotal);
            }
        }

        const auto specificCategory = SpecificVictimCategory(death.subject, death.identity);
        if (!specificCategory.empty()) {
            const auto globalIdentity = RegionalCreatureStateIdentity({}, specificCategory);
            const auto globalState = findState(globalIdentity);
            const auto matches = [&](const EventRecord& event) {
                return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" &&
                    SpecificVictimCategory(event.subject, event.identity) == specificCategory;
            };
            const auto priorGlobal = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), matches));
            const auto globalTotal = globalState != g_events.end()
                ? std::min(static_cast<std::uint32_t>(globalState->stage) + 1U, maximum)
                : std::min(priorGlobal + 1U, maximum);
            writeState(globalIdentity, 0, "all", "All holds", specificCategory.c_str(), globalTotal);
            if (!holdKey.empty()) {
                const auto holdIdentity = RegionalCreatureStateIdentity(holdKey, specificCategory);
                const auto holdState = findState(holdIdentity);
                const auto priorHold = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                    return matches(event) && RegionalHoldKeyForEvent(event) == holdKey;
                }));
                const auto holdTotal = holdState != g_events.end()
                    ? std::min(static_cast<std::uint32_t>(holdState->stage) + 1U, maximum)
                    : std::min(priorHold + 1U, maximum);
                writeState(holdIdentity, holdFormID, holdKey.c_str(), holdLabel, specificCategory.c_str(), holdTotal);
            }
        }

        if (!specificCategory.empty() && (death.stage & kVictimHostile)) {
            const auto hostileCategory = "hostile_" + specificCategory;
            const auto globalIdentity = RegionalCreatureStateIdentity({}, hostileCategory);
            const auto hostileMatch = [&](const EventRecord& event) {
                return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" &&
                    (event.stage & kVictimHostile) && SpecificVictimCategory(event.subject, event.identity) == specificCategory;
            };
            const auto globalState = findState(globalIdentity);
            const auto priorGlobal = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), hostileMatch));
            const auto globalTotal = globalState != g_events.end()
                ? std::min(static_cast<std::uint32_t>(globalState->stage) + 1U, maximum)
                : std::min(priorGlobal + 1U, maximum);
            writeState(globalIdentity, 0, "all", hostileCategory.c_str(), hostileCategory.c_str(), globalTotal);
            if (!holdKey.empty()) {
                const auto holdIdentity = RegionalCreatureStateIdentity(holdKey, hostileCategory);
                const auto holdState = findState(holdIdentity);
                const auto priorHold = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                    return hostileMatch(event) && RegionalHoldKeyForEvent(event) == holdKey;
                }));
                const auto holdTotal = holdState != g_events.end()
                    ? std::min(static_cast<std::uint32_t>(holdState->stage) + 1U, maximum)
                    : std::min(priorHold + 1U, maximum);
                writeState(holdIdentity, holdFormID, holdKey.c_str(), holdLabel, hostileCategory.c_str(), holdTotal);
            }
        }
    }

    void UpdateRegionalTheftStates(const EventRecord& theft, RE::PlayerCharacter* player)
    {
        auto* location = player ? player->GetCurrentLocation() : nullptr;
        if (!location) location = LookupLocation(theft.locationFormID);
        auto holdKey = RegionalHoldKeyForPlayer(player, location);
        if (holdKey.empty()) holdKey = RegionalHoldKeyForLocation(location);

        struct Counter { const char* key; const char* label; std::uint32_t increment; };
        const auto quantity = std::max<std::uint32_t>(1, theft.stage);
        const std::array<Counter, 3> counters{{
            {"thefts", "recorded thefts", 1},
            {"itemsstolen", "items stolen", quantity},
            {"goldstolen", "septims stolen", theft.formID == 0x0000000FU ? quantity : 0}
        }};
        const auto maximum = static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max());
        std::scoped_lock lock(g_eventsMutex);
        const auto findState = [&](const std::string& identity) {
            return std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return IsRegionalCreatureState(event) && std::string_view(event.identity) == identity;
            });
        };
        const auto incrementState = [&](std::string_view key, const char* label, std::uint32_t increment, std::string_view targetHold) {
            if (!increment) return;
            const auto identity = RegionalCreatureStateIdentity(targetHold, key);
            auto saved = findState(identity);
            std::uint32_t prior{};
            if (saved != g_events.end()) {
                prior = saved->stage;
            } else {
                prior = static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                    if (event.type != 10) return false;
                    if (!targetHold.empty() && RegionalHoldKeyForEvent(event) != targetHold) return false;
                    if (key == "thefts") return true;
                    if (key == "itemsstolen") return true;
                    return event.formID == 0x0000000FU;
                }));
                if (key != "thefts") {
                    prior = 0;
                    for (const auto& event : g_events) {
                        if (event.type != 10 || (!targetHold.empty() && RegionalHoldKeyForEvent(event) != targetHold)) continue;
                        if (key == "goldstolen" && event.formID != 0x0000000FU) continue;
                        prior = std::min(prior + static_cast<std::uint32_t>(std::max<std::uint16_t>(1, event.stage)), maximum);
                    }
                }
            }
            const auto total = std::min(prior + increment, maximum);
            if (saved != g_events.end()) {
                saved->stage = static_cast<std::uint16_t>(total);
                saved->gameDay = theft.gameDay;
                return;
            }
            const auto retention = g_journalRetention.load(std::memory_order_relaxed);
            while (g_events.size() >= retention) {
                auto removable = g_events.end();
                for (auto it = g_events.begin(); it != g_events.end(); ++it) {
                    const auto index = static_cast<std::size_t>(it - g_events.begin());
                    if (!IsProtectedWorldResponseEvent(index)) { removable = it; break; }
                }
                if (removable == g_events.end()) break;
                g_events.erase(removable);
            }
            EventRecord state{};
            state.type = 14;
            state.gameDay = theft.gameDay;
            state.stage = static_cast<std::uint16_t>(total);
            CopyText(state.subject, sizeof(state.subject), RegionalHoldLabel(targetHold.empty() ? "" : targetHold));
            CopyText(state.location, sizeof(state.location), targetHold.empty() ? "all" : std::string(targetHold).c_str());
            CopyText(state.identity, sizeof(state.identity), identity.c_str());
            CopyText(state.detail, sizeof(state.detail), label);
            g_events.push_back(state);
        };
        for (const auto& counter : counters) {
            incrementState(counter.key, counter.label, counter.increment, {});
            if (!holdKey.empty()) incrementState(counter.key, counter.label, counter.increment, holdKey);
        }
    }

    std::int32_t ReputationScore();
    std::int32_t RegionalHoldReputationScore(std::string_view holdKey);
    std::uint32_t RegionalHoldCreatureCount(std::string_view holdKey, std::string_view category);

    void ApplyBanditTradeResponse()
    {
        auto* collection = RE::GameSettingCollection::GetSingleton();
        auto* buyBase = collection ? collection->GetSetting("fBarterBuyBase") : nullptr;
        if (!buyBase || buyBase->GetType() != RE::Setting::Type::kFloat) return;
        static float originalBuyBase = 0.0F;
        static float lastAppliedBuyBase = 0.0F;
        if (originalBuyBase <= 0.0F) originalBuyBase = buyBase->GetFloat();
        const auto current = buyBase->GetFloat();
        if (lastAppliedBuyBase > 0.0F && std::abs(current - lastAppliedBuyBase) > 0.001F) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto holdKey = player ? RegionalHoldKeyForPlayer(player, player->GetCurrentLocation()) : std::string{};
        const auto score = holdKey.empty() ? ReputationScore() : RegionalHoldReputationScore(holdKey);
        const auto priceFactor = score < 0
            ? 1.0F + static_cast<float>(std::min<std::int64_t>(500, -static_cast<std::int64_t>(score))) * 0.01F
            : 1.0F - static_cast<float>(std::min<std::int64_t>(99, score)) * 0.01F;
        lastAppliedBuyBase = originalBuyBase * priceFactor;
        buyBase->SetFloat(lastAppliedBuyBase);
    }

    void ImproveLocalMerchantRelationships(std::uint32_t locationID)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* processes = RE::ProcessLists::GetSingleton();
        if (!player || !processes || !locationID) return;
        auto* holdFaction = CrimeFactionForLocation(locationID);
        auto* location = LookupLocation(locationID);
        const auto holdKey = RegionalHoldKeyForLocation(location);
        const auto localBanditKills = holdKey.empty()
            ? CountVictimEventsInFaction(kVictimBandit, holdFaction)
            : RegionalHoldCreatureCount(holdKey, "bandit");
        if (!holdFaction || !localBanditKills) return;
        const auto targetRank = static_cast<std::int32_t>(std::clamp(1U + (localBanditKills - 1U) / 5U, 1U, 4U));
        processes->ForAllActors([&](RE::Actor* actor) {
            if (!actor || actor->IsDead() || actor->IsPlayerTeammate() || actor->IsHostileToActor(player) || !actor->GetVendorFaction()) return RE::BSContainer::ForEachResult::kContinue;
            const auto* base = actor->GetActorBase();
            const bool assignedToHold = actor->GetCrimeFaction() == holdFaction || (base && base->crimeFaction == holdFaction);
            if (!assignedToHold) return RE::BSContainer::ForEachResult::kContinue;
            ImproveRelationshipRank(actor, player, targetRank);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }

    void ImproveLocalHoldRelationships(RE::PlayerCharacter* player, RE::BGSLocation* location)
    {
        if (!player || !location) return;
        auto* processes = RE::ProcessLists::GetSingleton();
        auto* crimeFaction = ResolveCurrentHoldCrimeFaction(player, location);
        if (!processes || !crimeFaction) return;
        const auto holdKey = RegionalHoldKeyForLocation(location);
        const auto dragonKills = holdKey.empty()
            ? CountVictimEventsInFaction(kVictimDragon, crimeFaction)
            : RegionalHoldCreatureCount(holdKey, "dragon");
        if (!dragonKills) return;
        const auto targetRank = static_cast<std::int32_t>(std::clamp(dragonKills, 1U, 4U));
        processes->ForAllActors([&](RE::Actor* actor) {
            if (!actor || actor->IsDead() || actor->IsPlayerTeammate() || actor->IsHostileToActor(player)) return RE::BSContainer::ForEachResult::kContinue;
            const auto* base = actor->GetActorBase();
            const auto* editorID = base ? base->GetFormEditorID() : nullptr;
            std::string baseName = editorID ? editorID : "";
            std::transform(baseName.begin(), baseName.end(), baseName.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            const bool assignedToHold = actor->GetCrimeFaction() == crimeFaction || (base && base->crimeFaction == crimeFaction);
            const bool localGuard = actor->IsGuard() && assignedToHold;
            const bool localJarl = baseName.find("jarl") != std::string::npos && assignedToHold;
            if (!localGuard && !localJarl) return RE::BSContainer::ForEachResult::kContinue;
            ImproveRelationshipRank(actor, player, targetRank);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }

    std::uint32_t RegionalCreatureCount(std::string_view category)
    {
        const auto identity = RegionalCreatureStateIdentity({}, category);
        std::scoped_lock lock(g_eventsMutex);
        const auto saved = std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            return IsRegionalCreatureState(event) && std::string_view(event.identity) == identity;
        });
        if (saved != g_events.end()) return saved->stage;

        if (category == "forgeries") {
            return static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [](const EventRecord& event) {
                return event.type == 24;
            }));
        }
        if (category == "thefts" || category == "itemsstolen" || category == "goldstolen") {
            std::uint32_t total{};
            for (const auto& event : g_events) {
                if (event.type != 10 || (category == "goldstolen" && event.formID != 0x0000000FU)) continue;
                total += category == "thefts" ? 1U : std::max<std::uint16_t>(1, event.stage);
            }
            return total;
        }
        std::uint16_t flag{};
        if (category == "bandit") flag = kVictimBandit;
        else if (category == "dragon") flag = kVictimDragon;
        else if (category == "forsworn") flag = kVictimForsworn;
        else if (category == "vampire") flag = kVictimVampire;
        else if (category == "wolf") flag = kVictimWolf;
        else if (category == "bear") flag = kVictimBear;
        else if (category == "spider") flag = kVictimSpider;
        else if (category == "monster") flag = kVictimMonster;
        else if (category == "allhostile") flag = kVictimAllHostile;
        else if (category == "otherhostile") flag = kVictimOtherHostile;
        else if (category == "civilians" || category == "murders") flag = kVictimCivilian;
        else {
            return static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" &&
                    SpecificVictimCategory(event.subject, event.identity) == category;
            }));
        }
        return static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" && EventMatchesVictimCategory(event, flag);
        }));
    }

    std::int32_t ReputationScore();

    std::uint32_t RegionalHoldCreatureCount(std::string_view holdKey, std::string_view category)
    {
        if (holdKey.empty()) return RegionalCreatureCount(category);
        const auto identity = RegionalCreatureStateIdentity(holdKey, category);
        std::scoped_lock lock(g_eventsMutex);
        const auto saved = std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            return IsRegionalCreatureState(event) && std::string_view(event.identity) == identity;
        });
        if (saved != g_events.end()) return saved->stage;
        if (category == "bandit") {
            const auto legacyIdentity = "SkyRewrite/RegionalBanditHoldKey/v1/" + std::string(holdKey);
            const auto legacy = std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return IsRegionalBanditHoldKeyState(event) && std::string_view(event.identity) == legacyIdentity;
            });
            if (legacy != g_events.end()) return legacy->stage;
        }
        std::uint16_t flag{};
        if (category == "bandit") flag = kVictimBandit;
        else if (category == "dragon") flag = kVictimDragon;
        else if (category == "forsworn") flag = kVictimForsworn;
        else if (category == "vampire") flag = kVictimVampire;
        else if (category == "wolf") flag = kVictimWolf;
        else if (category == "bear") flag = kVictimBear;
        else if (category == "spider") flag = kVictimSpider;
        else if (category == "monster") flag = kVictimMonster;
        else if (category == "allhostile") flag = kVictimAllHostile;
        else if (category == "otherhostile") flag = kVictimOtherHostile;
        else if (category == "civilians" || category == "murders") flag = kVictimCivilian;
        std::uint32_t total{};
        for (const auto& event : g_events) {
            if (category == "thefts" || category == "itemsstolen" || category == "goldstolen") {
                if (event.type != 10 || RegionalHoldKeyForEvent(event) != holdKey ||
                    (category == "goldstolen" && event.formID != 0x0000000FU)) continue;
                total += category == "thefts" ? 1U : std::max<std::uint16_t>(1, event.stage);
                continue;
            }
            if (event.type != 1 || std::string_view(event.detail) != "Player killed this actor" || RegionalHoldKeyForEvent(event) != holdKey) continue;
            if (flag ? EventMatchesVictimCategory(event, flag) : SpecificVictimCategory(event.subject, event.identity) == category) ++total;
        }
        return total;
    }

    std::int32_t RegionalHoldReputationScore(std::string_view holdKey)
    {
        if (holdKey.empty()) return ReputationScore();
        std::int64_t positive = RegionalHoldCreatureCount(holdKey, "bandit") + RegionalHoldCreatureCount(holdKey, "forsworn") +
            RegionalHoldCreatureCount(holdKey, "vampire") + RegionalHoldCreatureCount(holdKey, "monster");
        {
            std::scoped_lock lock(g_eventsMutex);
            positive += static_cast<std::int64_t>(std::count_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return IsReputationState(event) && std::string_view(event.identity).find("SkyRewrite/ReputationFavor/v1/") == 0 &&
                    RegionalHoldKeyForEvent(event) == holdKey;
            }));
        }
        const auto negative = static_cast<std::int64_t>(RegionalHoldCreatureCount(holdKey, "thefts")) +
            static_cast<std::int64_t>(RegionalHoldCreatureCount(holdKey, "murders")) * 5;
        return static_cast<std::int32_t>(std::clamp<std::int64_t>(positive - negative,
            std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
    }

    void UpdateRegionalShopRobberyState(std::string_view holdKey, float gameDay, bool robberyOccurred)
    {
        if (holdKey.empty()) return;
        const auto identity = "SkyRewrite/RegionalShopRobbery/v1/" + std::string(holdKey);
        std::scoped_lock lock(g_eventsMutex);
        auto saved = std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            return IsRegionalShopRobberyState(event) && std::string_view(event.identity) == identity;
        });
        if (saved != g_events.end()) {
            if (robberyOccurred) saved->stage = static_cast<std::uint16_t>(std::min<std::uint32_t>(saved->stage + 1U, std::numeric_limits<std::uint16_t>::max()));
            saved->gameDay = gameDay;
            return;
        }

        const auto retention = g_journalRetention.load(std::memory_order_relaxed);
        while (g_events.size() >= retention) {
            auto removable = g_events.end();
            for (auto it = g_events.begin(); it != g_events.end(); ++it) {
                const auto index = static_cast<std::size_t>(it - g_events.begin());
                if (!IsProtectedWorldResponseEvent(index)) { removable = it; break; }
            }
            if (removable == g_events.end()) break;
            g_events.erase(removable);
        }

        EventRecord state{};
        state.type = 14;
        state.gameDay = gameDay;
        state.stage = robberyOccurred ? 1 : 0;
        CopyText(state.subject, sizeof(state.subject), RegionalHoldLabel(holdKey));
        CopyText(state.location, sizeof(state.location), std::string(holdKey).c_str());
        CopyText(state.identity, sizeof(state.identity), identity.c_str());
        CopyText(state.detail, sizeof(state.detail), "Last local shop robbery check and cumulative robbery count");
        g_events.push_back(state);
    }

    void AttemptRegionalShopRobbery(RE::PlayerCharacter* player, RE::BGSLocation* location)
    {
        if (!player || !location) return;
        auto* processes = RE::ProcessLists::GetSingleton();
        if (!processes) return;
        const auto holdKey = RegionalHoldKeyForLocation(location);
        if (holdKey.empty()) return;

        const auto localReputation = RegionalHoldReputationScore(holdKey);
        if (localReputation >= 0) return;
        const auto now = CurrentGameDay();
        if (!std::isfinite(now) || now < 0.0F) return;
        const auto stateIdentity = "SkyRewrite/RegionalShopRobbery/v1/" + holdKey;
        {
            std::scoped_lock lock(g_eventsMutex);
            const auto lastCheck = std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return IsRegionalShopRobberyState(event) && std::string_view(event.identity) == stateIdentity;
            });
            if (lastCheck != g_events.end() && now >= lastCheck->gameDay && now - lastCheck->gameDay < 1.0F) return;
        }

        std::vector<RE::Actor*> shopkeepers;
        auto* playerCell = player->GetParentCell();
        processes->ForAllActors([&](RE::Actor* actor) {
            if (!actor || actor == player || actor->IsDead() || actor->IsDisabled() || !actor->GetVendorFaction() ||
                actor->IsHostileToActor(player) || actor->GetParentCell() != playerCell) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            const auto actorHold = RegionalHoldKeyForLocation(actor->GetCurrentLocation());
            if (actorHold == holdKey) shopkeepers.push_back(actor);
            return RE::BSContainer::ForEachResult::kContinue;
        });
        if (shopkeepers.empty()) return;

        std::vector<RE::Actor*> stockedShopkeepers;
        for (auto* shopkeeper : shopkeepers) {
            if (!shopkeeper) continue;
            auto inventory = shopkeeper->GetInventory();
            bool hasNonQuestStock = false;
            for (const auto& [object, entry] : inventory) {
                if (!object || entry.first <= 0 || !entry.second || entry.second->IsQuestObject()) continue;
                hasNonQuestStock = true;
                break;
            }
            if (hasNonQuestStock) stockedShopkeepers.push_back(shopkeeper);
        }
        if (stockedShopkeepers.empty()) return;

        const auto elapsedDays = static_cast<std::uint32_t>(std::clamp(now, 0.0F, 4294967.0F) * 1000.0F);
        std::uint32_t seed = location->GetFormID() ^ (elapsedDays * 0x9E3779B9U) ^
            (static_cast<std::uint32_t>(-static_cast<std::int64_t>(localReputation)) * 0x85EBCA6BU);
        seed ^= seed >> 16;
        seed *= 0x7FEB352DU;
        seed ^= seed >> 15;
        const auto chance = static_cast<std::uint32_t>(std::clamp<std::int64_t>(10 - static_cast<std::int64_t>(localReputation) * 2, 10, 75));
        const bool robberyOccurred = seed % 100U < chance;
        auto* shopkeeper = stockedShopkeepers[(seed >> 8) % stockedShopkeepers.size()];
        std::uint32_t goodsDropped{};
        std::string droppedItemNames;
        if (robberyOccurred && shopkeeper) {
            auto inventory = shopkeeper->GetInventory();
            std::vector<RE::TESBoundObject*> shopGoods;
            for (const auto& [object, entry] : inventory) {
                if (!object || entry.first <= 0 || !entry.second || entry.second->IsQuestObject()) continue;
                shopGoods.push_back(object);
            }
            std::sort(shopGoods.begin(), shopGoods.end(), [](const auto* left, const auto* right) {
                return left->GetFormID() < right->GetFormID();
            });
            const auto itemCount = std::min<std::size_t>(3, shopGoods.size());
            const auto firstItem = static_cast<std::size_t>(seed >> 8) % shopGoods.size();
            const auto position = shopkeeper->GetPosition();
            const auto angle = shopkeeper->GetAngle();
            for (std::size_t index = 0; index < itemCount; ++index) {
                const auto chosen = (firstItem + index) % shopGoods.size();
                auto* object = shopGoods[chosen];
                const float side = index % 2U == 0 ? -1.0F : 1.0F;
                const RE::NiPoint3 dropPosition{position.x + side * (48.0F + static_cast<float>(index) * 24.0F),
                    position.y + static_cast<float>(index) * 32.0F, position.z};
                shopkeeper->RemoveItem(object, 1, RE::ITEM_REMOVE_REASON::kDropping, nullptr, nullptr, &dropPosition, &angle);
                const auto* itemName = object->GetName();
                if (!droppedItemNames.empty()) droppedItemNames += ", ";
                droppedItemNames += itemName && *itemName ? itemName : "unidentified goods";
                ++goodsDropped;
            }
            if (goodsDropped) {
                shopkeeper->SetEmotionType(RE::EmotionType::kAnger);
                shopkeeper->SetEmotionValue(100);
                shopkeeper->EvaluatePackage(true, true);
            }
        }

        UpdateRegionalShopRobberyState(holdKey, now, goodsDropped > 0);
        if (!goodsDropped) return;
        EventRecord robbery{};
        robbery.type = 17;
        robbery.gameDay = now;
        robbery.formID = shopkeeper->GetFormID();
        robbery.locationFormID = location->GetFormID();
        robbery.stage = static_cast<std::uint16_t>(goodsDropped);
        CopyFormPlugin(robbery.formPlugin, sizeof(robbery.formPlugin), shopkeeper);
        CopyFormPlugin(robbery.locationPlugin, sizeof(robbery.locationPlugin), location);
        const auto* shopName = shopkeeper->GetName();
        CopyText(robbery.subject, sizeof(robbery.subject), shopName && *shopName ? shopName : "Local shopkeeper");
        CopyText(robbery.location, sizeof(robbery.location), location->GetName() && *location->GetName() ? location->GetName() : RegionalHoldLabel(holdKey));
        CopyText(robbery.identity, sizeof(robbery.identity), "SkyRewrite/RegionalShopRobberyEvent/v1");
        std::snprintf(robbery.detail, sizeof(robbery.detail), "Shop robbed in %.16s: %u items scattered: %.54s; owner angry", RegionalHoldLabel(holdKey), goodsDropped, droppedItemNames.c_str());
        AddEvent(robbery);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    bool IsFavorQuest(RE::TESQuest* quest)
    {
        if (!quest || !quest->IsCompleted()) return false;
        std::string name = quest->GetName() ? quest->GetName() : "";
        if (const auto* editorID = quest->GetFormEditorID(); editorID && *editorID) {
            name.push_back(' ');
            name += editorID;
        }
        name = LowerAscii(name);
        const auto* editorID = quest->GetFormEditorID();
        const auto editor = editorID ? LowerAscii(editorID) : std::string{};
        const bool guildQuest = editor.rfind("tg", 0) == 0 || name.find("thieves guild") != std::string::npos || name.find("thievesguild") != std::string::npos;
        const bool brotherhoodQuest = editor.rfind("db", 0) == 0 || name.find("dark brotherhood") != std::string::npos || name.find("darkbrotherhood") != std::string::npos;
        return !guildQuest && !brotherhoodQuest;
    }

    std::string FavorQuestIdentity(RE::TESQuest* quest)
    {
        const auto* editorID = quest ? quest->GetFormEditorID() : nullptr;
        const auto* file = quest ? quest->GetFile() : nullptr;
        const auto plugin = file ? file->GetFilename() : std::string_view{};
        char identity[96]{};
        if (editorID && *editorID) {
            std::snprintf(identity, sizeof(identity), "SkyRewrite/ReputationFavor/v1/%.20s/%.48s", plugin.data(), editorID);
        } else if (quest) {
            std::snprintf(identity, sizeof(identity), "SkyRewrite/ReputationFavor/v1/%08X", quest->GetFormID());
        }
        return identity;
    }

    bool RecordFavorQuestCompletion(RE::TESQuest* quest, RE::PlayerCharacter* player)
    {
        if (!IsFavorQuest(quest)) return false;
        const auto identity = FavorQuestIdentity(quest);
        if (identity.empty()) return false;
        {
            std::scoped_lock lock(g_eventsMutex);
            if (std::any_of(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                return IsReputationState(event) && std::string_view(event.identity) == identity;
            })) return false;
        }
        EventRecord marker{};
        marker.type = 16;
        marker.gameDay = CurrentGameDay();
        marker.formID = quest->GetFormID();
        marker.stage = 1;
        CopyFormPlugin(marker.formPlugin, sizeof(marker.formPlugin), quest);
        CopyText(marker.subject, sizeof(marker.subject), quest->GetName() && *quest->GetName() ? quest->GetName() : quest->GetFormEditorID());
        CopyText(marker.identity, sizeof(marker.identity), identity.c_str());
        CopyText(marker.detail, sizeof(marker.detail), "Completed a favor quest");
        AddLocation(marker, player);
        AddEvent(marker);
        if (player) {
            ApplyBanditTradeResponse();
            RE::SendHUDMessage::ShowHUDMessage("SkyRewrite: Your good deeds are earning Skyrim's gratitude.", nullptr, true);
        }
        return true;
    }

    std::uint32_t FavorQuestCount()
    {
        std::scoped_lock lock(g_eventsMutex);
        return static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [](const EventRecord& event) {
            return IsReputationState(event) && std::string_view(event.identity).find("SkyRewrite/ReputationFavor/v1/") == 0;
        }));
    }

    std::int32_t ReputationScore()
    {
        const std::int64_t positive = static_cast<std::int64_t>(FavorQuestCount()) + RegionalCreatureCount("bandit") +
            RegionalCreatureCount("forsworn") + RegionalCreatureCount("vampire") + RegionalCreatureCount("monster");
        const std::int64_t negative = static_cast<std::int64_t>(RegionalCreatureCount("thefts")) +
            static_cast<std::int64_t>(RegionalCreatureCount("forgeries")) +
            static_cast<std::int64_t>(RegionalCreatureCount("murders")) * 5;
        return static_cast<std::int32_t>(std::clamp<std::int64_t>(positive - negative,
            -500, std::numeric_limits<std::int32_t>::max()));
    }

    void ApplyMurderTierResponses(RE::PlayerCharacter* player)
    {
        auto* processes = RE::ProcessLists::GetSingleton();
        if (!player || !processes) return;
        constexpr std::uint8_t kAngry = 1U << 0;
        constexpr std::uint8_t kRefusesDialogue = 1U << 1;
        constexpr std::uint8_t kAttacks = 1U << 2;
        constexpr std::uint8_t kFlees = 1U << 3;
        constexpr float kResponseRadiusSquared = 4000.0F * 4000.0F;
        const auto murders = RegionalCreatureCount("murders");
        const auto score = ReputationScore();
        const auto playerPosition = player->GetPosition();

        processes->ForAllActors([&](RE::Actor* actor) {
            if (!actor || actor == player || actor->IsDead() || actor->IsPlayerTeammate() || !actor->IsHumanoid()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            const auto formID = actor->GetFormID();
            const auto position = actor->GetPosition();
            const auto dx = position.x - playerPosition.x;
            const auto dy = position.y - playerPosition.y;
            const auto dz = position.z - playerPosition.z;
            const bool inRange = dx * dx + dy * dy + dz * dz <= kResponseRadiusSquared;
            auto reaction = g_murderReactions.find(formID);
            if (!inRange && reaction == g_murderReactions.end()) return RE::BSContainer::ForEachResult::kContinue;

            std::uint8_t desiredFlags{};
            if (score <= 0) {
                if (murders > 50 || score <= -1) desiredFlags |= kAngry;
                if (murders > 100 || score <= -51) desiredFlags |= kRefusesDialogue;
                if (murders > 200 || (score <= -201 && formID % 8U == 0) || (score <= -351 && formID % 3U == 0)) desiredFlags |= kAttacks;
                if (murders > 500 || (score <= -151 && formID % 2U == 0) || (score <= -351 && formID % 2U != 0)) desiredFlags |= kFlees;
            }

            const auto previousFlags = reaction == g_murderReactions.end() ? 0U : reaction->second;
            auto flags = static_cast<std::uint8_t>(previousFlags);
            const auto removedFlags = static_cast<std::uint8_t>(previousFlags & ~desiredFlags);
            if (removedFlags & kRefusesDialogue) actor->AllowPCDialogue(true);
            if (removedFlags & kAttacks) actor->StopCombat();
            if (removedFlags & kFlees) actor->EvaluatePackage(true, true);
            flags = static_cast<std::uint8_t>(previousFlags & desiredFlags);

            if ((desiredFlags & kRefusesDialogue) && !(flags & kRefusesDialogue)) {
                actor->AllowPCDialogue(false);
                flags |= kRefusesDialogue;
            }
            if ((desiredFlags & kAttacks) && !(flags & kAttacks) && !actor->IsHostileToActor(player) && !actor->IsEssential()) {
                if (actor->StartCombat(player)) flags |= kAttacks;
            }
            if ((desiredFlags & kFlees) && !(flags & kFlees)) {
                actor->InitiateFlee(player, true, true, false, actor->GetParentCell(), nullptr, 512.0F, 2048.0F);
                flags |= kFlees;
            }
            if ((desiredFlags & kAngry) && !(flags & kAngry)) flags |= kAngry;
            if (desiredFlags & kFlees) {
                actor->SetEmotionType(RE::EmotionType::kFear);
                actor->SetEmotionValue(100);
            } else if (desiredFlags & kAngry) {
                actor->SetEmotionType(RE::EmotionType::kAnger);
                actor->SetEmotionValue(100);
            } else if (removedFlags & (kAngry | kFlees)) {
                actor->SetEmotionType(RE::EmotionType::kNeutral);
                actor->SetEmotionValue(0);
            }

            if (!flags) {
                if (reaction != g_murderReactions.end()) g_murderReactions.erase(reaction);
                RemoveMurderReactionState(formID);
            } else {
                g_murderReactions[formID] = flags;
                if (flags != previousFlags) SaveMurderReactionState(formID, flags);
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }

    void ImproveNearbyReputationRelationships(RE::PlayerCharacter* player, std::int32_t score)
    {
        auto* processes = RE::ProcessLists::GetSingleton();
        if (!player || !processes) return;
        const auto desiredRank = score >= 201 ? 4 : score >= 100 ? 3 : score >= 50 ? 2 : score >= 35 ? 1 : 0;
        if (!desiredRank) return;
        const auto playerPosition = player->GetPosition();
        constexpr float kRecognitionRadiusSquared = 3000.0F * 3000.0F;
        processes->ForAllActors([&](RE::Actor* actor) {
            if (!actor || actor == player || actor->IsDead() || actor->IsPlayerTeammate() || actor->IsHostileToActor(player)) return RE::BSContainer::ForEachResult::kContinue;
            const auto position = actor->GetPosition();
            const auto dx = position.x - playerPosition.x;
            const auto dy = position.y - playerPosition.y;
            const auto dz = position.z - playerPosition.z;
            if (dx * dx + dy * dy + dz * dz > kRecognitionRadiusSquared) return RE::BSContainer::ForEachResult::kContinue;
            ImproveRelationshipRank(actor, player, desiredRank);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }

    bool IsReputationGiftThreshold(std::uint32_t score)
    {
        if (score >= 201) return true;
        if (score <= 35) return score && score % 5 == 0;
        if (score <= 50) return score == 50 || (score - 35) % 4 == 0;
        if (score <= 100) return score == 100 || (score - 50) % 3 == 0;
        return score == 201 || (score - 100) % 2 == 0;
    }

    std::uint32_t NextReputationGiftThreshold(std::uint32_t afterScore)
    {
        for (std::uint32_t offset = 1; offset <= 5; ++offset) {
            if (afterScore > std::numeric_limits<std::uint32_t>::max() - offset) return std::numeric_limits<std::uint32_t>::max();
            const auto candidate = afterScore + offset;
            if (IsReputationGiftThreshold(candidate)) return candidate;
        }
        return std::numeric_limits<std::uint32_t>::max();
    }

    std::uint32_t LatestReputationGiftThreshold(std::uint32_t score)
    {
        for (std::uint32_t offset = 0; offset <= 5 && offset <= score; ++offset) {
            const auto candidate = score - offset;
            if (IsReputationGiftThreshold(candidate)) return candidate;
        }
        return 0;
    }

    RE::Actor* FindNearbyReputationGiftResident(RE::PlayerCharacter* player)
    {
        auto* processes = RE::ProcessLists::GetSingleton();
        if (!player || !processes) return nullptr;
        constexpr float kGiftGreetingRadiusSquared = 1800.0F * 1800.0F;
        const auto playerPosition = player->GetPosition();
        auto* playerCell = player->GetParentCell();
        RE::Actor* closest{};
        auto closestDistanceSquared = kGiftGreetingRadiusSquared;
        processes->ForAllActors([&](RE::Actor* actor) {
            if (!actor || actor == player || actor->IsDead() || actor->IsPlayerTeammate() || actor->IsGuard() ||
                !actor->IsHumanoid() || actor->IsHostileToActor(player) || actor->GetParentCell() != playerCell) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            const auto position = actor->GetPosition();
            const auto dx = position.x - playerPosition.x;
            const auto dy = position.y - playerPosition.y;
            const auto dz = position.z - playerPosition.z;
            const auto distanceSquared = dx * dx + dy * dy + dz * dz;
            if (distanceSquared < closestDistanceSquared) {
                closest = actor;
                closestDistanceSquared = distanceSquared;
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        return closest;
    }

    void AwardReputationGiftIfEarned(RE::PlayerCharacter* player)
    {
        if (!player) return;
        const auto reputation = ReputationScore();
        ApplyMurderTierResponses(player);
        ImproveNearbyReputationRelationships(player, reputation);
        if (reputation <= 0) return;
        const auto score = static_cast<std::uint32_t>(reputation);

        std::uint32_t previousGiftScore{};
        std::uint32_t previousGiftCount{};
        {
            std::scoped_lock lock(g_eventsMutex);
            const auto saved = std::find_if(g_events.begin(), g_events.end(), [](const EventRecord& event) {
                return IsReputationState(event) && std::string_view(event.identity) == "SkyRewrite/ReputationGift/v1/last";
            });
            if (saved != g_events.end()) {
                previousGiftScore = saved->formID ? saved->formID : static_cast<std::uint32_t>(saved->stage) * 5U;
                previousGiftCount = saved->stage;
            }
        }
        const auto giftThreshold = NextReputationGiftThreshold(previousGiftScore);
        if (!giftThreshold || giftThreshold > score) return;
        const auto earnedThreshold = LatestReputationGiftThreshold(score);
        if (!earnedThreshold || earnedThreshold <= previousGiftScore) return;

        // Release a reward through a nearby resident's vanilla greet. If there
        // is nobody available to meet the player, leave the milestone pending
        // until a later reputation-earning event in a populated area.
        auto* donor = FindNearbyReputationGiftResident(player);
        if (!donor) return;

        struct Gift { RE::FormID formID; const char* label; std::uint16_t quantity; bool unique = false; };
        static constexpr std::array<Gift, 3> modestGifts{{
            {0x0003EAF2U, "Potion of Minor Healing", 1},
            {0x0001D4ECU, "Torch", 2},
            {0x0000000AU, "Lockpick", 5}
        }};
        static constexpr std::array<Gift, 4> recognizedGifts{{
            {0x00063B45U, "Garnet", 1},
            {0x00063B46U, "Amethyst", 1},
            {0x00063B44U, "Sapphire", 1},
            {0x00063B42U, "Ruby", 1}
        }};
        static constexpr std::array<Gift, 4> honoredGifts{{
            {0x00068520U, "Flawless Emerald", 1},
            {0x00068522U, "Flawless Ruby", 1},
            {0x00068523U, "Flawless Sapphire", 1},
            {0x0005AD9EU, "Gold Ingot", 1}
        }};
        static constexpr std::array<Gift, 4> veneratedGifts{{
            {0x0006851EU, "Flawless Diamond", 1},
            {0x0002E4FCU, "Grand Soul Gem (Filled)", 1},
            {0x0002E504U, "Black Soul Gem (Filled)", 1},
            {0x0005AD9DU, "Ebony Ingot", 1}
        }};
        static constexpr std::array<Gift, 6> legendaryGifts{{
            {0x0006851EU, "Flawless Diamond", 1},
            {0x0002E4FCU, "Grand Soul Gem (Filled)", 1},
            {0x0002E504U, "Black Soul Gem (Filled)", 1},
            {0x0005AD9DU, "Ebony Ingot", 2},
            {0x000A4DCEU, "Bloodthorn", 1, true},
            {0x000C1989U, "Bolar's Oathblade", 1, true}
        }};
        const auto seed = score * 2654435761U + player->GetFormID() + earnedThreshold * 97U;
        const Gift* chosen{};
        if (score < 35) chosen = &modestGifts[seed % modestGifts.size()];
        else if (score < 50) chosen = &recognizedGifts[seed % recognizedGifts.size()];
        else if (score < 100) chosen = &honoredGifts[seed % honoredGifts.size()];
        else if (score < 201) chosen = &veneratedGifts[seed % veneratedGifts.size()];
        else chosen = &legendaryGifts[seed % legendaryGifts.size()];

        // Keep the high-reputation unique weapons genuinely unique per save.
        // If a candidate was granted before, substitute a valuable repeatable
        // reward rather than handing out another copy.
        if (chosen->unique) {
            bool alreadyGranted{};
            char uniqueGiftIdentity[64]{};
            std::snprintf(uniqueGiftIdentity, sizeof(uniqueGiftIdentity), "SkyRewrite/ReputationUniqueGift/v1/%08X", chosen->formID);
            {
                std::scoped_lock lock(g_eventsMutex);
                alreadyGranted = std::any_of(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
                    return (IsReputationUniqueGiftState(event) && std::string_view(event.identity) == uniqueGiftIdentity) ||
                        (event.type == 15 && event.formID == chosen->formID);
                });
            }
            if (alreadyGranted) chosen = &legendaryGifts[seed % 4U];
        }

        auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(chosen->formID);
        const bool itemFallback = item == nullptr;
        if (itemFallback) item = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000FU);
        if (!item) return;
        if (!donor->SetDialogueWithPlayer(true, true, nullptr)) return;
        donor->InitiateDialogue(player, nullptr, nullptr);
        const auto qualityStep = score / 5U;
        const auto goldQuantity = static_cast<std::int32_t>(25U + std::min(qualityStep, 199U) * 25U);
        auto* septims = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000FU);
        if (septims) player->AddObjectToContainer(septims, nullptr, goldQuantity, nullptr);
        const auto quantity = itemFallback ? 0 : static_cast<std::int32_t>(chosen->quantity + (score >= 201 ? std::min((score - 201U) / 50U, 4U) : 0U));
        if (quantity > 0) player->AddObjectToContainer(item, nullptr, quantity, nullptr);

        if (chosen->unique && !itemFallback) {
            EventRecord uniqueGiftState{};
            uniqueGiftState.type = 16;
            uniqueGiftState.gameDay = CurrentGameDay();
            uniqueGiftState.formID = item->GetFormID();
            uniqueGiftState.stage = 1;
            CopyText(uniqueGiftState.subject, sizeof(uniqueGiftState.subject), chosen->label);
            std::snprintf(uniqueGiftState.identity, sizeof(uniqueGiftState.identity), "SkyRewrite/ReputationUniqueGift/v1/%08X", item->GetFormID());
            CopyText(uniqueGiftState.detail, sizeof(uniqueGiftState.detail), "Unique reputation reward already granted in this save");
            AddEvent(uniqueGiftState);
        }

        EventRecord marker{};
        marker.type = 16;
        marker.gameDay = CurrentGameDay();
        marker.formID = earnedThreshold;
        marker.stage = static_cast<std::uint16_t>(std::min(previousGiftCount + 1U, static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max())));
        CopyText(marker.subject, sizeof(marker.subject), "Reputation reward milestone");
        CopyText(marker.identity, sizeof(marker.identity), "SkyRewrite/ReputationGift/v1/last");
        CopyText(marker.detail, sizeof(marker.detail), "Tracks the last granted reputation gift threshold");
        AddEvent(marker);

        EventRecord gift{};
        gift.type = 15;
        gift.gameDay = marker.gameDay;
        gift.formID = item->GetFormID();
        gift.stage = static_cast<std::uint16_t>(quantity);
        CopyFormPlugin(gift.formPlugin, sizeof(gift.formPlugin), item);
        AddLocation(gift, player);
        const char* itemLabel = itemFallback ? "Septims" : (item->GetName() && *item->GetName() ? item->GetName() : chosen->label);
        CopyText(gift.subject, sizeof(gift.subject), itemLabel);
        const char* donorName = donor->GetName() && *donor->GetName() ? donor->GetName() : "A grateful resident";
        if (itemFallback) std::snprintf(gift.detail, sizeof(gift.detail), "%.72s thanked you in person and gave you %d septims at reputation %u", donorName, goldQuantity, score);
        else std::snprintf(gift.detail, sizeof(gift.detail), "%.72s thanked you in person and gave you %d septims and %d %s at reputation %u", donorName, goldQuantity, quantity, itemLabel, score);
        std::snprintf(gift.identity, sizeof(gift.identity), "SkyRewrite/ReputationGiftEvent/v1/%u", earnedThreshold);
        AddEvent(gift);
        char message[160]{};
        if (itemFallback) std::snprintf(message, sizeof(message), "%.48s thanks you in person and gives you %d septims.", donorName, goldQuantity);
        else std::snprintf(message, sizeof(message), "%.40s thanks you and gives you %d septims and %d %s.", donorName, goldQuantity, quantity, itemLabel);
        RE::SendHUDMessage::ShowHUDMessage(message, nullptr, true);
    }

    bool HasBountyResolution(std::size_t index)
    {
        if (index >= g_events.size() || !IsTemporaryBountyApplied(g_events[index])) return false;
        return std::any_of(g_events.begin() + static_cast<std::ptrdiff_t>(index + 1), g_events.end(), [&](const EventRecord& event) {
            return event.type == 7 && std::string_view(event.identity) == g_events[index].identity;
        });
    }

    bool PendingTemporaryBountiesMatchBalance(RE::PlayerCharacter* player, RE::TESFaction* faction)
    {
        if (!player || !faction) return false;

        std::uint64_t expected = 0;
        bool found = false;
        {
            std::scoped_lock lock(g_eventsMutex);
            for (std::size_t i = 0; i < g_events.size(); ++i) {
                const auto& event = g_events[i];
                if (!IsTemporaryBountyApplied(event) || event.formID != faction->GetFormID() || HasBountyResolution(i)) continue;
                if (!found) {
                    expected = event.baseFormID;
                    found = true;
                } else if (expected != event.baseFormID) {
                    // Another system changed the hold balance between our
                    // bounty additions. Preserve it rather than guessing.
                    return false;
                }
                expected += event.stage;
                if (expected > std::numeric_limits<std::uint32_t>::max()) return false;
            }
        }

        return found && player->GetNonViolentCrimeGoldValue(faction) == expected;
    }

    void ShiftLaterTemporaryBountyBaselines(const EventRecord& removed)
    {
        const auto amount = static_cast<std::uint32_t>(removed.stage);
        const auto removedEnd = static_cast<std::uint64_t>(removed.baseFormID) + amount;
        if (!amount || removedEnd > std::numeric_limits<std::uint32_t>::max()) return;

        std::scoped_lock lock(g_eventsMutex);
        const auto removedIt = std::find_if(g_events.begin(), g_events.end(), [&](const EventRecord& event) {
            return IsTemporaryBountyApplied(event) && std::string_view(event.identity) == removed.identity;
        });
        if (removedIt == g_events.end()) return;
        for (auto it = std::next(removedIt); it != g_events.end(); ++it) {
            const auto index = static_cast<std::size_t>(it - g_events.begin());
            if (!IsTemporaryBountyApplied(*it) || it->formID != removed.formID || HasBountyResolution(index)) continue;
            if (it->baseFormID >= removedEnd) it->baseFormID -= amount;
        }
    }

    void AddEvent(EventRecord event)
    {
        bool removePairedConsumableDrop = false;
        const auto now = std::chrono::steady_clock::now();
        if (event.type == 22) {
            constexpr auto kPairedRemovalWindow = std::chrono::seconds(2);
            std::scoped_lock lock(g_recentConsumableDropsMutex);
            for (auto it = g_recentConsumableDrops.begin(); it != g_recentConsumableDrops.end();) {
                if (now < it->second.capturedAt || now - it->second.capturedAt > kPairedRemovalWindow) {
                    it = g_recentConsumableDrops.erase(it);
                } else {
                    ++it;
                }
            }
            const auto found = g_recentConsumableDrops.find(event.formID);
            if (found != g_recentConsumableDrops.end() &&
                found->second.locationFormID == event.locationFormID && found->second.quantity == event.stage) {
                removePairedConsumableDrop = true;
                g_recentConsumableDrops.erase(found);
            }
        }
        {
            std::scoped_lock lock(g_eventsMutex);
            if (removePairedConsumableDrop) {
                const auto pairedDrop = std::find_if(g_events.rbegin(), g_events.rend(), [&](const EventRecord& prior) {
                    return prior.type == 4 && prior.formID == event.formID &&
                        prior.locationFormID == event.locationFormID && prior.stage == event.stage &&
                        std::string_view(prior.identity) == "SkyRewrite/InventoryAction/v1/dropped";
                });
                if (pairedDrop != g_events.rend()) {
                    g_events.erase(std::next(pairedDrop).base());
                    SKSE::log::info("SkyRewrite removed a paired drop record for consumed item {:08X}", event.formID);
                }
            }
            const auto retention = g_journalRetention.load(std::memory_order_relaxed);
            while (g_events.size() >= retention) {
                auto removable = g_events.end();
                for (auto it = g_events.begin(); it != g_events.end(); ++it) {
                    const auto index = static_cast<std::size_t>(it - g_events.begin());
                    if (!IsProtectedWorldResponseEvent(index)) { removable = it; break; }
                }
                if (removable == g_events.end()) break;
                g_events.erase(removable);
            }
            g_events.push_back(event);
        }
        if (event.type == 4 && std::string_view(event.identity) == "SkyRewrite/InventoryAction/v1/dropped") {
            std::scoped_lock lock(g_recentConsumableDropsMutex);
            if (g_recentConsumableDrops.size() >= 128 && !g_recentConsumableDrops.contains(event.formID)) {
                g_recentConsumableDrops.clear();
            }
            g_recentConsumableDrops[event.formID] = { event.locationFormID, event.stage, now };
        }
        NotifyDashboardHistoryChanged();
    }

    bool IsPairedConsumableDropRecord(const EventRecord& dropped, const EventRecord& consumed)
    {
        if (dropped.type != 4 || consumed.type != 22 ||
            std::string_view(dropped.identity) != "SkyRewrite/InventoryAction/v1/dropped" ||
            dropped.formID != consumed.formID || dropped.locationFormID != consumed.locationFormID ||
            dropped.stage != consumed.stage || !std::isfinite(dropped.gameDay) || !std::isfinite(consumed.gameDay)) {
            return false;
        }
        return std::lround(dropped.gameDay * 10.0F) == std::lround(consumed.gameDay * 10.0F);
    }

    void RetryGuardDispatchForActiveBounty(RE::PlayerCharacter* player, RE::BGSLocation* location, float now)
    {
        if (!player || !location || !g_dispatchGuardOnBounty.load(std::memory_order_relaxed)) return;
        auto* faction = ResolveCurrentHoldCrimeFaction(player, location);
        if (!faction) return;

        EventRecord bounty{};
        bool found = false;
        std::uint32_t guardsAlreadySpawned = 0;
        std::uint32_t guardsStillNeeded = 0;
        {
            std::scoped_lock lock(g_eventsMutex);
            for (std::size_t i = 0; i < g_events.size(); ++i) {
                const auto& candidate = g_events[i];
                if (!IsTemporaryBountyApplied(candidate) || candidate.formID != faction->GetFormID() || HasBountyResolution(i)) continue;
                const auto expected = static_cast<std::uint64_t>(candidate.baseFormID) + candidate.stage;
                if (expected > std::numeric_limits<std::uint32_t>::max() || player->GetNonViolentCrimeGoldValue(faction) < expected) continue;
                auto alreadySpawned = GuardCountFromDetail(candidate.detail);
                std::uint32_t retryAttempts = 0;
                for (const auto& prior : g_events) {
                    if (prior.type != 13 || std::string_view(prior.identity) != candidate.identity) continue;
                    alreadySpawned += GuardCountFromDetail(prior.detail);
                    ++retryAttempts;
                }
                alreadySpawned = std::min(alreadySpawned, 2U);
                // Keep a few safe retries available for active bounties. This
                // also lets an already-recorded failure retry after a plugin
                // update changes the placement logic.
                if (alreadySpawned >= 2 || retryAttempts >= 3) continue;
                bounty = candidate;
                guardsAlreadySpawned = alreadySpawned;
                guardsStillNeeded = 2U - alreadySpawned;
                found = true;
                break;
            }
        }
        if (!found) return;

        std::uint32_t spawnedThisAttempt = 0;
        const auto result = SpawnHoldGuardResponse(player, faction, guardsStillNeeded, &spawnedThisAttempt);
        EventRecord outcome{};
        outcome.type = 13;
        outcome.gameDay = now;
        outcome.formID = faction->GetFormID();
        outcome.locationFormID = location->GetFormID();
        outcome.stage = bounty.stage;
        CopyFormPlugin(outcome.formPlugin, sizeof(outcome.formPlugin), faction);
        CopyFormPlugin(outcome.locationPlugin, sizeof(outcome.locationPlugin), location);
        const auto* name = location->GetName();
        if (!name || !*name) name = location->GetFormEditorID();
        CopyText(outcome.subject, sizeof(outcome.subject), name && *name ? name : "Unknown hold");
        CopyText(outcome.location, sizeof(outcome.location), outcome.subject);
        CopyText(outcome.identity, sizeof(outcome.identity), bounty.identity);
        const auto guardsTotal = std::min(2U, guardsAlreadySpawned + spawnedThisAttempt);
        const char* dispatchStatus = result == GuardDispatchResult::dispatched ? "" :
            result == GuardDispatchResult::partiallyDispatched ? "; partial" :
            result == GuardDispatchResult::spawnFailed ? "; spawn failed" : "; no matching guard";
        std::snprintf(outcome.detail, sizeof(outcome.detail), "Guard retry: guards=%u/%u this attempt; %u/2 total%s",
            spawnedThisAttempt, guardsStillNeeded, guardsTotal, dispatchStatus);
        AddEvent(outcome);
    }

    std::string JsonEscape(const char* value)
    {
        std::string out;
        if (!value) return out;
        for (const unsigned char ch : std::string_view(value)) {
            switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: if (ch >= 0x20) out += static_cast<char>(ch); break;
            }
        }
        return out;
    }

    bool LoadLegacySkyCollectionRecord(SKSE::SerializationInterface* intfc,
        std::uint32_t type, std::uint32_t version, std::uint32_t length)
    {
        if (type != 'SCAT') return false;
        using LoadLegacyRecord = bool(*)(SKSE::SerializationInterface*, std::uint32_t, std::uint32_t, std::uint32_t);
        const auto module = REX::W32::GetModuleHandleW(L"SkyCollection.dll");
        const auto loadRecord = module ? reinterpret_cast<LoadLegacyRecord>(
            REX::W32::GetProcAddress(module, "SkyCollection_LoadLegacySaveRecord")) : nullptr;
        if (loadRecord && loadRecord(intfc, type, version, length)) return true;

        std::array<std::byte, 4096> discard{};
        auto remaining = length;
        while (remaining) {
            const auto chunk = std::min<std::uint32_t>(remaining, static_cast<std::uint32_t>(discard.size()));
            if (intfc->ReadRecordData(discard.data(), chunk) != chunk) break;
            remaining -= chunk;
        }
        SKSE::log::warn("SkyRewrite found a legacy SkyCollection save record, but SkyCollection.dll is unavailable; the record was skipped");
        return true;
    }

    void Save(SKSE::SerializationInterface* intfc)
    {
        if (!intfc) {
            SKSE::log::error("SkyRewrite could not save its journal: serialization interface is unavailable");
            return;
        }
        std::vector<EventRecord> events;
        {
            std::scoped_lock lock(g_eventsMutex);
            events = g_events;
        }
        if (!intfc->OpenRecord(kRecordType, kRecordVersion)) {
            SKSE::log::error("SkyRewrite failed to open its save record for {} journal events", events.size());
            return;
        }
        const auto count = static_cast<std::uint32_t>(events.size());
        if (!intfc->WriteRecordData(count)) {
            SKSE::log::error("SkyRewrite failed to write its journal event count ({})", count);
            return;
        }
        for (const auto& event : events) {
            if (!intfc->WriteRecordData(event)) {
                SKSE::log::error("SkyRewrite stopped writing its journal after {} of {} events", &event - events.data(), events.size());
                return;
            }
        }
        SKSE::log::info("SkyRewrite saved {} journal events", events.size());
    }

    void Load(SKSE::SerializationInterface* intfc)
    {
        if (!intfc) {
            SKSE::log::error("SkyRewrite could not load its journal: serialization interface is unavailable");
            return;
        }
        std::scoped_lock lock(g_eventsMutex);
        g_events.clear();
        g_murderReactions.clear();
        g_sourceEventsCapturedThisSession.store(0, std::memory_order_relaxed);
        g_deathEventsSeenThisSession.store(0, std::memory_order_relaxed);
        g_playerKillsCapturedThisSession.store(0, std::memory_order_relaxed);
        std::uint32_t type{}, version{}, length{};
        std::uint32_t loadedEvents{};
        bool foundJournal = false;
        bool foundCollection = false;
        while (intfc->GetNextRecordInfo(type, version, length)) {
            if (LoadLegacySkyCollectionRecord(intfc, type, version, length)) {
                foundCollection = true;
                continue;
            }
            constexpr std::uint32_t kMaxRecordBytes = sizeof(std::uint32_t) + kMaxEvents * sizeof(EventRecord);
            if (type != kRecordType || version == 0 || version > kRecordVersion || length < sizeof(std::uint32_t) || length > kMaxRecordBytes) {
                std::array<std::byte, 4096> discard{};
                auto remaining = length;
                while (remaining) {
                    const auto chunk = std::min<std::uint32_t>(remaining, static_cast<std::uint32_t>(discard.size()));
                    if (intfc->ReadRecordData(discard.data(), chunk) != chunk) break;
                    remaining -= chunk;
                }
                if (type == kRecordType) SKSE::log::warn("SkyRewrite skipped incompatible or oversized save record, version {}, {} bytes", version, length);
                continue;
            }
            foundJournal = true;
            std::vector<std::byte> data(length);
            if (intfc->ReadRecordData(data.data(), length) != length) continue;
            std::uint32_t count{};
            std::memcpy(&count, data.data(), sizeof(count));
            const auto entrySize = version == 1 ? sizeof(LegacyEventRecord) : version == 2 ? sizeof(Version2EventRecord) :
                version == 3 ? sizeof(Version3EventRecord) : version == 4 ? sizeof(Version4EventRecord) : sizeof(EventRecord);
            const auto available = static_cast<std::uint32_t>((length - sizeof(count)) / entrySize);
            count = std::min(count, std::min(kMaxEvents, available));
            for (std::uint32_t i = 0; i < count; ++i) {
                EventRecord event{};
                const auto* entry = data.data() + sizeof(count) + static_cast<std::size_t>(i) * entrySize;
                if (version == 1) {
                    LegacyEventRecord legacy{};
                    std::memcpy(&legacy, entry, sizeof(legacy));
                    event.type = legacy.type;
                    event.gameDay = legacy.gameDay;
                    event.formID = legacy.formID;
                    event.stage = legacy.stage;
                    std::memcpy(event.subject, legacy.subject, sizeof(legacy.subject));
                    std::memcpy(event.detail, legacy.detail, sizeof(legacy.detail));
                } else if (version == 2) {
                    Version2EventRecord previous{};
                    std::memcpy(&previous, entry, sizeof(previous));
                    event.type = previous.type;
                    event.gameDay = previous.gameDay;
                    event.formID = previous.formID;
                    event.locationFormID = previous.locationFormID;
                    event.stage = previous.stage;
                    std::memcpy(event.subject, previous.subject, sizeof(previous.subject));
                    std::memcpy(event.detail, previous.detail, sizeof(previous.detail));
                    std::memcpy(event.location, previous.location, sizeof(previous.location));
                    std::memcpy(event.identity, previous.identity, sizeof(previous.identity));
                } else if (version == 3) {
                    Version3EventRecord previous{};
                    std::memcpy(&previous, entry, sizeof(previous));
                    event.type = previous.type;
                    event.gameDay = previous.gameDay;
                    event.formID = previous.formID;
                    event.locationFormID = previous.locationFormID;
                    event.baseFormID = previous.baseFormID;
                    event.stage = previous.stage;
                    std::memcpy(event.subject, previous.subject, sizeof(previous.subject));
                    std::memcpy(event.detail, previous.detail, sizeof(previous.detail));
                    std::memcpy(event.location, previous.location, sizeof(previous.location));
                    std::memcpy(event.identity, previous.identity, sizeof(previous.identity));
                } else if (version == 4) {
                    Version4EventRecord previous{};
                    std::memcpy(&previous, entry, sizeof(previous));
                    event.type = previous.type;
                    event.gameDay = previous.gameDay;
                    event.formID = previous.formID;
                    event.locationFormID = previous.locationFormID;
                    event.baseFormID = previous.baseFormID;
                    std::memcpy(event.formPlugin, previous.formPlugin, sizeof(previous.formPlugin));
                    std::memcpy(event.locationPlugin, previous.locationPlugin, sizeof(previous.locationPlugin));
                    std::memcpy(event.basePlugin, previous.basePlugin, sizeof(previous.basePlugin));
                    event.stage = previous.stage;
                    std::memcpy(event.subject, previous.subject, sizeof(previous.subject));
                    std::memcpy(event.detail, previous.detail, sizeof(previous.detail));
                    std::memcpy(event.location, previous.location, sizeof(previous.location));
                    std::memcpy(event.identity, previous.identity, sizeof(previous.identity));
                } else {
                    std::memcpy(&event, entry, sizeof(event));
                }
                event.subject[sizeof(event.subject) - 1] = '\0';
                event.detail[sizeof(event.detail) - 1] = '\0';
                event.location[sizeof(event.location) - 1] = '\0';
                event.identity[sizeof(event.identity) - 1] = '\0';
                event.formPlugin[sizeof(event.formPlugin) - 1] = '\0';
                event.locationPlugin[sizeof(event.locationPlugin) - 1] = '\0';
                event.basePlugin[sizeof(event.basePlugin) - 1] = '\0';
                if (event.type == 6 || event.type == 7 || event.type == 26 || (event.type == 14 && event.formID)) {
                    RE::FormID resolvedFaction{};
                    if (intfc->ResolveFormID(event.formID, resolvedFaction)) event.formID = resolvedFaction;
                    else event.formID = 0;
                } else if (event.type == 8 || event.type == 9) {
                    RE::FormID resolvedActor{};
                    if (intfc->ResolveFormID(event.formID, resolvedActor)) event.formID = resolvedActor;
                    else event.formID = 0;
                }
                g_events.push_back(event);
                ++loadedEvents;
            }
        }
        if (!foundCollection) {
            SKSE::log::warn("SkyCollection save record is absent; checking for a legacy catalog after load");
        }
        for (const auto& event : g_events) {
            if (IsMurderReactionState(event) && event.formID && event.stage) {
                g_murderReactions[event.formID] = static_cast<std::uint8_t>(event.stage);
            }
        }
        SKSE::log::info("SkyRewrite loaded {} journal events{}", loadedEvents, foundJournal ? "" : " (no compatible EVNT record found in this save)");
    }

    void Revert(SKSE::SerializationInterface*)
    {
        if (auto* processes = RE::ProcessLists::GetSingleton(); processes && !g_murderReactions.empty()) {
            processes->ForAllActors([&](RE::Actor* actor) {
                if (!actor || !g_murderReactions.contains(actor->GetFormID())) return RE::BSContainer::ForEachResult::kContinue;
                actor->AllowPCDialogue(true);
                actor->StopCombat();
                actor->EvaluatePackage(true, true);
                actor->SetEmotionType(RE::EmotionType::kNeutral);
                actor->SetEmotionValue(0);
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
        g_murderReactions.clear();
        {
            std::scoped_lock lock(g_eventsMutex);
            SKSE::log::info("SkyRewrite cleared its in-memory journal for save revert ({} events)", g_events.size());
            g_events.clear();
        }
        g_sourceEventsCapturedThisSession.store(0, std::memory_order_relaxed);
        g_deathEventsSeenThisSession.store(0, std::memory_order_relaxed);
        g_playerKillsCapturedThisSession.store(0, std::memory_order_relaxed);
        ApplyBanditTradeResponse();
    }

    void ExpireTemporaryBounties(RE::PlayerCharacter* player, float now)
    {
        if (!player || now < 0.0F) return;

        std::vector<EventRecord> pending;
        {
            std::scoped_lock lock(g_eventsMutex);
            for (std::size_t i = 0; i < g_events.size(); ++i) {
                const auto& applied = g_events[i];
                if (IsTemporaryBountyApplied(applied) && !HasBountyResolution(i) && now - applied.gameDay >= TemporaryBountyDuration(applied)) pending.push_back(applied);
            }
        }
        // Remove the newest stacked increment first so each remaining record's
        // saved before-balance continues to match after the previous removal.
        std::reverse(pending.begin(), pending.end());

        for (const auto& applied : pending) {
            auto* form = RE::TESForm::LookupByID(applied.formID);
            auto* faction = form && form->Is(RE::TESFaction::FORMTYPE) ? static_cast<RE::TESFaction*>(form) : nullptr;
            const auto current = faction ? player->GetNonViolentCrimeGoldValue(faction) : 0U;
            const auto amount = static_cast<std::uint32_t>(applied.stage);
            const bool canUndo = faction && amount > 0 && PendingTemporaryBountiesMatchBalance(player, faction);

            EventRecord resolution{};
            resolution.type = 7;
            resolution.gameDay = now;
            resolution.formID = applied.formID;
            CopyText(resolution.formPlugin, sizeof(resolution.formPlugin), applied.formPlugin);
            resolution.locationFormID = applied.locationFormID;
            resolution.baseFormID = applied.baseFormID;
            resolution.stage = applied.stage;
            CopyText(resolution.subject, sizeof(resolution.subject), applied.subject);
            CopyText(resolution.location, sizeof(resolution.location), applied.location);
            CopyText(resolution.identity, sizeof(resolution.identity), applied.identity);
            if (canUndo) {
                player->RemoveCrimeGoldValue(faction, false, static_cast<std::int32_t>(amount));
                const auto afterExpiry = player->GetNonViolentCrimeGoldValue(faction);
                if (afterExpiry == current - amount) {
                    ShiftLaterTemporaryBountyBaselines(applied);
                    std::snprintf(resolution.detail, sizeof(resolution.detail), "Temporary hold bounty expired; removed %u septims because its balance was unchanged", amount);
                } else {
                    CopyText(resolution.detail, sizeof(resolution.detail), "Temporary hold bounty expiry attempted but balance verification failed; no further adjustment was made");
                }
            } else {
                CopyText(resolution.detail, sizeof(resolution.detail), "Temporary hold bounty expired; left the balance unchanged because it had changed or the hold could not be resolved");
            }
            AddEvent(resolution);
            if (g_localEchoNotificationEnabled.load(std::memory_order_relaxed)) {
                const auto resolved = canUndo && std::string_view(resolution.detail).find("removed") != std::string_view::npos;
                RE::SendHUDMessage::ShowHUDMessage(resolved ? "SkyRewrite: the temporary hold bounty has expired." : "SkyRewrite: bounty expiry skipped to preserve your hold balance.", nullptr, true);
            }
        }
    }

    std::uint32_t ApplyAdaptiveGuardResponse([[maybe_unused]] RE::BGSLocation* location, [[maybe_unused]] float now, [[maybe_unused]] std::uint32_t durationDays)
    {
        // Directly changing ActorValue::kConfidence from the location event
        // callback caused an access violation inside Skyrim's actor-value
        // dispatch. Keep this experimental response suspended until it can be
        // applied through a deferred, verified game-safe path.
        return 0;
    }

    void ExpireAdaptiveGuardResponses(float now)
    {
        if (now < 0.0F) return;
        std::vector<EventRecord> pending;
        {
            std::scoped_lock lock(g_eventsMutex);
            for (std::size_t i = 0; i < g_events.size(); ++i) {
                const auto& applied = g_events[i];
                if (IsGuardConfidenceApplied(applied) && !HasGuardConfidenceResolution(i) && now - applied.gameDay >= GuardConfidenceDuration(applied)) pending.push_back(applied);
            }
        }

        for (const auto& applied : pending) {
            auto* form = RE::TESForm::LookupByID(applied.formID);
            auto* actor = form && form->Is(RE::Actor::FORMTYPE) ? static_cast<RE::Actor*>(form) : nullptr;
            if (!actor) continue;  // Retry when this reference is loaded on a later transition.

            EventRecord resolution{};
            resolution.type = 9;
            resolution.gameDay = now;
            resolution.formID = applied.formID;
            resolution.locationFormID = applied.locationFormID;
            resolution.baseFormID = applied.baseFormID;
            resolution.stage = applied.stage;
            CopyFormPlugin(resolution.formPlugin, sizeof(resolution.formPlugin), actor);
            CopyFormPlugin(resolution.locationPlugin, sizeof(resolution.locationPlugin), RE::TESForm::LookupByID(applied.locationFormID));
            CopyText(resolution.subject, sizeof(resolution.subject), applied.subject);
            CopyText(resolution.location, sizeof(resolution.location), applied.location);
            CopyText(resolution.identity, sizeof(resolution.identity), applied.identity);
            CopyText(resolution.detail, sizeof(resolution.detail), "Adaptive guard response suspended for stability; no confidence value was changed");
            AddEvent(resolution);
        }
    }

    bool IsRelevantJournalQuest(const RE::TESQuest* quest)
    {
        if (!quest) return false;
        // Player-facing quests have a journal category, a visible objective,
        // or are explicitly displayed in the HUD. Internal MCM, detector,
        // and framework quests normally have none of those signals.
        return quest->GetType() != RE::QUEST_DATA::Type::kNone ||
            quest->data.flags.any(RE::QuestFlag::kDisplayedInHUD) ||
            !quest->objectives.empty();
    }

    bool HasQuestCompletionAfterLatestStart(RE::FormID questID)
    {
        std::scoped_lock lock(g_eventsMutex);
        std::size_t latestStart = 0;
        bool hasStart = false;
        for (std::size_t i = 0; i < g_events.size(); ++i) {
            if (g_events[i].type == 20 && g_events[i].formID == questID) {
                latestStart = i;
                hasStart = true;
            }
        }
        for (std::size_t i = 0; i < g_events.size(); ++i) {
            if (g_events[i].type == 19 && g_events[i].formID == questID && (!hasStart || i > latestStart)) return true;
        }
        return false;
    }

    void RecordQuestCompletion(RE::TESQuest* quest, RE::PlayerCharacter* player)
    {
        if (!IsRelevantJournalQuest(quest) || HasQuestCompletionAfterLatestStart(quest->GetFormID())) return;
        EventRecord completion{};
        completion.type = 19;
        completion.gameDay = CurrentGameDay();
        completion.formID = quest->GetFormID();
        CopyFormPlugin(completion.formPlugin, sizeof(completion.formPlugin), quest);
        AddLocation(completion, player);
        const auto* name = quest->GetName();
        const auto* editorID = quest->GetFormEditorID();
        if (!name || !*name) name = editorID;
        CopyText(completion.subject, sizeof(completion.subject), name && *name ? name : "Unknown quest");
        CopyText(completion.identity, sizeof(completion.identity), editorID);
        CopyText(completion.detail, sizeof(completion.detail), "Completed quest");
        AddEvent(completion);
    }

    void RecordQuestStart(const RE::TESQuestStartStopEvent* event, RE::TESQuest* quest, RE::PlayerCharacter* player)
    {
        if (!event || !event->started || !IsRelevantJournalQuest(quest)) return;
        EventRecord record{};
        record.type = 20;
        record.gameDay = CurrentGameDay();
        record.formID = quest->GetFormID();
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), quest);
        AddLocation(record, player);
        const auto* name = quest->GetName();
        const auto* editorID = quest->GetFormEditorID();
        if (!name || !*name) name = editorID;
        CopyText(record.subject, sizeof(record.subject), name && *name ? name : "Unknown quest");
        CopyText(record.identity, sizeof(record.identity), editorID);
        CopyText(record.detail, sizeof(record.detail), "Started quest");
        AddEvent(record);
    }

    enum class CraftingActionKind : std::uint8_t { None, Smithed, Tanned, Potion, Enchanted, Cooked };

    bool IsTanningStation(const RE::TESObjectREFR* workbenchReference)
    {
        auto* base = workbenchReference ? const_cast<RE::TESObjectREFR*>(workbenchReference)->GetObjectReference() : nullptr;
        const auto* name = ReferenceDisplayName(workbenchReference);
        const auto* editorID = base ? base->GetFormEditorID() : nullptr;
        return ContainsInsensitive(name, "tanning") || ContainsInsensitive(name, "tanner") ||
            ContainsInsensitive(editorID, "tanning") || ContainsInsensitive(editorID, "tanner");
    }

    bool IsCookingStation(const RE::TESObjectREFR* workbenchReference)
    {
        auto* base = workbenchReference ? const_cast<RE::TESObjectREFR*>(workbenchReference)->GetObjectReference() : nullptr;
        const auto* name = ReferenceDisplayName(workbenchReference);
        const auto* editorID = base ? base->GetFormEditorID() : nullptr;
        return ContainsInsensitive(name, "cook") || ContainsInsensitive(name, "oven") ||
            ContainsInsensitive(name, "hearth") || ContainsInsensitive(name, "spit") ||
            ContainsInsensitive(name, "pot") || ContainsInsensitive(editorID, "cook") ||
            ContainsInsensitive(editorID, "oven") || ContainsInsensitive(editorID, "hearth") ||
            ContainsInsensitive(editorID, "spit") || ContainsInsensitive(editorID, "pot");
    }

    CraftingActionKind ResolveCraftingAction(const RE::ItemCrafted::Event* event,
        const RE::TESObjectREFR* workbenchReference)
    {
        if (!event || !event->item) return CraftingActionKind::None;
        auto* base = workbenchReference ? const_cast<RE::TESObjectREFR*>(workbenchReference)->GetObjectReference() : nullptr;
        auto* furniture = base ? base->As<RE::TESFurniture>() : nullptr;
        const auto benchType = furniture ? furniture->workBenchData.benchType.get() : RE::TESFurniture::WorkBenchData::BenchType::kNone;
        if (IsTanningStation(workbenchReference)) return CraftingActionKind::Tanned;
        const auto* alchemyResult = event->item->As<RE::AlchemyItem>();
        if (alchemyResult && alchemyResult->IsFood() &&
            (IsCookingStation(workbenchReference) || benchType == RE::TESFurniture::WorkBenchData::BenchType::kCreateObject)) {
            return CraftingActionKind::Cooked;
        }
        if (event->unk09 || benchType == RE::TESFurniture::WorkBenchData::BenchType::kEnchanting ||
            benchType == RE::TESFurniture::WorkBenchData::BenchType::kEnchantingExperiment) {
            return CraftingActionKind::Enchanted;
        }
        if (benchType == RE::TESFurniture::WorkBenchData::BenchType::kSmithingWeapon ||
            benchType == RE::TESFurniture::WorkBenchData::BenchType::kSmithingArmor ||
            (event->unk08 && (event->unk0A || event->item->GetFormType() == RE::FormType::Weapon ||
                event->item->GetFormType() == RE::FormType::Armor))) {
            return CraftingActionKind::Smithed;
        }
        if (benchType == RE::TESFurniture::WorkBenchData::BenchType::kAlchemy ||
            benchType == RE::TESFurniture::WorkBenchData::BenchType::kAlchemyExperiment ||
            (event->unk08 && event->item->As<RE::AlchemyItem>())) {
            return CraftingActionKind::Potion;
        }
        return CraftingActionKind::None;
    }

    void CraftingPlaceLabel(char* target, std::size_t capacity,
        RE::TESObjectREFR* workbenchReference, RE::PlayerCharacter* player)
    {
        if (!target || !capacity) return;
        const auto* stationName = ReferenceDisplayName(workbenchReference);
        auto* cell = workbenchReference ? workbenchReference->GetParentCell() : nullptr;
        const auto* cellName = CellDisplayName(cell);
        auto* location = player ? player->GetCurrentLocation() : nullptr;
        auto* stationLocation = workbenchReference ? workbenchReference->GetCurrentLocation() : nullptr;
        auto* cellLocation = cell ? cell->GetLocation() : nullptr;
        const auto* stationLocationName = stationLocation ? ReferenceDisplayName(stationLocation) : nullptr;
        const auto* cellLocationName = cellLocation ? ReferenceDisplayName(cellLocation) : nullptr;
        const auto* locationName = location ? ReferenceDisplayName(location) : nullptr;
        const auto* placeName = stationLocation && stationLocation != location ? stationLocationName :
            cellLocationName ? cellLocationName :
            cell && cell->IsInteriorCell() && cellName ? cellName : locationName;
        if (!placeName) placeName = cellName;
        if (!placeName) placeName = "unknown location";
        if (!stationName || !*stationName) stationName = "workbench";

        const auto holdKey = RegionalHoldKeyForPlayer(player, location);
        const auto* holdLabel = RegionalHoldLabel(holdKey);
        char holdName[40]{};
        if (holdLabel && std::string_view(holdLabel) != "All holds") {
            std::snprintf(holdName, sizeof(holdName), "%s Hold", holdLabel);
        } else {
            CopyText(holdName, sizeof(holdName), "hold unavailable");
        }
        std::snprintf(target, capacity, "%.24s at %.34s, %.26s", stationName, placeName, holdName);
    }

    EventRecord MakeCraftMaterialUse(EventRecord material, const EventRecord& crafted,
        std::string_view placeLabel)
    {
        char materialName[sizeof(material.subject)]{};
        CopyText(materialName, sizeof(materialName), material.subject[0] ? material.subject : "unknown material");
        material.type = kEventCraftMaterialUsed;
        material.baseFormID = crafted.formID;
        CopyFormPlugin(material.basePlugin, sizeof(material.basePlugin), RE::TESForm::LookupByID(crafted.formID));
        CopyText(material.subject, sizeof(material.subject), crafted.subject);
        std::snprintf(material.identity, sizeof(material.identity), "SkyRewrite/Crafting/v1/material/%08X", material.formID);
        std::snprintf(material.detail, sizeof(material.detail), "Used %.28s x%u at %.72s",
            materialName, std::max<unsigned>(1, material.stage), placeLabel.data());
        return material;
    }

    void AddCraftMaterialUse(EventRecord material, const EventRecord& crafted,
        std::string_view placeLabel)
    {
        AddEvent(MakeCraftMaterialUse(material, crafted, placeLabel));
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void RecordCraftMaterialUses(const EventRecord& crafted, std::string_view placeLabel)
    {
        std::vector<EventRecord> pending;
        {
            std::scoped_lock lock(g_craftingMaterialsMutex);
            pending.swap(g_pendingCraftMaterials);
        }
        for (auto& material : pending) {
            AddCraftMaterialUse(material, crafted, placeLabel);
        }
    }

    void UpdateLatestCraftedPotionName(RE::FormID potionFormID, const char* displayName)
    {
        if (IsPlaceholderAlchemyName(displayName)) return;
        const auto nowMs = SteadyClockMilliseconds();
        const auto eventAtMs = g_recentCraftedPotionEventAtMs.load(std::memory_order_acquire);
        if (!eventAtMs || nowMs < eventAtMs || nowMs - eventAtMs > 5000) return;
        char resolvedName[128]{};
        CopyText(resolvedName, sizeof(resolvedName), displayName);
        bool updated = false;
        RE::FormID resolvedFormID = potionFormID;
        {
            std::scoped_lock lock(g_eventsMutex);
            for (auto it = g_events.rbegin(); it != g_events.rend(); ++it) {
                if (it->type != kEventPotionCrafted || (potionFormID && it->formID != potionFormID)) continue;
                resolvedFormID = it->formID;
                if (!IsPlaceholderAlchemyName(it->subject)) break;
                if (std::string_view(it->subject) != resolvedName) {
                    CopyText(it->subject, sizeof(it->subject), resolvedName);
                    updated = true;
                }
                break;
            }
            if (resolvedFormID) {
                for (auto& event : g_events) {
                    if (event.type != kEventCraftMaterialUsed || event.baseFormID != resolvedFormID ||
                        !IsPlaceholderAlchemyName(event.subject)) continue;
                    CopyText(event.subject, sizeof(event.subject), resolvedName);
                    updated = true;
                }
            }
        }
        if (updated) {
            SKSE::log::info("SkyRewrite updated crafted potion {:08X} with display name '{}'", potionFormID, resolvedName);
            NotifyDashboardHistoryChanged();
        }
    }

    void RememberRecentAlchemyCraftName(const char* displayName)
    {
        if (IsPlaceholderAlchemyName(displayName)) return;
        std::scoped_lock lock(g_recentAlchemyCraftNameMutex);
        CopyText(g_recentAlchemyCraftName.data(), g_recentAlchemyCraftName.size(), displayName);
        g_recentAlchemyCraftNameAtMs = SteadyClockMilliseconds();
    }

    bool CaptureCraftedPotionInventoryName(RE::FormID potionFormID)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* potion = RE::TESForm::LookupByID<RE::TESBoundObject>(potionFormID);
        if (!player || !potion) return false;
        auto inventory = player->GetInventory();
        const auto found = inventory.find(potion);
        if (found == inventory.end() || !found->second.second) return false;
        const auto* displayName = found->second.second->GetDisplayName();
        if (IsPlaceholderAlchemyName(displayName)) {
            if (auto* alchemyItem = potion->As<RE::AlchemyItem>()) {
                char generatedName[192]{};
                displayName = AlchemyDisplayName(alchemyItem, generatedName, sizeof(generatedName));
            }
        }
        if (IsPlaceholderAlchemyName(displayName)) return false;
        UpdateLatestCraftedPotionName(0, displayName);
        return true;
    }

    void ScheduleCraftedPotionInventoryNameCaptureAttempt(RE::FormID potionFormID, std::uint8_t attempt)
    {
        if (!potionFormID || attempt > 60) return;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        tasks->AddTask([potionFormID, attempt] {
            if (CaptureCraftedPotionInventoryName(potionFormID) || attempt >= 60) return;
            ScheduleCraftedPotionInventoryNameCaptureAttempt(potionFormID, static_cast<std::uint8_t>(attempt + 1));
        });
    }

    void ScheduleCraftedPotionInventoryNameCapture(RE::FormID potionFormID)
    {
        ScheduleCraftedPotionInventoryNameCaptureAttempt(potionFormID, 0);
    }

    bool RecordCraftingAction(const RE::ItemCrafted::Event* event, RE::PlayerCharacter* player)
    {
        if (!event || !event->item || !player) return false;
        const auto workbenchID = g_activeCraftingWorkbench.load(std::memory_order_acquire);
        auto* workbenchReference = InventoryContainerReference(workbenchID);
        const auto action = ResolveCraftingAction(event, workbenchReference);
        if (action == CraftingActionKind::None) return false;

        EventRecord record{};
        switch (action) {
        case CraftingActionKind::Smithed: record.type = kEventItemSmithed; break;
        case CraftingActionKind::Tanned: record.type = kEventItemTanned; break;
        case CraftingActionKind::Potion: record.type = kEventPotionCrafted; break;
        case CraftingActionKind::Enchanted: record.type = kEventItemEnchanted; break;
        case CraftingActionKind::Cooked: record.type = kEventItemCooked; break;
        default: return false;
        }
        record.gameDay = CurrentGameDay();
        record.formID = event->item->GetFormID();
        record.baseFormID = workbenchReference ? workbenchReference->GetFormID() : 0;
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), event->item);
        CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), workbenchReference);
        AddLocation(record, player);
        char generatedAlchemyName[192]{};
        const auto* itemName = ItemDisplayName(event->item, generatedAlchemyName, sizeof(generatedAlchemyName));
        if (action == CraftingActionKind::Potion && IsPlaceholderAlchemyName(itemName)) {
            const auto recentCraftName = RecentAlchemyCraftName();
            if (!recentCraftName.empty() && !IsPlaceholderAlchemyName(recentCraftName.c_str())) {
                itemName = recentCraftName.c_str();
            } else if (auto* boundItem = event->item->As<RE::TESBoundObject>()) {
                auto inventory = player->GetInventory();
                if (const auto found = inventory.find(boundItem); found != inventory.end() && found->second.second) {
                    const auto* inventoryName = found->second.second->GetDisplayName();
                    if (!IsPlaceholderAlchemyName(inventoryName)) itemName = inventoryName;
                }
            }
        }
        if (itemName && *itemName) CopyText(record.subject, sizeof(record.subject), itemName);
        else std::snprintf(record.subject, sizeof(record.subject), "Item FormID %08X", record.formID);

        char placeLabel[96]{};
        CraftingPlaceLabel(placeLabel, sizeof(placeLabel), workbenchReference, player);
        const char* actionName = action == CraftingActionKind::Tanned ? "Tanned" :
            action == CraftingActionKind::Potion ? "Crafted potion" :
            action == CraftingActionKind::Enchanted ? "Enchanted item" :
            action == CraftingActionKind::Cooked ? "Cooked" : "Smithed";
        const char* actionIdentity = action == CraftingActionKind::Tanned ? "tanned" :
            action == CraftingActionKind::Potion ? "potion" :
            action == CraftingActionKind::Enchanted ? "enchanted" :
            action == CraftingActionKind::Cooked ? "cooked" : "smithed";
        std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/Crafting/v1/%s", actionIdentity);
        std::snprintf(record.detail, sizeof(record.detail), "%s at %.88s", actionName, placeLabel);

        RecordCraftMaterialUses(record, placeLabel);
        {
            std::scoped_lock lock(g_craftingMaterialsMutex);
            g_recentCraftedAction.record = record;
            CopyText(g_recentCraftedAction.placeLabel, sizeof(g_recentCraftedAction.placeLabel), placeLabel);
            g_recentCraftedAction.recordedAt = std::chrono::steady_clock::now();
            g_recentCraftedAction.valid = true;
        }
        AddEvent(record);
        if (action == CraftingActionKind::Potion) {
            g_recentCraftedPotionFormID.store(record.formID, std::memory_order_release);
            g_recentCraftedPotionEventAtMs.store(SteadyClockMilliseconds(), std::memory_order_release);
            ScheduleCraftedPotionInventoryNameCapture(record.formID);
        }
        return true;
    }

    bool QueueCraftMaterialRemoval(const RE::TESContainerChangedEvent* event,
        RE::PlayerCharacter* player, RE::TESForm* item, bool hasRemovalContext,
        RE::ITEM_REMOVE_REASON removalReason)
    {
        constexpr RE::FormID kPlayerReferenceID = 0x14;
        if (!event || !player || !item || event->oldContainer != kPlayerReferenceID ||
            event->newContainer != 0 || !event->itemCount) return false;
        if (hasRemovalContext && removalReason != RE::ITEM_REMOVE_REASON::kRemove) return false;
        auto* ui = RE::UI::GetSingleton();
        if (!ui || !ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME)) return false;

        EventRecord material{};
        material.type = kEventCraftMaterialUsed;
        material.gameDay = CurrentGameDay();
        material.formID = event->baseObj;
        CopyFormPlugin(material.formPlugin, sizeof(material.formPlugin), item);
        AddLocation(material, player);
        char generatedAlchemyName[192]{};
        const auto* name = ItemDisplayName(item, generatedAlchemyName, sizeof(generatedAlchemyName));
        CopyText(material.subject, sizeof(material.subject), name && *name ? name : "Unknown crafting material");
        const auto quantity = event->itemCount < 0 ? -static_cast<long long>(event->itemCount) : static_cast<long long>(event->itemCount);
        material.stage = static_cast<std::uint16_t>(std::clamp<long long>(quantity, 1, std::numeric_limits<std::uint16_t>::max()));
        CopyText(material.identity, sizeof(material.identity), "SkyRewrite/Crafting/v1/pending-material");
        RecentCraftedAction recentCraft{};
        bool attachToRecentCraft = false;
        {
            std::scoped_lock lock(g_craftingMaterialsMutex);
            const auto elapsed = std::chrono::steady_clock::now() - g_recentCraftedAction.recordedAt;
            if (g_recentCraftedAction.valid && elapsed >= std::chrono::steady_clock::duration::zero() &&
                elapsed <= std::chrono::milliseconds(750)) {
                recentCraft = g_recentCraftedAction;
                attachToRecentCraft = true;
            } else if (g_pendingCraftMaterials.size() < 64) {
                g_pendingCraftMaterials.push_back(material);
            }
        }
        if (attachToRecentCraft) {
            AddCraftMaterialUse(material, recentCraft.record, recentCraft.placeLabel);
        } else {
            SKSE::log::info("SkyRewrite deferred crafting material {:08X} x{} until its crafted output is identified",
                material.formID, material.stage);
        }
        return true;
    }

    void DiscardUnmatchedCraftMaterials()
    {
        std::size_t discarded = 0;
        {
            std::scoped_lock lock(g_craftingMaterialsMutex);
            discarded = g_pendingCraftMaterials.size();
            g_pendingCraftMaterials.clear();
            g_recentCraftedAction.valid = false;
        }
        if (discarded) {
            SKSE::log::warn("SkyRewrite discarded {} crafting material removal(s) because no crafted output event arrived", discarded);
        }
    }

    bool NumbersJobForgeryAlreadyRecorded(RE::FormID questID)
    {
        std::scoped_lock lock(g_eventsMutex);
        std::size_t latestStart = 0;
        bool hasStart = false;
        for (std::size_t i = 0; i < g_events.size(); ++i) {
            if (g_events[i].type == 20 && g_events[i].formID == questID) {
                latestStart = i;
                hasStart = true;
            }
        }
        for (std::size_t i = 0; i < g_events.size(); ++i) {
            if (g_events[i].type == 24 && g_events[i].baseFormID == questID && (!hasStart || i > latestStart)) return true;
        }
        return false;
    }

    void RecordNumbersJobForgery(RE::TESQuest* quest, RE::TESObjectREFR* ledger, RE::PlayerCharacter* player)
    {
        if (!quest || !ledger || !player || NumbersJobForgeryAlreadyRecorded(quest->GetFormID())) return;
        EventRecord record{};
        record.type = 24;
        record.gameDay = CurrentGameDay();
        record.formID = ledger->GetFormID();
        record.baseFormID = quest->GetFormID();
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), ledger);
        CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), quest);
        AddLocation(record, player);
        CopyText(record.subject, sizeof(record.subject), record.location);
        std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/Forgery/v1/%08X/%08X", quest->GetFormID(), ledger->GetFormID());
        std::snprintf(record.detail, sizeof(record.detail), "Forged records at %.56s", record.location);
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
        ApplyCrimeBounty(player, record, "forgery");
        ApplyMurderTierResponses(player);
        SKSE::log::info("SkyRewrite recorded forged business records for The Numbers Job at {} ({:08X})", record.location, ledger->GetFormID());
    }

    void RecordLockpick(RE::TESObjectREFR* target, RE::PlayerCharacter* player)
    {
        if (!target || !player) return;
        auto* base = target->GetObjectReference();
        const bool isDoor = base && base->Is(RE::TESObjectDOOR::FORMTYPE);
        if (!isDoor && !target->HasContainer()) return;

        EventRecord record{};
        record.type = 21;
        record.gameDay = CurrentGameDay();
        record.formID = target->GetFormID();
        record.baseFormID = base ? base->GetFormID() : 0;
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), target);
        CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), base);
        AddLocation(record, player);

        if (isDoor) {
            // A door's current location is usually the exterior city, not the
            // building the player unlocked. Prefer a specific door name, then
            // resolve its linked interior cell/location for unnamed doors.
            const auto* doorName = target->GetDisplayFullName();
            if (!doorName || !*doorName) doorName = target->GetName();
            auto* teleport = target->extraList.GetByType<RE::ExtraTeleport>();
            auto linkedDoor = teleport && teleport->teleportData ? teleport->teleportData->linkedDoor.get() : RE::NiPointer<RE::TESObjectREFR>{};
            auto* destination = linkedDoor.get();
            auto* destinationCell = destination ? destination->GetParentCell() : nullptr;
            const auto* linkedDoorName = destination ? destination->GetDisplayFullName() : nullptr;
            if ((!linkedDoorName || !*linkedDoorName) && destination) linkedDoorName = destination->GetName();
            auto* targetCell = target->GetParentCell();
            const auto* targetCellName = CellDisplayName(targetCell);
            auto* interiorLocation = destinationCell && destinationCell->IsInteriorCell() ? destinationCell->GetLocation() : nullptr;
            auto* exteriorLocation = target->GetCurrentLocation();
            const auto* interiorLocationName = interiorLocation ? ReferenceDisplayName(interiorLocation) : nullptr;
            if (interiorLocation && exteriorLocation && interiorLocation->GetFormID() == exteriorLocation->GetFormID()) interiorLocationName = nullptr;
            const auto* destinationCellName = destinationCell && destinationCell->IsInteriorCell() ? CellDisplayName(destinationCell) : nullptr;
            if (!destinationCellName && targetCell && targetCell->IsInteriorCell()) destinationCellName = targetCellName;
            const auto* playerCellName = CellDisplayName(player->GetParentCell());
            const auto* baseName = base ? ReferenceDisplayName(base) : nullptr;
            const auto isGenericDoorName = [](const char* name) {
                if (!name || !*name) return true;
                constexpr std::array<const char*, 6> genericNames{ "Door", "Wooden Door", "Stone Door", "Gate", "Trapdoor", "Doorway" };
                return std::any_of(genericNames.begin(), genericNames.end(), [name](const char* generic) { return _stricmp(name, generic) == 0; });
            };
            const auto isEnclosingLocationName = [&](const char* name) {
                if (!name || !*name) return false;
                if (record.location[0] && _stricmp(name, record.location) == 0) return true;
                for (auto* location = exteriorLocation; location; location = location->parentLoc) {
                    const auto* locationName = ReferenceDisplayName(location);
                    if (locationName && _stricmp(name, locationName) == 0) return true;
                }
                return false;
            };
            const auto isSpecificDoorName = [&](const char* name) {
                return !isGenericDoorName(name) && !isEnclosingLocationName(name);
            };
            // Prefer the linked interior's identifying location/cell. Some
            // exterior door references report the enclosing city as their
            // display name, which made the journal say "Lockpicked Morthal".
            const auto* specificDoorName = isSpecificDoorName(interiorLocationName) ? interiorLocationName :
                isSpecificDoorName(destinationCellName) ? destinationCellName :
                isSpecificDoorName(linkedDoorName) ? linkedDoorName :
                isSpecificDoorName(doorName) ? doorName :
                isSpecificDoorName(baseName) ? baseName : nullptr;
            const auto* cellFallbackName = destinationCellName && *destinationCellName ? destinationCellName :
                targetCellName && *targetCellName ? targetCellName : playerCellName;
            const auto* resolvedName = specificDoorName ? specificDoorName :
                cellFallbackName && *cellFallbackName ? cellFallbackName :
                record.location[0] ? record.location : "Unknown door";
            CopyText(record.subject, sizeof(record.subject), resolvedName && *resolvedName ? resolvedName : "Unknown door");
            CopyText(record.identity, sizeof(record.identity), "SkyRewrite/Lockpick/v1/door");
            if (specificDoorName) {
                std::snprintf(record.detail, sizeof(record.detail), "Lockpicked door: %.56s", record.subject);
            } else if (cellFallbackName || record.location[0]) {
                std::snprintf(record.detail, sizeof(record.detail), "Lockpicked in cell: %.56s", record.subject);
            } else {
                std::snprintf(record.detail, sizeof(record.detail), "Lockpicked door: %.56s", record.subject);
            }
        } else {
            const auto* name = target->GetName();
            if (!name || !*name) name = base ? base->GetName() : nullptr;
            CopyText(record.subject, sizeof(record.subject), name && *name ? name : "Container");
            CopyText(record.identity, sizeof(record.identity), "SkyRewrite/Lockpick/v1/container");
            std::snprintf(record.detail, sizeof(record.detail), "Lockpicked container: %.48s", record.subject);
        }
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void RememberGuardDialogueStart(const RE::TESTopicInfoEvent* event, RE::PlayerCharacter* player,
        RE::Actor* npc)
    {
        if (!event || !player || !npc || !npc->IsGuard() ||
            event->type != RE::TESTopicInfoEvent::TopicInfoEventType::kTopicBegin) return;
        auto* faction = npc->GetCrimeFaction();
        if (!faction) return;

        GuardDialogueSnapshot snapshot{};
        snapshot.crimeFactionID = faction->GetFormID();
        snapshot.crimeGold = player->GetCrimeGoldValue(faction);
        if (!snapshot.crimeGold) return;
        auto* septims = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000FU);
        snapshot.playerGoldAvailable = septims != nullptr;
        snapshot.playerGold = septims ? player->GetItemCount(septims) : 0;
        snapshot.capturedAt = std::chrono::steady_clock::now();

        std::scoped_lock lock(g_guardDialogueMutex);
        for (auto it = g_guardDialogueSnapshots.begin(); it != g_guardDialogueSnapshots.end();) {
            if (snapshot.capturedAt - it->second.capturedAt > std::chrono::seconds(30)) it = g_guardDialogueSnapshots.erase(it);
            else ++it;
        }
        g_guardDialogueSnapshots[npc->GetFormID()] = snapshot;
    }

    bool TakeGuardDialogueSnapshot(RE::FormID npcFormID, GuardDialogueSnapshot& snapshot)
    {
        if (!npcFormID) return false;
        std::scoped_lock lock(g_guardDialogueMutex);
        const auto found = g_guardDialogueSnapshots.find(npcFormID);
        if (found == g_guardDialogueSnapshots.end()) return false;
        snapshot = found->second;
        g_guardDialogueSnapshots.erase(found);
        return true;
    }

    const char* InferDialogueAction(const RE::TESTopic* topic, RE::TESTopicInfo* topicInfo)
    {
        (void)topicInfo;
        if (!topic) return nullptr;
        const auto subtype = topic->data.subtype;
        if (subtype == RE::DIALOGUE_DATA::Subtype::kIntimidate) return "Intimidated";
        if (subtype == RE::DIALOGUE_DATA::Subtype::kBribe) return "Bribed";
        if (subtype == RE::DIALOGUE_DATA::Subtype::kFlatter) return "Persuaded";
        return nullptr;
    }

    void RememberPlayerDialogueSelection(const RE::TESTopicInfoEvent* event, RE::Actor* npc)
    {
        if (!event || !npc) return;
        const auto now = std::chrono::steady_clock::now();
        std::scoped_lock lock(g_playerDialogueMutex);
        for (auto it = g_playerDialogueSelections.begin(); it != g_playerDialogueSelections.end();) {
            if (now - it->second.capturedAt > std::chrono::seconds(30)) it = g_playerDialogueSelections.erase(it);
            else ++it;
        }
        g_playerDialogueSelections[npc->GetFormID()] = { event->topicInfoFormID, now };
    }

    bool TakePlayerDialogueSelection(const RE::TESTopicInfoEvent* event, RE::Actor* npc)
    {
        if (!event || !npc) return false;
        std::scoped_lock lock(g_playerDialogueMutex);
        const auto found = g_playerDialogueSelections.find(npc->GetFormID());
        if (found == g_playerDialogueSelections.end()) return false;
        const auto selection = found->second;
        g_playerDialogueSelections.erase(found);
        return selection.topicInfoID == event->topicInfoFormID &&
            std::chrono::steady_clock::now() - selection.capturedAt <= std::chrono::seconds(30);
    }

    void ScheduleGuardDialogueRelease(const GuardDialogueSnapshot& snapshot, RE::PlayerCharacter* player,
        RE::Actor* npc, RE::TESTopicInfo* topicInfo, std::uint16_t stage)
    {
        if (!player || !npc || !topicInfo || !snapshot.playerGoldAvailable || !snapshot.crimeGold) return;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;

        EventRecord record{};
        record.type = 23;
        record.gameDay = CurrentGameDay();
        record.formID = npc->GetFormID();
        record.baseFormID = topicInfo->GetFormID();
        record.stage = stage;
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), npc);
        CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), topicInfo);
        AddLocation(record, player);
        const auto* name = npc->GetName();
        if (!name || !*name) name = npc->GetFormEditorID();
        CopyText(record.subject, sizeof(record.subject), name && *name ? name : "Guard");
        std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/Dialogue/v1/guard-release/%08X/%08X/%u",
            record.formID, record.baseFormID, stage);

        tasks->AddTask([snapshot, record]() mutable {
            auto* currentPlayer = RE::PlayerCharacter::GetSingleton();
            auto* faction = RE::TESForm::LookupByID<RE::TESFaction>(snapshot.crimeFactionID);
            auto* septims = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000FU);
            if (!currentPlayer || !faction || !septims) return;
            const auto& playerFlags = currentPlayer->GetPlayerFlags();
            if (playerFlags.goToJailQueued || playerFlags.servingJailTime) return;
            if (std::chrono::steady_clock::now() - snapshot.capturedAt > std::chrono::seconds(30)) return;

            const auto currentCrimeGold = currentPlayer->GetCrimeGoldValue(faction);
            const auto currentPlayerGold = currentPlayer->GetItemCount(septims);
            if (currentCrimeGold >= snapshot.crimeGold || currentPlayerGold < snapshot.playerGold) return;

            std::snprintf(record.detail, sizeof(record.detail),
                "Persuaded guard to let the player go: %.44s (bounty %u to %u; no gold paid)",
                record.subject, snapshot.crimeGold, currentCrimeGold);
            AddEvent(record);
            g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
            SKSE::log::info("SkyRewrite recorded guard dialogue resolution for {:08X}: crime gold {} -> {}, gold unchanged",
                record.formID, snapshot.crimeGold, currentCrimeGold);
        });
    }

    void RecordDialogueAction(const RE::TESTopicInfoEvent* event, RE::PlayerCharacter* player)
    {
        if (!event || !player) return;
        auto* topicInfo = RE::TESForm::LookupByID<RE::TESTopicInfo>(event->topicInfoFormID);
        auto* topic = topicInfo ? topicInfo->parentTopic : nullptr;
        auto* speaker = event->speakerRef.get();
        auto* npc = speaker ? speaker->As<RE::Actor>() : nullptr;
        if (!npc || npc == player || npc->IsDead()) return;

        if (event->type == RE::TESTopicInfoEvent::TopicInfoEventType::kTopicBegin) {
            auto* ui = RE::UI::GetSingleton();
            if (!ui || !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) return;
            g_recentDialogueSpeakerFormID.store(npc->GetFormID(), std::memory_order_release);
            g_recentDialogueSpeakerAtMs.store(SteadyClockMilliseconds(), std::memory_order_release);
            RememberPlayerDialogueSelection(event, npc);
            RememberGuardDialogueStart(event, player, npc);
            return;
        }
        if (event->type != RE::TESTopicInfoEvent::TopicInfoEventType::kTopicEnd) return;
        if (!TakePlayerDialogueSelection(event, npc)) return;
        // Dialogue scripts generally toggle the teammate flag as the topic
        // closes. Check it on the next game task so both hire and dismissal
        // choices are observed after their Papyrus response has run.
        ScheduleFollowerStateCheck(npc);
        if (!topic || !topicInfo) return;

        GuardDialogueSnapshot guardSnapshot{};
        const bool hasGuardSnapshot = npc->IsGuard() && TakeGuardDialogueSnapshot(npc->GetFormID(), guardSnapshot);
        // Only Skyrim's explicit persuasion/intimidation/bribe topic types are
        // recorded. Generic dialogue text can mention these actions without
        // the player actually choosing one.
        const char* action = InferDialogueAction(topic, topicInfo);
        if (!action) {
            if (hasGuardSnapshot) ScheduleGuardDialogueRelease(guardSnapshot, player, npc, topicInfo, event->stage);
            return;
        }

        EventRecord record{};
        record.type = 23;
        record.gameDay = CurrentGameDay();
        record.formID = npc->GetFormID();
        record.baseFormID = event->topicInfoFormID;
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), npc);
        CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), topicInfo);
        AddLocation(record, player);
        const auto* name = npc->GetName();
        if (!name || !*name) name = npc->GetFormEditorID();
        CopyText(record.subject, sizeof(record.subject), name && *name ? name : "Unknown NPC");
        std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/Dialogue/v1/%.16s/%08X/%u", action, npc->GetFormID(), event->stage);
        std::snprintf(record.detail, sizeof(record.detail), "%s NPC: %.48s", action, record.subject);
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
    }

    void RememberPlayerItemRemovalContext(RE::FormID formID, std::int32_t count,
        RE::ITEM_REMOVE_REASON reason)
    {
        if (!formID) return;
        constexpr std::int64_t kContextLifetimeMs = 2000;
        const auto now = SteadyClockMilliseconds();
        const auto quantity = count < 0 ? -static_cast<std::int64_t>(count) : static_cast<std::int64_t>(count);
        std::scoped_lock lock(g_recentPlayerItemRemovalsMutex);
        for (auto mapIt = g_recentPlayerItemRemovals.begin(); mapIt != g_recentPlayerItemRemovals.end();) {
            auto& contexts = mapIt->second;
            contexts.erase(std::remove_if(contexts.begin(), contexts.end(), [&](const RecentPlayerItemRemoval& context) {
                return now < context.capturedAtMs || now - context.capturedAtMs > kContextLifetimeMs;
            }), contexts.end());
            if (contexts.empty()) mapIt = g_recentPlayerItemRemovals.erase(mapIt);
            else ++mapIt;
        }
        auto& contexts = g_recentPlayerItemRemovals[formID];
        if (contexts.size() >= 8) contexts.erase(contexts.begin());
        contexts.push_back({ quantity, reason, now });
    }

    bool TakeRecentPlayerItemRemovalContext(RE::FormID formID, std::int32_t eventCount,
        RE::ITEM_REMOVE_REASON& reason)
    {
        constexpr std::int64_t kContextLifetimeMs = 2000;
        const auto now = SteadyClockMilliseconds();
        const auto quantity = eventCount < 0 ? -static_cast<std::int64_t>(eventCount) : static_cast<std::int64_t>(eventCount);
        std::scoped_lock lock(g_recentPlayerItemRemovalsMutex);
        const auto found = g_recentPlayerItemRemovals.find(formID);
        if (found == g_recentPlayerItemRemovals.end()) return false;
        auto& contexts = found->second;
        contexts.erase(std::remove_if(contexts.begin(), contexts.end(), [&](const RecentPlayerItemRemoval& context) {
            return now < context.capturedAtMs || now - context.capturedAtMs > kContextLifetimeMs;
        }), contexts.end());
        if (contexts.empty()) {
            g_recentPlayerItemRemovals.erase(found);
            return false;
        }
        auto matched = std::find_if(contexts.begin(), contexts.end(), [quantity](const RecentPlayerItemRemoval& context) {
            return context.quantity == quantity;
        });
        if (matched == contexts.end() && contexts.size() == 1) matched = contexts.begin();
        if (matched == contexts.end()) return false;
        reason = matched->reason;
        contexts.erase(matched);
        if (contexts.empty()) g_recentPlayerItemRemovals.erase(found);
        return true;
    }

    RE::ObjectRefHandle CapturePlayerItemRemovalReason(RE::Actor* actor, RE::TESBoundObject* item, std::int32_t count,
        RE::ITEM_REMOVE_REASON reason, RE::ExtraDataList* extraData, RE::TESObjectREFR* moveTo,
        const RE::NiPoint3* dropLocation, const RE::NiPoint3* rotate)
    {
        if (!g_originalPlayerRemoveItem) return {};
        const auto* player = RE::PlayerCharacter::GetSingleton();
        if (actor != player || !item) {
            return g_originalPlayerRemoveItem(actor, item, count, reason, extraData, moveTo, dropLocation, rotate);
        }

        RememberPlayerItemRemovalContext(item->GetFormID(), count, reason);
        const auto previous = g_playerItemRemovalContext;
        g_playerItemRemovalContext = { actor, item->GetFormID(), reason };
        auto result = g_originalPlayerRemoveItem(actor, item, count, reason, extraData, moveTo, dropLocation, rotate);
        g_playerItemRemovalContext = previous;
        return result;
    }

    bool InstallPlayerItemRemovalReasonHook()
    {
        if (g_playerRemoveItemHookInstalled) return true;
        REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_PlayerCharacter[0] };
        const auto original = vtable.write_vfunc(0x56, CapturePlayerItemRemovalReason);
        if (!original) return false;
        g_originalPlayerRemoveItem = reinterpret_cast<ActorRemoveItemFunction>(original);
        g_playerRemoveItemHookInstalled = true;
        return true;
    }

    bool BuildDisenchantmentRecord(RE::CraftingSubMenus::EnchantConstructMenu::ItemChangeEntry* selected,
        RE::PlayerCharacter* player, EventRecord& record, RE::TESBoundObject*& disenchantedItem)
    {
        auto* entry = selected ? selected->data : nullptr;
        disenchantedItem = entry ? entry->GetObject() : nullptr;
        if (!entry || !disenchantedItem || !player) return false;
        auto* enchantment = entry->GetEnchantment();
        if (!enchantment) {
            if (auto* weapon = disenchantedItem->As<RE::TESObjectWEAP>()) enchantment = weapon->formEnchanting;
            else if (auto* armor = disenchantedItem->As<RE::TESObjectARMO>()) enchantment = armor->formEnchanting;
        }

        record.type = kEventItemDisenchanted;
        record.gameDay = CurrentGameDay();
        record.formID = disenchantedItem->GetFormID();
        record.baseFormID = enchantment ? enchantment->GetFormID() : 0;
        record.stage = 1;
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), disenchantedItem);
        CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), enchantment);
        AddLocation(record, player);
        const auto* itemName = entry->GetDisplayName();
        if (!itemName || !*itemName) itemName = ReferenceDisplayName(disenchantedItem);
        const auto* enchantmentName = ReferenceDisplayName(enchantment);
        CopyText(record.subject, sizeof(record.subject), itemName && *itemName ? itemName : "Unknown item");
        std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/Disenchantment/v1");
        std::snprintf(record.detail, sizeof(record.detail), "Learned enchantment: %.106s",
            enchantmentName ? enchantmentName : "Unknown enchantment");
        return true;
    }

    RE::CraftingSubMenus::EnchantConstructMenu::ItemChangeEntry* CurrentDisenchantItem(
        RE::CraftingSubMenus::EnchantConstructMenu* menu)
    {
        if (!menu || menu->currentCategory.get() != RE::CraftingSubMenus::EnchantConstructMenu::Category::Disenchant) {
            return nullptr;
        }
        if (auto* selected = menu->selected.item.get()) return selected;
        const auto index = static_cast<std::size_t>(menu->highlightIndex);
        if (index >= menu->listEntries.size()) return nullptr;

        auto* highlighted = menu->listEntries[index].get();
        if (!highlighted) return nullptr;
        REL::Relocation<std::uintptr_t> itemEntryVTable{
            RE::VTABLE_CraftingSubMenus__EnchantConstructMenu__ItemChangeEntry[0] };
        if (*reinterpret_cast<const std::uintptr_t*>(highlighted) != itemEntryVTable.address()) return nullptr;
        return static_cast<RE::CraftingSubMenus::EnchantConstructMenu::ItemChangeEntry*>(highlighted);
    }

    bool BuildDisenchantmentRecordFromRemovedItem(RE::TESBoundObject* disenchantedItem,
        RE::PlayerCharacter* player, EventRecord& record)
    {
        if (!disenchantedItem || !player) return false;
        auto* enchantment = static_cast<RE::EnchantmentItem*>(nullptr);
        auto inventory = player->GetInventory();
        if (const auto found = inventory.find(disenchantedItem);
            found != inventory.end() && found->second.second) {
            enchantment = found->second.second->GetEnchantment();
        }
        if (!enchantment) {
            if (auto* weapon = disenchantedItem->As<RE::TESObjectWEAP>()) enchantment = weapon->formEnchanting;
            else if (auto* armor = disenchantedItem->As<RE::TESObjectARMO>()) enchantment = armor->formEnchanting;
        }

        record.type = kEventItemDisenchanted;
        record.gameDay = CurrentGameDay();
        record.formID = disenchantedItem->GetFormID();
        record.baseFormID = enchantment ? enchantment->GetFormID() : 0;
        record.stage = 1;
        CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), disenchantedItem);
        CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), enchantment);
        AddLocation(record, player);
        const auto* itemName = ReferenceDisplayName(disenchantedItem);
        const auto* enchantmentName = ReferenceDisplayName(enchantment);
        if (itemName && *itemName) CopyText(record.subject, sizeof(record.subject), itemName);
        else std::snprintf(record.subject, sizeof(record.subject), "Item FormID %08X", record.formID);
        std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/Disenchantment/v1");
        std::snprintf(record.detail, sizeof(record.detail), "Learned enchantment: %.106s",
            enchantmentName ? enchantmentName : "Unknown enchantment");
        return true;
    }

    bool RecordDisenchantmentDuringConfirmedCallback(const RE::TESContainerChangedEvent* event,
        RE::PlayerCharacter* player)
    {
        constexpr RE::FormID kPlayerReferenceID = 0x14;
        if (!event || !player || event->oldContainer != kPlayerReferenceID || event->newContainer != 0) return false;
        auto* removedItem = RE::TESForm::LookupByID<RE::TESBoundObject>(event->baseObj);
        EventRecord record{};
        if (!BuildDisenchantmentRecordFromRemovedItem(removedItem, player, record)) return false;
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
        if (g_activeDisenchantCallback.load(std::memory_order_acquire)) {
            g_activeDisenchantItemFormID.store(event->baseObj, std::memory_order_release);
        }
        SKSE::log::info("SkyRewrite recorded disenchantment from confirmed inventory removal: '{}' and learned '{}'",
            record.subject, record.detail);
        g_recentConfirmedDisenchantAtMs.store(0, std::memory_order_release);
        return true;
    }

    void CaptureDisenchantCallbackRun(EnchantDisenchantCallback* callback, std::uint8_t button)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* selected = CurrentDisenchantItem(callback ? callback->subMenu : nullptr);
        EventRecord record{};
        RE::TESBoundObject* disenchantedItem{};
        const bool hasCandidate = BuildDisenchantmentRecord(selected, player, record, disenchantedItem);
        const auto countBefore = hasCandidate ? player->GetItemCount(disenchantedItem) : 0;
        auto* submenu = callback ? callback->subMenu : nullptr;
        SKSE::log::info("SkyRewrite disenchant callback invoked: button={}, submenu={}, category={}, selected={}, highlight={}, entries={}, count={}",
            button, submenu != nullptr,
            submenu ? static_cast<std::uint32_t>(submenu->currentCategory.get()) : std::numeric_limits<std::uint32_t>::max(),
            hasCandidate ? record.formID : 0, submenu ? submenu->highlightIndex : 0,
            submenu ? submenu->listEntries.size() : 0, countBefore);

        if (button == 0) {
            // Keep a short confirmation window because container-change events
            // can be delivered after Skyrim returns from the menu callback.
            g_recentConfirmedDisenchantAtMs.store(SteadyClockMilliseconds(), std::memory_order_release);
        }
        const auto previousCallbackActive = g_activeDisenchantCallback.exchange(button == 0, std::memory_order_acq_rel);
        const auto previousDisenchantFormID = g_activeDisenchantItemFormID.exchange(
            hasCandidate ? disenchantedItem->GetFormID() : 0, std::memory_order_acq_rel);
        if (g_originalDisenchantRun) g_originalDisenchantRun(callback, button);
        g_activeDisenchantItemFormID.store(previousDisenchantFormID, std::memory_order_release);
        g_activeDisenchantCallback.store(previousCallbackActive, std::memory_order_release);

        if (hasCandidate && player && disenchantedItem && button == 0) {
            const auto countAfter = player->GetItemCount(disenchantedItem);
            // Skyrim's confirmation callback is the authoritative success signal.
            // Inventory counts can remain unchanged when a mod replaces the
            // selected item or stores the result in an extra-data stack.
            AddEvent(record);
            g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
            g_recentConfirmedDisenchantAtMs.store(0, std::memory_order_release);
            SKSE::log::info("SkyRewrite recorded confirmed disenchantment of '{}' and learned '{}' (count {} -> {})",
                record.subject, record.detail, countBefore, countAfter);
        } else if (hasCandidate) {
            SKSE::log::info("SkyRewrite ignored disenchant callback response {} for '{}'",
                button, record.subject);
        }
    }

    bool RecordDisenchantmentFromInventoryRemoval(const RE::TESContainerChangedEvent* event,
        RE::PlayerCharacter* player, bool hasRemovalContext, RE::ITEM_REMOVE_REASON removalReason)
    {
        constexpr RE::FormID kPlayerReferenceID = 0x14;
        if (!event || !player || event->oldContainer != kPlayerReferenceID || event->newContainer != 0 ||
            g_activeDisenchantItemFormID.load(std::memory_order_acquire) == event->baseObj ||
            (hasRemovalContext && removalReason != RE::ITEM_REMOVE_REASON::kRemove)) {
            return false;
        }

        auto* ui = RE::UI::GetSingleton();
        if (!ui || !ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME)) return false;
        auto menuHandle = ui->GetMenu<RE::CraftingMenu>();
        auto* craftingMenu = menuHandle.get();
        auto* submenu = craftingMenu ? craftingMenu->GetCraftingSubMenu() : nullptr;
        if (!submenu) return false;

        REL::Relocation<std::uintptr_t> enchantMenuVTable{ RE::VTABLE_CraftingSubMenus__EnchantConstructMenu[0] };
        if (*reinterpret_cast<const std::uintptr_t*>(submenu) != enchantMenuVTable.address()) return false;
        auto* enchantMenu = static_cast<RE::CraftingSubMenus::EnchantConstructMenu*>(submenu);
        if (enchantMenu->currentCategory.get() != RE::CraftingSubMenus::EnchantConstructMenu::Category::Disenchant) return false;

        auto* selected = CurrentDisenchantItem(enchantMenu);
        auto* selectedObject = selected && selected->data ? selected->data->GetObject() : nullptr;
        if (!selectedObject || selectedObject->GetFormID() != event->baseObj) return false;

        EventRecord record{};
        RE::TESBoundObject* disenchantedItem{};
        if (!BuildDisenchantmentRecord(selected, player, record, disenchantedItem)) return false;
        AddEvent(record);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
        SKSE::log::info("SkyRewrite recorded disenchantment from the selected item removal: '{}' and learned '{}'",
            record.subject, record.detail);
        return true;
    }

    bool InstallDisenchantCallbackHook()
    {
        if (g_disenchantCallbackHookInstalled) return true;
        REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_CraftingSubMenus__EnchantConstructMenu__EnchantMenuDisenchantCallback[0] };
        const auto original = vtable.write_vfunc(0x1, CaptureDisenchantCallbackRun);
        if (!original) return false;
        g_originalDisenchantRun = reinterpret_cast<EnchantDisenchantRunFunction>(original);
        g_disenchantCallbackHookInstalled = true;
        return true;
    }

    void CaptureAlchemyCraftCallbackRun(AlchemyCraftCallback* callback, std::uint8_t button)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (button == 0) {
            auto* submenu = callback ? callback->subMenu : nullptr;
            auto* result = submenu && submenu->resultPotionEntry ? submenu->resultPotionEntry->GetObject() : nullptr;
            const char* entryName = submenu && submenu->resultPotionEntry ? submenu->resultPotionEntry->GetDisplayName() : nullptr;
            SKSE::log::info("SkyRewrite alchemy craft callback invoked: result={}, resultPotion={}, entryName='{}', recipe='{}'",
                result ? result->GetFormID() : 0,
                submenu && submenu->resultPotion ? submenu->resultPotion->GetFormID() : 0,
                entryName ? entryName : "",
                submenu ? submenu->newRecipeFormatString.c_str() : "");
        }
        if (button == 0) {
            std::scoped_lock lock(g_recentAlchemyCraftNameMutex);
            g_recentAlchemyCraftName.fill('\0');
            g_recentAlchemyCraftNameAtMs = 0;
        }
        std::unordered_map<RE::FormID, std::int32_t> inventoryCountsBefore;
        RE::FormID callbackResultFormID{};
        if (callback && callback->subMenu) {
            if (auto* resultEntry = callback->subMenu->resultPotionEntry) {
                if (auto* resultObject = resultEntry->GetObject()) callbackResultFormID = resultObject->GetFormID();
            }
            if (!callbackResultFormID && callback->subMenu->resultPotion) {
                callbackResultFormID = callback->subMenu->resultPotion->GetFormID();
            }
        }
        if (button == 0 && player) {
            auto inventory = player->GetInventory();
            for (const auto& [boundItem, inventoryData] : inventory) {
                if (boundItem) inventoryCountsBefore[boundItem->GetFormID()] = inventoryData.first;
            }
        }
        const auto captureResultName = [callback, button] {
            if (button != 0 || !callback || !callback->subMenu) return;
            auto* resultPotion = callback->subMenu->resultPotion;
            const char* resultName = nullptr;
            if (auto* resultEntry = callback->subMenu->resultPotionEntry) {
                resultName = resultEntry->GetDisplayName();
            }
            if (IsPlaceholderAlchemyName(resultName)) {
                const auto* recipeName = callback->subMenu->newRecipeFormatString.c_str();
                if (!IsPlaceholderAlchemyName(recipeName) && !std::strchr(recipeName, '%') &&
                    !std::strchr(recipeName, '{') && !std::strchr(recipeName, '}')) {
                    resultName = recipeName;
                }
            }
            char generatedName[192]{};
            if (IsPlaceholderAlchemyName(resultName)) {
                resultName = AlchemyDisplayName(resultPotion, generatedName, sizeof(generatedName));
            }
            if (!IsPlaceholderAlchemyName(resultName)) {
                {
                    std::scoped_lock lock(g_recentAlchemyCraftNameMutex);
                    CopyText(g_recentAlchemyCraftName.data(), g_recentAlchemyCraftName.size(), resultName);
                    g_recentAlchemyCraftNameAtMs = SteadyClockMilliseconds();
                }
                UpdateLatestCraftedPotionName(0, resultName);
            }
        };

        captureResultName();
        if (g_originalAlchemyCraftRun) g_originalAlchemyCraftRun(callback, button);
        captureResultName();
        if (button == 0 && player) {
            auto inventory = player->GetInventory();
            const auto resolveInventoryOutput = [&](bool requireMenuResultMatch) {
                for (const auto& [boundItem, inventoryData] : inventory) {
                    if (!boundItem || !inventoryData.second) continue;
                    const auto formID = boundItem->GetFormID();
                    if (requireMenuResultMatch && formID != callbackResultFormID) continue;
                    const auto before = inventoryCountsBefore.find(formID);
                    if (inventoryData.first <= (before == inventoryCountsBefore.end() ? 0 : before->second)) continue;

                    const char* resultName = inventoryData.second->GetDisplayName();
                    char generatedName[192]{};
                    if (IsPlaceholderAlchemyName(resultName)) {
                        if (auto* alchemyItem = boundItem->As<RE::AlchemyItem>()) {
                            resultName = AlchemyDisplayName(alchemyItem, generatedName, sizeof(generatedName));
                        }
                    }
                    if (IsPlaceholderAlchemyName(resultName)) continue;
                    {
                        std::scoped_lock lock(g_recentAlchemyCraftNameMutex);
                        CopyText(g_recentAlchemyCraftName.data(), g_recentAlchemyCraftName.size(), resultName);
                        g_recentAlchemyCraftNameAtMs = SteadyClockMilliseconds();
                    }
                    UpdateLatestCraftedPotionName(0, resultName);
                    SKSE::log::info("SkyRewrite resolved crafted potion output as '{}' (FormID {:08X}, menuResultMatch={})",
                        resultName, formID, formID == callbackResultFormID);
                    return true;
                }
                return false;
            };
            if (!resolveInventoryOutput(callbackResultFormID != 0)) resolveInventoryOutput(false);
        }
    }

    bool InstallAlchemyCraftCallbackHook()
    {
        if (g_alchemyCraftCallbackHookInstalled) return true;
        REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_CraftingSubMenus__AlchemyMenu__CraftItemCallback[0] };
        const auto original = vtable.write_vfunc(0x1, CaptureAlchemyCraftCallbackRun);
        if (!original) return false;
        g_originalAlchemyCraftRun = reinterpret_cast<AlchemyCraftRunFunction>(original);
        g_alchemyCraftCallbackHookInstalled = true;
        return true;
    }

    void BeginBrawlTracking()
    {
        std::scoped_lock lock(g_brawlStateMutex);
        g_brawlOpponent = {};
        g_brawlActive = true;
    }

    void ResetBrawlTracking()
    {
        std::scoped_lock lock(g_brawlStateMutex);
        g_brawlOpponent = {};
        g_brawlActive = false;
    }

    bool FinishBrawlTracking(bool playerWon)
    {
        EventRecord result{};
        {
            std::scoped_lock lock(g_brawlStateMutex);
            if (!g_brawlActive) return false;
            result = g_brawlOpponent;
            g_brawlOpponent = {};
            g_brawlActive = false;
        }

        result.type = kEventBrawl;
        result.gameDay = CurrentGameDay();
        result.stage = playerWon ? 1 : 2;
        if (!result.subject[0]) CopyText(result.subject, sizeof(result.subject), "Unknown opponent");
        CopyText(result.detail, sizeof(result.detail), playerWon ? "Player won the brawl" : "Player lost the brawl");
        CopyText(result.identity, sizeof(result.identity), playerWon ? "SkyRewrite/Brawl/v1/won" : "SkyRewrite/Brawl/v1/lost");
        if (!result.location[0]) AddLocation(result, RE::PlayerCharacter::GetSingleton());
        AddEvent(result);
        g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    void RememberBrawlOpponent(RE::Actor* actor)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!actor || actor == player) return;

        EventRecord opponent{};
        opponent.formID = actor->GetFormID();
        CopyFormPlugin(opponent.formPlugin, sizeof(opponent.formPlugin), actor);
        if (auto* base = actor->GetActorBase()) {
            opponent.baseFormID = base->GetFormID();
            CopyFormPlugin(opponent.basePlugin, sizeof(opponent.basePlugin), base);
        }
        const auto* name = ReferenceDisplayName(actor);
        CopyText(opponent.subject, sizeof(opponent.subject), name && *name ? name : "Unknown opponent");
        AddLocation(opponent, player);

        std::scoped_lock lock(g_brawlStateMutex);
        if (g_brawlActive) g_brawlOpponent = opponent;
    }

    class SkyRewriteEventSink final :
        public RE::BSTEventSink<RE::TESActivateEvent>,
        public RE::BSTEventSink<RE::TESDeathEvent>,
        public RE::BSTEventSink<RE::TESEnterBleedoutEvent>,
        public RE::BSTEventSink<RE::TESQuestStageEvent>,
        public RE::BSTEventSink<RE::TESQuestStartStopEvent>,
        public RE::BSTEventSink<RE::TESLockChangedEvent>,
        public RE::BSTEventSink<RE::TESTopicInfoEvent>,
        public RE::BSTEventSink<RE::TESActorLocationChangeEvent>,
        public RE::BSTEventSink<RE::TESContainerChangedEvent>,
        public RE::BSTEventSink<RE::TESEquipEvent>,
        public RE::BSTEventSink<RE::TESLoadGameEvent>,
        public RE::BSTEventSink<RE::BooksRead::Event>,
        public RE::BSTEventSink<RE::SkillIncrease::Event>,
        public RE::BSTEventSink<RE::LevelIncrease::Event>,
        public RE::BSTEventSink<RE::SpellsLearned::Event>,
        public RE::BSTEventSink<RE::LocationDiscovery::Event>,
        public RE::BSTEventSink<RE::TESFurnitureEvent>,
        public RE::BSTEventSink<RE::ItemCrafted::Event>,
        public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (event && event->menuName == RE::BarterMenu::MENU_NAME) {
                g_barterMenuOpen.store(event->opening, std::memory_order_release);
                if (!event->opening) {
                    RememberActiveBarterVendor();
                    g_lastBarterMenuCloseMs.store(SteadyClockMilliseconds(), std::memory_order_release);
                } else {
                    RememberActiveBarterVendor();
                }
            }
            if (event && event->menuName == RE::LockpickingMenu::MENU_NAME) {
                const auto now = SteadyClockMilliseconds();
                if (event->opening) {
                    auto target = RE::LockpickingMenu::GetTargetReference();
                    g_activeLockpickTarget.store(target ? target->GetFormID() : 0, std::memory_order_release);
                    g_lockpickMenuOpenedAtMs.store(now, std::memory_order_release);
                    g_lockpickMenuClosedAtMs.store(0, std::memory_order_release);
                } else {
                    g_lockpickMenuClosedAtMs.store(now, std::memory_order_release);
                }
            }
            if (event && event->menuName == RE::StatsMenu::MENU_NAME) {
                CaptureNewPlayerShoutsAndPerks(RE::PlayerCharacter::GetSingleton());
            }
            if (event && event->menuName == RE::CraftingMenu::MENU_NAME && event->opening) {
                if (!g_disenchantCallbackHookInstalled && InstallDisenchantCallbackHook()) {
                    SKSE::log::info("SkyRewrite installed the disenchant callback hook when the enchanting menu opened");
                }
                if (!g_alchemyCraftCallbackHookInstalled && InstallAlchemyCraftCallbackHook()) {
                    SKSE::log::info("SkyRewrite installed the alchemy callback hook when the crafting menu opened");
                }
            }
            if (event && event->menuName == RE::CraftingMenu::MENU_NAME && !event->opening) {
                ScheduleAllIngredientEffectCapture();
                DiscardUnmatchedCraftMaterials();
                g_activeCraftingWorkbench.store(0, std::memory_order_release);
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* event,
            RE::BSTEventSource<RE::TESFurnitureEvent>*) override
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* actor = event ? event->actor.get() : nullptr;
            auto* furniture = event ? event->targetFurniture.get() : nullptr;
            if (!player || actor != player || !furniture) return RE::BSEventNotifyControl::kContinue;

            if (event->type.get() == RE::TESFurnitureEvent::FurnitureEventType::kEnter) {
                g_activeCraftingWorkbench.store(furniture->GetFormID(), std::memory_order_release);
            } else if (event->type.get() == RE::TESFurnitureEvent::FurnitureEventType::kExit) {
                auto expected = furniture->GetFormID();
                g_activeCraftingWorkbench.compare_exchange_strong(expected, 0, std::memory_order_acq_rel);
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::ItemCrafted::Event* event,
            RE::BSTEventSource<RE::ItemCrafted::Event>*) override
        {
            if (RecordCraftingAction(event, RE::PlayerCharacter::GetSingleton())) {
                g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent*, RE::BSTEventSource<RE::TESLoadGameEvent>*) override
        {
            g_activeCraftingWorkbench.store(0, std::memory_order_release);
            g_recentStorageTargetID.store(0, std::memory_order_release);
            g_recentStorageTargetAtMs.store(0, std::memory_order_release);
            ResetBrawlTracking();
            DiscardUnmatchedCraftMaterials();
            {
                std::scoped_lock lock(g_playerDialogueMutex);
                g_playerDialogueSelections.clear();
            }
            {
                std::scoped_lock lock(g_unlockSnapshotMutex);
                g_knownPlayerShouts.clear();
                g_knownPlayerPerks.clear();
                g_unlockSnapshotInitialized = false;
            }
            {
                std::scoped_lock lock(g_skillSnapshotMutex);
                g_playerSkillValues.clear();
                g_lastPlayerLevel = 0;
            }
            CaptureNewPlayerShoutsAndPerks(RE::PlayerCharacter::GetSingleton());
            SnapshotKnownIngredientEffects();
            SnapshotLoadedFollowerStates();
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESEnterBleedoutEvent* event,
            RE::BSTEventSource<RE::TESEnterBleedoutEvent>*) override
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* reference = event ? event->actor.get() : nullptr;
            auto* actor = reference ? reference->As<RE::Actor>() : nullptr;
            if (!player || !actor) return RE::BSEventNotifyControl::kContinue;

            if (actor == player) FinishBrawlTracking(false);
            else RememberBrawlOpponent(actor);
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) override
        {
            RecordEquipmentChange(event, RE::PlayerCharacter::GetSingleton());
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::BooksRead::Event* event,
            RE::BSTEventSource<RE::BooksRead::Event>*) override
        {
            RecordBookRead(event, RE::PlayerCharacter::GetSingleton());
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::SkillIncrease::Event* event,
            RE::BSTEventSource<RE::SkillIncrease::Event>*) override
        {
            SchedulePlayerSkillIncrease(event);
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::LevelIncrease::Event* event,
            RE::BSTEventSource<RE::LevelIncrease::Event>*) override
        {
            RecordPlayerLevelIncrease(event, RE::PlayerCharacter::GetSingleton());
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::SpellsLearned::Event* event,
            RE::BSTEventSource<RE::SpellsLearned::Event>*) override
        {
            if (!event || !event->spell) return RE::BSEventNotifyControl::kContinue;
            auto* player = RE::PlayerCharacter::GetSingleton();
            RecordPlayerSpellLearned(event->spell, player);
            CaptureNewPlayerShoutsAndPerks(player);
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::LocationDiscovery::Event* event,
            RE::BSTEventSource<RE::LocationDiscovery::Event>*) override
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                RecordDiscoveredMapMarker(event, player);
                CaptureNewPlayerShoutsAndPerks(player);
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event, RE::BSTEventSource<RE::TESActivateEvent>*) override
        {
            RememberActivatedTheftTarget(event);
            RememberActivatedStorageTarget(event);
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* activator = event ? event->actionRef.get() : nullptr;
            auto* target = event ? event->objectActivated.get() : nullptr;
            if (player && activator == player && target && IsBusinessLedger(target)) {
                if (auto* quest = FindRunningNumbersJob()) RecordNumbersJobForgery(quest, target, player);
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*) override
        {
            if (!event) return RE::BSEventNotifyControl::kContinue;
            g_deathEventsSeenThisSession.fetch_add(1, std::memory_order_relaxed);
            if (!event->dead) return RE::BSEventNotifyControl::kContinue;
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* dying = event->actorDying.get();
            auto* killer = event->actorKiller.get();
            if (!player || (dying != player && killer != player)) return RE::BSEventNotifyControl::kContinue;
            EventRecord record{};
            record.type = 1;
            record.gameDay = CurrentGameDay();
            AddLocation(record, player);
            if (dying) {
                record.formID = dying->GetFormID();
                CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), dying);
                CopyText(record.identity, sizeof(record.identity), dying->GetFormEditorID());
            }
            if (auto* actor = dying ? dying->As<RE::Actor>() : nullptr) {
                if (auto* base = actor->GetActorBase()) {
                    record.baseFormID = base->GetFormID();
                    CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), base);
                    if (base->GetFormEditorID() && *base->GetFormEditorID()) CopyText(record.identity, sizeof(record.identity), base->GetFormEditorID());
                }
            }
            CopyText(record.subject, sizeof(record.subject), dying ? dying->GetName() : "Unknown actor");
            if (killer == player) {
                CopyText(record.detail, sizeof(record.detail), "Player killed this actor");
                g_playerKillsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
            }
            else CopyText(record.detail, sizeof(record.detail), killer && killer->GetName() ? killer->GetName() : "Player died");
            // Preserve the victim's reaction context in the existing save-safe
            // stage field (unused by death records). Older records remain 0 and
            // are intentionally reported as unknown rather than guessed.
            if (killer == player) {
                if (auto* actor = dying ? dying->As<RE::Actor>() : nullptr) {
                    record.stage |= kDispositionKnown;
                    const bool hostile = actor->IsHostileToActor(player);
                    if (hostile) record.stage |= kVictimHostile | kVictimAllHostile;
                    if (actor->IsGuard()) record.stage |= kVictimGuard;
                    if (actor->IsEssential()) record.stage |= kVictimEssential;
                    if (actor->IsPlayerTeammate()) record.stage |= kVictimTeammate;
                    auto* base = actor->GetActorBase();
                    if (!hostile && !actor->IsPlayerTeammate() && actor->IsHumanoid()) record.stage |= kVictimCivilian;
                    const auto inferred = InferVictimCategories(record.subject, base && base->GetFormEditorID() ? base->GetFormEditorID() : "");
                    record.stage |= inferred;
                    if (base) {
                        if (base->HasKeywordString("ActorTypeBandit")) record.stage |= kVictimBandit;
                        if (base->HasKeywordString("ActorTypeForsworn")) record.stage |= kVictimForsworn;
                        if (base->HasKeywordString("ActorTypeVampire")) record.stage |= kVictimVampire;
                    }
                    if (dying->IsDragon()) record.stage |= kVictimDragon | kVictimMonster;
                    if (hostile && (!actor->IsHumanoid() || (record.stage & (kVictimDragon | kVictimWolf | kVictimBear | kVictimSpider)))) {
                        record.stage |= kVictimMonster;
                    }
                    if (hostile && !(record.stage & (kVictimBandit | kVictimDragon | kVictimForsworn | kVictimVampire | kVictimWolf | kVictimBear | kVictimSpider | kVictimMonster))) {
                        record.stage |= kVictimOtherHostile;
                    }
                }
            }
            if (killer == player && (record.stage & kVictimBandit)) {
                UpdateRegionalBanditState(record, player);
                UpdateRegionalBanditHoldKeyState(record, player);
            }
            if (killer == player) UpdateRegionalCreatureStates(record, player);
            AddEvent(record);
            g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
            if (killer == player && (record.stage & kVictimBandit)) {
                ImproveLocalMerchantRelationships(record.locationFormID);
            }
            if (killer == player && (record.stage & kVictimDragon)) {
                ImproveLocalHoldRelationships(player, LookupLocation(record.locationFormID));
            }
            if (killer == player) {
                ApplyMurderTierResponses(player);
                ApplyBanditTradeResponse();
                constexpr std::uint16_t kPositiveReputationVictim = kVictimBandit | kVictimForsworn | kVictimVampire | kVictimMonster;
                if (record.stage & kPositiveReputationVictim) AwardReputationGiftIfEarned(player);
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESQuestStageEvent* event, RE::BSTEventSource<RE::TESQuestStageEvent>*) override
        {
            if (!event || !event->formID) return RE::BSEventNotifyControl::kContinue;
            if (event->formID == kVanillaBrawlQuestFormID) {
                if (event->stage == 10) BeginBrawlTracking();
                else if (event->stage == 15) FinishBrawlTracking(true);
                return RE::BSEventNotifyControl::kContinue;
            }
            auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(event->formID);
            if (!IsRelevantJournalQuest(quest)) return RE::BSEventNotifyControl::kContinue;
            EventRecord record{};
            record.type = 2;
            record.gameDay = CurrentGameDay();
            record.formID = event->formID;
            record.stage = event->stage;
            CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), quest);
            AddLocation(record, RE::PlayerCharacter::GetSingleton());
            const auto* name = quest ? quest->GetName() : nullptr;
            const auto* editorID = quest ? quest->GetFormEditorID() : nullptr;
            if (!name || !*name) name = editorID;
            if (name && *name) CopyText(record.subject, sizeof(record.subject), name);
            else std::snprintf(record.subject, sizeof(record.subject), "Quest FormID %08X", event->formID);
            CopyText(record.identity, sizeof(record.identity), editorID);
            std::snprintf(record.detail, sizeof(record.detail), "Quest stage %u reached", static_cast<unsigned>(event->stage));
            AddEvent(record);
            if (quest && quest->IsCompleted()) {
                RecordQuestCompletion(quest, RE::PlayerCharacter::GetSingleton());
            }
            CaptureNewPlayerShoutsAndPerks(RE::PlayerCharacter::GetSingleton());
            g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
            if (RecordFavorQuestCompletion(quest, RE::PlayerCharacter::GetSingleton())) AwardReputationGiftIfEarned(RE::PlayerCharacter::GetSingleton());
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESQuestStartStopEvent* event, RE::BSTEventSource<RE::TESQuestStartStopEvent>*) override
        {
            if (!event || !event->formID) return RE::BSEventNotifyControl::kContinue;
            if (event->formID == kVanillaBrawlQuestFormID) {
                if (event->started) BeginBrawlTracking();
                else ResetBrawlTracking();
                return RE::BSEventNotifyControl::kContinue;
            }
            auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(event->formID);
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (event->started) {
                if (IsRelevantJournalQuest(quest)) {
                    RecordQuestStart(event, quest, player);
                    g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
                }
            } else if (IsRelevantJournalQuest(quest) && quest->IsCompleted()) {
                RecordQuestCompletion(quest, player);
            }
            CaptureNewPlayerShoutsAndPerks(player);
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESLockChangedEvent* event, RE::BSTEventSource<RE::TESLockChangedEvent>*) override
        {
            auto* target = event ? event->lockedObject.get() : nullptr;
            if (!target || target->IsLocked()) return RE::BSEventNotifyControl::kContinue;
            const auto targetID = g_activeLockpickTarget.load(std::memory_order_acquire);
            if (!targetID || targetID != target->GetFormID()) return RE::BSEventNotifyControl::kContinue;
            const auto now = SteadyClockMilliseconds();
            const auto openedAt = g_lockpickMenuOpenedAtMs.load(std::memory_order_acquire);
            const auto closedAt = g_lockpickMenuClosedAtMs.load(std::memory_order_acquire);
            const auto referenceTime = closedAt ? closedAt : openedAt;
            if (!referenceTime || now < referenceTime || now - referenceTime > 10000) return RE::BSEventNotifyControl::kContinue;
            g_activeLockpickTarget.store(0, std::memory_order_release);
            RecordLockpick(target, RE::PlayerCharacter::GetSingleton());
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESTopicInfoEvent* event, RE::BSTEventSource<RE::TESTopicInfoEvent>*) override
        {
            RecordDialogueAction(event, RE::PlayerCharacter::GetSingleton());
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESActorLocationChangeEvent* event, RE::BSTEventSource<RE::TESActorLocationChangeEvent>*) override
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* changedActor = event ? event->actor.get() : nullptr;
            if (!event || !player || !changedActor) return RE::BSEventNotifyControl::kContinue;
            if (changedActor != player) {
                if (auto* npc = changedActor->As<RE::Actor>()) RecordFollowerState(npc, player);
                return RE::BSEventNotifyControl::kContinue;
            }
            CaptureNewPlayerShoutsAndPerks(player);
            auto* location = event->newLoc ? event->newLoc : event->oldLoc;
            EventRecord record{};
            record.type = 3;
            record.gameDay = CurrentGameDay();
            if (location) {
                record.formID = location->GetFormID();
                record.locationFormID = location->GetFormID();
                CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), location);
                CopyFormPlugin(record.locationPlugin, sizeof(record.locationPlugin), location);
                const auto* name = location->GetName();
                const auto* editorID = location->GetFormEditorID();
                if (!name || !*name) name = editorID;
                CopyText(record.subject, sizeof(record.subject), name && *name ? name : "Unknown location");
                CopyText(record.identity, sizeof(record.identity), editorID);
                CopyText(record.location, sizeof(record.location), record.subject);
                CopyText(record.detail, sizeof(record.detail), event->newLoc ? "Entered location" : "Left location");
            } else {
                CopyText(record.detail, sizeof(record.detail), "Location changed");
            }
            AddEvent(record);
            g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
            // The configured-duration hold bounty is removed only when the
            // hold's current nonviolent balance still matches the applied amount.
            ExpireTemporaryBounties(player, CurrentGameDay());
            ExpireAdaptiveGuardResponses(CurrentGameDay());
            if (event->newLoc) RetryGuardDispatchForActiveBounty(player, event->newLoc, CurrentGameDay());
            // Recheck accumulated thefts on entry as well as at the inventory
            // event. This covers settings synchronization and location-form
            // timing without letting a Local Echo bounty take precedence.
            if (event->newLoc) AttemptTheftBountyResponse(player, event->newLoc->GetFormID(), event->newLoc, CurrentGameDay());

            // Reapply save-derived responses when actors in a hold load, so
            // vendors and local officials can react even if they were not
            // loaded at the time of the kill.
            if (event->newLoc) {
                ApplyMurderTierResponses(player);
                ImproveNearbyReputationRelationships(player, ReputationScore());
                ApplyBanditTradeResponse();
                ImproveLocalMerchantRelationships(event->newLoc->GetFormID());
                ImproveLocalHoldRelationships(player, event->newLoc);
                AttemptRegionalShopRobbery(player, event->newLoc);
            }

            // Optional, informational response: only emit a journal entry when
            // this save already records repeated player kills at the location.
            if (g_localEchoEnabled.load(std::memory_order_relaxed) && event->newLoc) {
                const auto lookbackDays = g_localEchoLookbackDays.load(std::memory_order_relaxed);
                const auto cooldownDays = g_localEchoCooldownDays.load(std::memory_order_relaxed);
                const auto killThreshold = g_localEchoKillThreshold.load(std::memory_order_relaxed);
                const auto now = CurrentGameDay();
                const auto locationID = event->newLoc->GetFormID();
                std::size_t recentKills = 0;
                std::size_t recentThefts = 0;
                std::size_t recentHostileKills = 0;
                std::size_t recentNonHostileKills = 0;
                float recentPressure = 0.0F;
                bool recentlyEchoed = false;
                float lastEchoDay = -1.0F;
                {
                    std::scoped_lock lock(g_eventsMutex);
                    for (const auto& prior : g_events) {
                        if (prior.locationFormID != locationID || prior.gameDay < 0.0F || now < prior.gameDay) continue;
                        if (prior.type == 1 && std::string_view(prior.detail) == "Player killed this actor") {
                            const auto age = now - prior.gameDay;
                            if (age < lookbackDays) {
                                ++recentKills;
                                recentPressure += std::max(0.0F, 1.0F - age / lookbackDays);
                                if (prior.stage & kDispositionKnown) {
                                    if (prior.stage & kVictimHostile) ++recentHostileKills;
                                    else ++recentNonHostileKills;
                                }
                            }
                        }
                        if (prior.type == 10 && prior.gameDay >= 0.0F && now >= prior.gameDay && now - prior.gameDay < lookbackDays) {
                            ++recentThefts;
                            recentPressure += std::max(0.0F, 1.0F - (now - prior.gameDay) / lookbackDays);
                        }
                        if (prior.type == 5 && prior.locationFormID == locationID && prior.gameDay >= 0.0F && now >= prior.gameDay && now - prior.gameDay < cooldownDays) {
                            recentlyEchoed = true;
                            lastEchoDay = std::max(lastEchoDay, prior.gameDay);
                        }
                    }
                }
                if (recentPressure >= static_cast<float>(killThreshold) && !recentlyEchoed) {
                    const auto recentSignals = recentKills + recentThefts;
                    EventRecord echo{};
                    echo.type = 5;
                    echo.gameDay = now;
                    echo.formID = locationID;
                    echo.locationFormID = locationID;
                    echo.stage = static_cast<std::uint16_t>(std::min<std::size_t>(recentSignals, std::numeric_limits<std::uint16_t>::max()));
                    CopyFormPlugin(echo.formPlugin, sizeof(echo.formPlugin), event->newLoc);
                    CopyFormPlugin(echo.locationPlugin, sizeof(echo.locationPlugin), event->newLoc);
                    CopyText(echo.subject, sizeof(echo.subject), record.subject);
                    CopyText(echo.location, sizeof(echo.location), record.location);
                    // Keep the causal counts parseable and leave room in the
                    // fixed-size save field for the bounty outcome below.
                    std::snprintf(echo.detail, sizeof(echo.detail), "Local Echo rule fired: pressure %.2f (threshold %u) from %zu kills and %zu thefts", recentPressure, killThreshold, recentKills, recentThefts);
                    CopyText(echo.identity, sizeof(echo.identity), "LocalEcho/PressureThreshold/v1");
                    char bountyStatus[128]{};
                    if (g_temporaryBountyEnabled.load(std::memory_order_relaxed)) {
                        const auto configuredBounty = g_temporaryBountyAmount.load(std::memory_order_relaxed);
                        const auto pressureScaling = g_temporaryBountyPressureScaling.load(std::memory_order_relaxed);
                        auto kTemporaryBounty = configuredBounty;
                        if (pressureScaling) {
                            const auto scaledAmount = std::clamp(static_cast<float>(configuredBounty) * (recentPressure / static_cast<float>(killThreshold)), 5.0F, 100.0F);
                            kTemporaryBounty = static_cast<std::uint32_t>(std::clamp(std::lround(scaledAmount / 5.0F) * 5L, 5L, 100L));
                        }
                        const auto temporaryBountyDays = g_temporaryBountyDuration.load(std::memory_order_relaxed);
                        if (BountyVictimModeAllows(recentKills, recentHostileKills, recentNonHostileKills)) {
                            const auto result = ApplyTemporaryHoldBounty(player, event->newLoc, now, kTemporaryBounty, temporaryBountyDays, "Local Echo", static_cast<std::uint32_t>(std::min(recentSignals, static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))));
                            if (result == BountyApplyResult::applied || result == BountyApplyResult::appliedWithoutGuard) {
                            std::snprintf(bountyStatus, sizeof(bountyStatus), " Temporary hold bounty applied: +%u septims.", kTemporaryBounty);
                            const auto used = std::strlen(echo.detail);
                            if (used < sizeof(echo.detail) - 1) std::snprintf(echo.detail + used, sizeof(echo.detail) - used, "; bounty +%u septims", kTemporaryBounty);
                            if (result == BountyApplyResult::appliedWithoutGuard && g_dispatchGuardOnBounty.load(std::memory_order_relaxed)) {
                                const auto statusLength = std::strlen(bountyStatus);
                                std::snprintf(bountyStatus + statusLength, sizeof(bountyStatus) - statusLength, " Hold guard could not be dispatched.");
                            } else if (result == BountyApplyResult::appliedWithoutGuard) {
                                const auto statusLength = std::strlen(bountyStatus);
                                std::snprintf(bountyStatus + statusLength, sizeof(bountyStatus) - statusLength, " Hold guard dispatch is disabled.");
                            }
                        } else {
                            const char* reason = result == BountyApplyResult::noCrimeFaction ? "; bounty skipped: no hold crime faction" : "; bounty skipped: balance unchanged";
                            std::snprintf(bountyStatus, sizeof(bountyStatus), "%s", result == BountyApplyResult::noCrimeFaction ? " Bounty skipped: no hold crime faction was found." : " Bounty skipped: Skyrim did not change the bounty balance.");
                            const auto used = std::strlen(echo.detail);
                            if (used < sizeof(echo.detail) - 1) std::snprintf(echo.detail + used, sizeof(echo.detail) - used, "%s", reason);
                        }
                        } else {
                            const auto mode = g_bountyVictimMode.load(std::memory_order_relaxed);
                            const char* reason = mode == 2 ? "no kills" : mode == 1 ? "no confirmed hostile kills" : "no confirmed non-hostile kills";
                            std::snprintf(bountyStatus, sizeof(bountyStatus), " Bounty skipped: %s in the lookback window.", reason);
                            const auto used = std::strlen(echo.detail);
                            if (used < sizeof(echo.detail) - 1) std::snprintf(echo.detail + used, sizeof(echo.detail) - used, "; bounty skipped: %s", reason);
                        }
                    }
                    std::uint32_t adaptedGuards = 0;
                    if (g_adaptiveGuardResponseEnabled.load(std::memory_order_relaxed) && recentHostileKills >= killThreshold && recentNonHostileKills == 0) {
                        // A one-day, reversible response: local guards become
                        // one confidence step braver after exclusively hostile
                        // losses. Mixed or non-hostile kills never trigger it.
                        adaptedGuards = ApplyAdaptiveGuardResponse(event->newLoc, now, 1);
                        if (adaptedGuards > 0) std::snprintf(bountyStatus + std::strlen(bountyStatus), sizeof(bountyStatus) - std::strlen(bountyStatus), " Adaptive response: %u local guard%s changed.", adaptedGuards, adaptedGuards == 1 ? "" : "s");
                        else std::snprintf(bountyStatus + std::strlen(bountyStatus), sizeof(bountyStatus) - std::strlen(bountyStatus), " Adaptive response fired, but no eligible loaded guards were found here.");
                    }
                    AddEvent(echo);
                    if (g_localEchoNotificationEnabled.load(std::memory_order_relaxed)) {
                        char message[192]{};
                        const auto* locationName = event->newLoc->GetName();
                        if (!locationName || !*locationName) locationName = event->newLoc->GetFormEditorID();
                        if (!locationName || !*locationName) locationName = "this area";
                        std::snprintf(message, sizeof(message), "SkyRewrite: Local pressure %.1f/%.1f near %.45s (%zu kills, %zu thefts).%s", recentPressure, static_cast<double>(killThreshold), locationName, recentKills, recentThefts, bountyStatus);
                        RE::SendHUDMessage::ShowHUDMessage(message, nullptr, true);
                    }
                }
                else if (recentPressure >= static_cast<float>(killThreshold) && recentlyEchoed) {
                    char cooldownIdentity[96]{};
                    std::snprintf(cooldownIdentity, sizeof(cooldownIdentity), "SkyRewrite/LocalEchoCooldown/v1/%08X/%.3f", locationID, static_cast<double>(lastEchoDay));
                    bool noticeAlreadyRecorded = false;
                    {
                        std::scoped_lock lock(g_eventsMutex);
                        noticeAlreadyRecorded = std::any_of(g_events.begin(), g_events.end(), [&](const EventRecord& prior) {
                            return prior.type == 12 && std::string_view(prior.identity) == cooldownIdentity;
                        });
                    }
                    if (!noticeAlreadyRecorded) {
                        EventRecord cooldown{};
                        cooldown.type = 12;
                        cooldown.gameDay = now;
                        cooldown.formID = locationID;
                        cooldown.locationFormID = locationID;
                        cooldown.stage = static_cast<std::uint16_t>(std::min<std::size_t>(recentKills + recentThefts, std::numeric_limits<std::uint16_t>::max()));
                        CopyFormPlugin(cooldown.formPlugin, sizeof(cooldown.formPlugin), event->newLoc);
                        CopyFormPlugin(cooldown.locationPlugin, sizeof(cooldown.locationPlugin), event->newLoc);
                        CopyText(cooldown.subject, sizeof(cooldown.subject), record.subject);
                        CopyText(cooldown.location, sizeof(cooldown.location), record.location);
                        CopyText(cooldown.identity, sizeof(cooldown.identity), cooldownIdentity);
                        const auto remaining = std::max(0.0F, cooldownDays - (now - lastEchoDay));
                        std::snprintf(cooldown.detail, sizeof(cooldown.detail), "Local Echo not repeated: pressure %.2f meets threshold %u, but this location's cooldown has %.2f in-game days remaining", recentPressure, killThreshold, static_cast<double>(remaining));
                        AddEvent(cooldown);
                    }
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
        {
            constexpr RE::FormID kPlayerReferenceID = 0x14;
            if (!event || !event->baseObj || event->itemCount == 0) return RE::BSEventNotifyControl::kContinue;
            auto* item = RE::TESForm::LookupByID(event->baseObj);
            const bool goldItem = IsGoldForm(item) || event->baseObj == 0x0000000F;
            const bool gained = event->newContainer == kPlayerReferenceID && event->oldContainer != kPlayerReferenceID;
            const bool lost = event->oldContainer == kPlayerReferenceID && event->newContainer != kPlayerReferenceID;
            const auto confirmedDisenchantAt = g_recentConfirmedDisenchantAtMs.load(std::memory_order_acquire);
            const auto nowMs = SteadyClockMilliseconds();
            const bool recentConfirmedDisenchantRemoval = event->oldContainer == kPlayerReferenceID &&
                event->newContainer == 0 && confirmedDisenchantAt > 0 && nowMs >= confirmedDisenchantAt &&
                nowMs - confirmedDisenchantAt <= 2500;
            if (lost && (g_activeDisenchantCallback.load(std::memory_order_acquire) ||
                recentConfirmedDisenchantRemoval)) {
                const auto activeItemID = g_activeDisenchantItemFormID.load(std::memory_order_acquire);
                if (activeItemID) return RE::BSEventNotifyControl::kContinue;
                if (RecordDisenchantmentDuringConfirmedCallback(event, RE::PlayerCharacter::GetSingleton())) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                // A removal inside the confirmed callback must not be
                // misclassified as an ordinary crafting material.
                return RE::BSEventNotifyControl::kContinue;
            }
            if (lost && g_activeDisenchantItemFormID.load(std::memory_order_acquire) == event->baseObj) {
                return RE::BSEventNotifyControl::kContinue;
            }
            auto transferredReference = event->reference.get();
            if (goldItem) {
                SKSE::log::info("SkyRewrite gold container event: formID={:08X}, name='{}', count={}, oldContainer={:08X}, newContainer={:08X}, playerGain={}, playerLoss={}, reference={:08X}",
                    event->baseObj, item && item->GetName() ? item->GetName() : "Gold", event->itemCount,
                    event->oldContainer, event->newContainer, gained, lost,
                    transferredReference ? transferredReference->GetFormID() : 0);
            }
            if (!gained && !lost) return RE::BSEventNotifyControl::kContinue;
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return RE::BSEventNotifyControl::kContinue;
            auto* alchemyItem = item ? item->As<RE::AlchemyItem>() : nullptr;
            auto* ui = RE::UI::GetSingleton();
            if (gained && event->oldContainer == 0 && ui && ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME)) {
                // Skyrim reports a newly-created smithing/alchemy/enchanting
                // result as a loose pickup. Its ItemCrafted event records the
                // more useful action; do not add a second generic pickup row.
                // Keep the inventory-name capture here before returning: the
                // dynamic potion name lives on its inventory entry, while the
                // ItemCrafted event can expose only the generic base form.
                auto* workbench = InventoryContainerReference(g_activeCraftingWorkbench.load(std::memory_order_acquire));
                auto* workbenchBase = workbench ? workbench->GetObjectReference() : nullptr;
                auto* furniture = workbenchBase ? workbenchBase->As<RE::TESFurniture>() : nullptr;
                const auto benchType = furniture ? furniture->workBenchData.benchType.get() :
                    RE::TESFurniture::WorkBenchData::BenchType::kNone;
                const bool potionOutput = alchemyItem ||
                    benchType == RE::TESFurniture::WorkBenchData::BenchType::kAlchemy ||
                    benchType == RE::TESFurniture::WorkBenchData::BenchType::kAlchemyExperiment;
                if (potionOutput) {
                    auto* boundPotion = item ? item->As<RE::TESBoundObject>() : nullptr;
                    const char* displayName = nullptr;
                    if (boundPotion) {
                        auto inventory = player->GetInventory();
                        const auto inventoryEntry = inventory.find(boundPotion);
                        if (inventoryEntry != inventory.end() && inventoryEntry->second.second) {
                            displayName = inventoryEntry->second.second->GetDisplayName();
                        }
                    }
                    SKSE::log::info("SkyRewrite alchemy output inventory event: formID={:08X}, type={}, name='{}', entry={}",
                        event->baseObj, item ? static_cast<std::uint32_t>(item->GetFormType()) : 0,
                        displayName ? displayName : "", boundPotion != nullptr);
                    if (!IsPlaceholderAlchemyName(displayName)) {
                        RememberRecentAlchemyCraftName(displayName);
                        UpdateLatestCraftedPotionName(0, displayName);
                    }
                    if (boundPotion) ScheduleCraftedPotionInventoryNameCapture(event->baseObj);
                }
                return RE::BSEventNotifyControl::kContinue;
            }
            auto* ingredientItem = item ? item->As<RE::IngredientItem>() : nullptr;
            // Container-change notifications may be queued after RemoveItem
            // returns, so check both the synchronous context and its short-lived
            // form/count-matched snapshot. Equip events provide a second signal
            // for Skyrim's use-item path when a mod bypasses RemoveItem.
            bool hasRemovalContext = g_playerItemRemovalContext.actor == player &&
                g_playerItemRemovalContext.baseObj == event->baseObj;
            auto removalReason = hasRemovalContext ? g_playerItemRemovalContext.reason : RE::ITEM_REMOVE_REASON::kRemove;
            if (!hasRemovalContext && lost && event->newContainer == 0) {
                hasRemovalContext = TakeRecentPlayerItemRemovalContext(event->baseObj, event->itemCount, removalReason);
            }
            const bool isConsumable = (alchemyItem && !alchemyItem->IsPoison()) || ingredientItem;
            const bool recentConsumableUse = lost && event->newContainer == 0 && isConsumable &&
                ConsumeRecentConsumableUse(event->baseObj);
            const auto* spellTome = item ? item->As<RE::TESObjectBOOK>() : nullptr;
            const bool consumedSpellTome = lost && event->newContainer == 0 && hasRemovalContext &&
                removalReason == RE::ITEM_REMOVE_REASON::kRemove &&
                spellTome && spellTome->TeachesSpell();
            if (consumedSpellTome) return RE::BSEventNotifyControl::kContinue;
            const bool consumed = lost && event->newContainer == 0 && isConsumable &&
                (recentConsumableUse || (hasRemovalContext && removalReason == RE::ITEM_REMOVE_REASON::kRemove));
            if (lost && event->newContainer == 0 &&
                RecordDisenchantmentFromInventoryRemoval(event, player, hasRemovalContext, removalReason)) {
                return RE::BSEventNotifyControl::kContinue;
            }
            if (QueueCraftMaterialRemoval(event, player, item, hasRemovalContext, removalReason)) {
                return RE::BSEventNotifyControl::kContinue;
            }
            if (gained && event->newContainer == kPlayerReferenceID) {
                const auto craftAtMs = g_recentCraftedPotionEventAtMs.load(std::memory_order_acquire);
                const auto craftedFormID = g_recentCraftedPotionFormID.load(std::memory_order_acquire);
                const auto nowMs = SteadyClockMilliseconds();
                if (craftAtMs && nowMs >= craftAtMs && nowMs - craftAtMs <= 5000 &&
                    (event->baseObj == craftedFormID || alchemyItem)) {
                    auto* boundPotion = item->As<RE::TESBoundObject>();
                    const char* displayName = nullptr;
                    if (boundPotion) {
                        auto inventory = player->GetInventory();
                        const auto inventoryEntry = inventory.find(boundPotion);
                        if (inventoryEntry != inventory.end() && inventoryEntry->second.second) {
                            displayName = inventoryEntry->second.second->GetDisplayName();
                        }
                    }
                    char generatedName[192]{};
                    if (IsPlaceholderAlchemyName(displayName)) {
                        displayName = AlchemyDisplayName(alchemyItem, generatedName, sizeof(generatedName));
                    }
                    if (displayName && !IsPlaceholderAlchemyName(displayName)) {
                        RememberRecentAlchemyCraftName(displayName);
                        UpdateLatestCraftedPotionName(0, displayName);
                    }
                    ScheduleCraftedPotionInventoryNameCapture(event->baseObj);
                }
            }
            auto* sourceForm = event->oldContainer ? RE::TESForm::LookupByID(event->oldContainer) : nullptr;
            auto* destinationForm = event->newContainer ? RE::TESForm::LookupByID(event->newContainer) : nullptr;
            auto* sourceReference = InventoryContainerReference(event->oldContainer);
            auto* destinationReference = InventoryContainerReference(event->newContainer);
            auto* sourceActor = sourceForm ? sourceForm->As<RE::Actor>() :
                (sourceReference ? sourceReference->As<RE::Actor>() : nullptr);
            auto* destinationActor = destinationForm ? destinationForm->As<RE::Actor>() : nullptr;
            auto* recentStorageTarget = lost ? RecentActivatedStorageTarget() : nullptr;
            if (recentStorageTarget && event->newContainer != 0 &&
                event->newContainer != recentStorageTarget->GetFormID() &&
                !IsMannequinStorage(recentStorageTarget) &&
                !IsLikelyDisplayStorage(recentStorageTarget)) {
                recentStorageTarget = nullptr;
            }
            if (!recentStorageTarget && lost && IsLikelyDisplayStorage(transferredReference.get())) {
                // Some display/rack transfers report only the activated target
                // in the reference field, with no destination container ID.
                recentStorageTarget = transferredReference.get();
            }
            const bool storedAtActivatedDisplay = recentStorageTarget &&
                (IsLikelyDisplayStorage(recentStorageTarget) ||
                    (ui && ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)));
            const bool storedOnMannequin = (destinationActor && IsMannequinActor(destinationActor)) ||
                (recentStorageTarget && IsMannequinStorage(recentStorageTarget) &&
                    (event->newContainer == 0 || event->newContainer == recentStorageTarget->GetFormID() ||
                        (ui && ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME))));
            auto* barterVendor = InventoryCounterparty(event, gained);
            const bool barterTransfer = barterVendor != nullptr;
            const bool dialogueOpen = ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
            const bool pickpocketSource = gained && !barterTransfer &&
                IsPickpocketInventorySource(player, event->oldContainer);
            const auto dialogueNow = SteadyClockMilliseconds();
            const auto dialogueSpeakerID = g_recentDialogueSpeakerFormID.load(std::memory_order_acquire);
            const auto dialogueSpeakerAt = g_recentDialogueSpeakerAtMs.load(std::memory_order_acquire);
            const bool recentDialogueContext = dialogueSpeakerID && dialogueSpeakerAt > 0 &&
                dialogueNow >= dialogueSpeakerAt && dialogueNow - dialogueSpeakerAt <= 8000;
            auto* recentDialogueActor = recentDialogueContext ?
                RE::TESForm::LookupByID<RE::Actor>(dialogueSpeakerID) : nullptr;
            const bool courierDialogue = recentDialogueActor &&
                (ContainsInsensitive(recentDialogueActor->GetName(), "courier") ||
                    ContainsInsensitive(recentDialogueActor->GetFormEditorID(), "courier"));
            const bool courierItemsContainer =
                (sourceReference && (ContainsInsensitive(sourceReference->GetName(), "courier items") ||
                    ContainsInsensitive(sourceReference->GetFormEditorID(), "courier items"))) ||
                (sourceForm && (ContainsInsensitive(sourceForm->GetName(), "courier items") ||
                    ContainsInsensitive(sourceForm->GetFormEditorID(), "courier items")));
            const bool courierDeliveryContext = gained && !pickpocketSource &&
                (courierDialogue || courierItemsContainer);
            const bool actorInventorySource = sourceActor || (sourceReference && sourceReference->IsActor());
            bool isTheft = pickpocketSource;
            if (courierDeliveryContext) {
                // Courier rewards are added by quest scripts while or just after
                // the courier's dialogue. Some such transfers have no actor
                // container in TESContainerChangedEvent, so ownership checks on
                // the fallback reference can falsely label the reward stolen.
                // Keep genuine pickpocketing above this guard, but never infer
                // theft for a transfer tied to the courier conversation.
                SKSE::log::info("SkyRewrite classified courier reward as received, not theft: speakerFormID={:08X}, speaker='{}', courierItemsContainer={}, itemFormID={:08X}, itemName='{}', quantity={}, oldContainer={:08X}, sourceFormID={:08X}, sourceReference={:08X}, dialogueOpen={}, pickpocketMode=false",
                    recentDialogueActor ? recentDialogueActor->GetFormID() : 0,
                    recentDialogueActor && recentDialogueActor->GetName() ? recentDialogueActor->GetName() : "(no recent courier dialogue)",
                    courierItemsContainer,
                    event->baseObj, item && item->GetName() ? item->GetName() : "(unknown item)",
                    event->itemCount, event->oldContainer,
                    sourceForm ? sourceForm->GetFormID() : 0,
                    sourceReference ? sourceReference->GetFormID() : 0,
                    dialogueOpen);
            } else if (gained && actorInventorySource && !pickpocketSource) {
                // Scripted deliveries (such as courier rewards) can be
                // represented as transfers from a living actor's inventory.
                // Ownership on that actor is not evidence of theft: Skyrim
                // considers actor-origin transfers theft only in pickpocket
                // mode. In particular, do not reuse a recent activation's
                // ownership state for a courier gift.
                SKSE::log::info("SkyRewrite excluded actor-origin inventory transfer from theft classification: sourceFormID={:08X}, sourceName='{}', itemFormID={:08X}, itemName='{}', quantity={}, pickpocketMode=false, barter={}, dialogueOpen={}, oldContainer={:08X}",
                    sourceReference ? sourceReference->GetFormID() : event->oldContainer,
                    sourceActor && sourceActor->GetName() ? sourceActor->GetName() : "(unnamed actor)",
                    event->baseObj, item && item->GetName() ? item->GetName() : "(unknown item)",
                    event->itemCount, barterTransfer, dialogueOpen, event->oldContainer);
            } else if (gained && !barterTransfer && !isTheft && !actorInventorySource &&
                sourceReference && sourceReference->HasContainer()) {
                // Ownership alone is not proof that this transfer was theft:
                // quest scripts can add rewards while the player is near an
                // owned reference. RememberActivatedTheftTarget snapshots
                // Skyrim's steal state at activation time, so require this
                // exact source container to match that captured reference.
                isTheft = MatchesRecentlyActivatedTheftTarget(event, sourceReference);
                if (!isTheft && ReferenceWasStolenFrom(player, sourceReference)) {
                    SKSE::log::info("SkyRewrite ignored owner-only theft candidate without matching activation: itemFormID={:08X}, itemName='{}', sourceFormID={:08X}, sourceName='{}', oldContainer={:08X}, newContainer={:08X}, courierDelivery={}",
                        event->baseObj, item && item->GetName() ? item->GetName() : "(unknown item)",
                        sourceReference->GetFormID(), sourceReference->GetName() ? sourceReference->GetName() : "(unnamed container)",
                        event->oldContainer, event->newContainer, courierDeliveryContext);
                }
            } else if (gained && !barterTransfer && !isTheft && !actorInventorySource &&
                event->oldContainer == 0 && transferredReference && !transferredReference->IsActor()) {
                // A loose, owned world item is theft only when the exact same
                // reference was just activated while Skyrim considered it
                // stolen. Never infer it from another item with the same base.
                isTheft = MatchesRecentlyActivatedTheftTarget(event, transferredReference.get());
            }

            auto* dialogueGiftActor = destinationActor &&
                    (dialogueOpen || destinationActor == recentDialogueActor)
                ? destinationActor
                : (!gained && event->newContainer == 0 && recentDialogueContext ? recentDialogueActor : nullptr);
            const bool ambiguousGoldRemoval = goldItem && lost && !gained && !consumed && !isTheft && !barterTransfer &&
                !dialogueGiftActor && !destinationActor && !destinationReference && !storedAtActivatedDisplay &&
                !storedOnMannequin && event->newContainer == 0;
            if (ambiguousGoldRemoval) {
                SKSE::log::info("SkyRewrite will record ambiguous gold removal without labeling it as a drop: formID={:08X}, name='{}', oldContainer={:08X}, newContainer={:08X}",
                    event->baseObj, item && item->GetName() ? item->GetName() : "Gold", event->oldContainer, event->newContainer);
            }

            auto* theftSource = isTheft ? sourceActor : nullptr;
            if (theftSource && theftSource->IsDead()) theftSource = nullptr;
            const bool pickpocket = isTheft && theftSource && IsPickpocketInventorySource(player, event->oldContainer);

            EventRecord record{};
            record.type = isTheft ? 10 : consumed ? 22 : 4;
            record.gameDay = CurrentGameDay();
            record.formID = event->baseObj;
            if (isTheft && theftSource) {
                record.baseFormID = theftSource->GetFormID();
                CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), theftSource);
            } else if (courierDeliveryContext) {
                auto* courierSource = recentDialogueActor ? static_cast<RE::TESForm*>(recentDialogueActor) :
                    sourceReference ? static_cast<RE::TESForm*>(sourceReference) : sourceForm;
                if (courierSource) {
                    record.baseFormID = courierSource->GetFormID();
                    CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), courierSource);
                }
            }
            CopyFormPlugin(record.formPlugin, sizeof(record.formPlugin), item);
            AddLocation(record, player);
            const auto quantity = event->itemCount < 0 ? -static_cast<long long>(event->itemCount) : static_cast<long long>(event->itemCount);
            if (!consumed && lost && event->newContainer == 0 && isConsumable &&
                IsPairedDropAfterConsumption(event->baseObj, record.locationFormID, quantity)) {
                return RE::BSEventNotifyControl::kContinue;
            }
            if (consumed && IsDuplicateConsumptionNotification(event->baseObj, record.locationFormID, quantity)) {
                return RE::BSEventNotifyControl::kContinue;
            }
            char generatedAlchemyName[192]{};
            const auto* name = ItemDisplayName(item, generatedAlchemyName, sizeof(generatedAlchemyName));
            if (name && *name) CopyText(record.subject, sizeof(record.subject), name);
            else std::snprintf(record.subject, sizeof(record.subject), "Item FormID %08X", event->baseObj);
            record.stage = static_cast<std::uint16_t>(std::min<long long>(quantity, std::numeric_limits<std::uint16_t>::max()));
            if (!isTheft && !consumed) {
                auto* otherReference = barterVendor ? static_cast<RE::TESObjectREFR*>(barterVendor) : gained ? sourceReference : destinationReference;
                if (!otherReference && gained && event->oldContainer == 0) otherReference = transferredReference.get();
                if (!otherReference && storedAtActivatedDisplay) otherReference = recentStorageTarget;
                if (otherReference) {
                    record.baseFormID = otherReference->GetFormID();
                    CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), otherReference);
                }
            }
            if (!gained && dialogueGiftActor) {
                record.baseFormID = dialogueGiftActor->GetFormID();
                CopyFormPlugin(record.basePlugin, sizeof(record.basePlugin), dialogueGiftActor);
            }
            if (isTheft) {
                const auto* sourceName = theftSource ? theftSource->GetName() : nullptr;
                if (!sourceName || !*sourceName) sourceName = theftSource ? theftSource->GetFormEditorID() : nullptr;
                if (pickpocket && sourceName && *sourceName) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/Theft/v1/pickpocket/%.58s", sourceName);
                    std::snprintf(record.detail, sizeof(record.detail), "Pickpocketed %.40s; stole %.40s x%lld (FormID %08X)", sourceName, record.subject, quantity, event->baseObj);
                } else {
                    CopyText(record.identity, sizeof(record.identity), "SkyRewrite/Theft/v1/container");
                    std::snprintf(record.detail, sizeof(record.detail), "Stole %.48s x%lld (FormID %08X)", record.subject, quantity, event->baseObj);
                }
                UpdateRegionalTheftStates(record, player);
            } else if (consumed) {
                const bool isIngredient = ingredientItem != nullptr;
                const bool isFood = isIngredient ? ingredientItem->IsFood() : alchemyItem->IsFood();
                const auto* kind = isIngredient ? "ingredient" : isFood ? "food or drink" : "potion";
                const auto* category = isIngredient ? "ingredient" : isFood ? "food" : "potion";
                std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/Consumption/v1/%s", category);
                std::snprintf(record.detail, sizeof(record.detail), "Consumed %s x%lld (FormID %08X)", kind, quantity, event->baseObj);
            } else {
                char sourceNameBuffer[72]{};
                char destinationNameBuffer[72]{};
                char vendorNameBuffer[72]{};
                char sourceActorNameBuffer[72]{};
                char destinationActorNameBuffer[72]{};
                const auto* sourceName = DisplayNameOrFormID(sourceReference ? static_cast<RE::TESForm*>(sourceReference) : sourceForm,
                    "Source", sourceNameBuffer, sizeof(sourceNameBuffer));
                const auto* destinationName = DisplayNameOrFormID(destinationReference ? static_cast<RE::TESForm*>(destinationReference) : destinationForm,
                    "Destination", destinationNameBuffer, sizeof(destinationNameBuffer));
                const auto* vendorName = DisplayNameOrFormID(barterVendor, "Vendor", vendorNameBuffer, sizeof(vendorNameBuffer));
                const auto* sourceActorName = DisplayNameOrFormID(sourceActor, "NPC", sourceActorNameBuffer, sizeof(sourceActorNameBuffer));
                const auto* destinationActorName = DisplayNameOrFormID(destinationActor, "NPC", destinationActorNameBuffer, sizeof(destinationActorNameBuffer));
                const auto* sourceCellName = CellDisplayName(sourceReference ? sourceReference->GetParentCell() :
                    (transferredReference ? transferredReference->GetParentCell() : nullptr));
                const auto* playerCellName = CellDisplayName(player->GetParentCell());
                if (barterTransfer) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/%s", gained ? "purchased" : "sold");
                    std::snprintf(record.detail, sizeof(record.detail), "%s vendor: %.64s", gained ? "Purchased from" : "Sold to", vendorName ? vendorName : "Unknown vendor");
                } else if (courierDeliveryContext) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/received-gift");
                    std::snprintf(record.detail, sizeof(record.detail), "Received courier delivery from: %.60s",
                        recentDialogueActor && recentDialogueActor->GetName() && *recentDialogueActor->GetName()
                            ? recentDialogueActor->GetName()
                            : sourceReference && sourceReference->GetName() && *sourceReference->GetName()
                                ? sourceReference->GetName() : "Courier");
                } else if (gained && sourceActor && sourceActor->IsDead()) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/looted-body");
                    std::snprintf(record.detail, sizeof(record.detail), "Looted from body: %.68s", sourceName ? sourceName : "Unknown deceased actor");
                } else if (gained && sourceActor && dialogueOpen) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/received-gift");
                    std::snprintf(record.detail, sizeof(record.detail), "Received gift from: %.68s", sourceActorName);
                } else if (gained && sourceActor) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/received-from-npc");
                    std::snprintf(record.detail, sizeof(record.detail), "Received from NPC: %.70s", sourceActorName);
                } else if (gained && sourceReference && sourceReference->HasContainer()) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/looted-container");
                    std::snprintf(record.detail, sizeof(record.detail), "Looted from container: %.64s", sourceName ? sourceName : "Unknown container");
                } else if (gained && event->oldContainer == 0) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/picked-up");
                    std::snprintf(record.detail, sizeof(record.detail), "Picked up at cell: %.70s", sourceCellName ? sourceCellName : playerCellName ? playerCellName : "Unknown cell");
                } else if (gained) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/looted-container");
                    std::snprintf(record.detail, sizeof(record.detail), "Looted from: %.70s", sourceName ? sourceName : "Unknown container");
                } else if (storedOnMannequin) {
                    auto* mannequin = destinationActor && IsMannequinActor(destinationActor) ?
                        static_cast<RE::TESObjectREFR*>(destinationActor) : recentStorageTarget;
                    const auto* mannequinName = DisplayNameOrFormID(mannequin, "Mannequin",
                        destinationNameBuffer, sizeof(destinationNameBuffer));
                    const auto* mannequinCell = mannequin ? CellDisplayName(mannequin->GetParentCell()) : nullptr;
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/stored");
                    std::snprintf(record.detail, sizeof(record.detail), "Stored on mannequin: %.48s at %.40s",
                        mannequinName ? mannequinName : "Mannequin", mannequinCell ? mannequinCell : record.location);
                } else if (destinationActor && destinationActor->IsGuard()) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/confiscated");
                    std::snprintf(record.detail, sizeof(record.detail), "Confiscated by guard: %.66s", destinationActorName);
                } else if (destinationActor && (dialogueOpen || destinationActor == recentDialogueActor)) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/gifted");
                    std::snprintf(record.detail, sizeof(record.detail), "Gifted to NPC: %.72s", destinationActorName);
                } else if (!gained && event->newContainer == 0 && dialogueGiftActor) {
                    char giftActorNameBuffer[72]{};
                    const auto* giftActorName = DisplayNameOrFormID(dialogueGiftActor, "NPC",
                        giftActorNameBuffer, sizeof(giftActorNameBuffer));
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/gifted");
                    std::snprintf(record.detail, sizeof(record.detail), "Gifted to NPC: %.72s", giftActorName);
                } else if (storedAtActivatedDisplay) {
                    char storageName[72]{};
                    const auto* displayName = StorageFixtureName(recentStorageTarget,
                        storageName, sizeof(storageName));
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/stored");
                    std::snprintf(record.detail, sizeof(record.detail), "Stored in: %.48s at %.40s",
                        displayName ? displayName : "Display", record.location);
                } else if (destinationActor) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/transferred-to-actor");
                    std::snprintf(record.detail, sizeof(record.detail), "Transferred to NPC: %.68s", destinationActorName);
                } else if (destinationReference) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/stored");
                    std::snprintf(record.detail, sizeof(record.detail), "Stored in: %.74s", destinationName ? destinationName : "Unknown container or display");
                } else if (ambiguousGoldRemoval) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/removed");
                    std::snprintf(record.detail, sizeof(record.detail), "Gold removed from inventory (reason unavailable)");
                } else if (event->newContainer == 0) {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/dropped");
                    std::snprintf(record.detail, sizeof(record.detail), "Dropped at cell: %.72s", playerCellName ? playerCellName : "Unknown cell");
                } else {
                    std::snprintf(record.identity, sizeof(record.identity), "SkyRewrite/InventoryAction/v1/transfer");
                    std::snprintf(record.detail, sizeof(record.detail), "Transferred %s: %.65s", gained ? "in" : "out", destinationName ? destinationName : sourceName ? sourceName : "item context unavailable");
                }
            }
            if (goldItem) {
                auto* septims = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000FU);
                const auto currentGold = septims ? player->GetItemCount(septims) : -1;
                const std::string_view actionIdentity(record.identity);
                const char* action = isTheft ? "Stole" : "Transferred";
                if (!isTheft && gained) {
                    if (actionIdentity.find("/picked-up") != std::string_view::npos) action = "Picked up";
                    else if (actionIdentity.find("/looted-") != std::string_view::npos) action = "Looted";
                    else if (actionIdentity.find("/purchased") != std::string_view::npos) action = "Received";
                    else action = "Received";
                } else if (!isTheft && lost) {
                    if (ambiguousGoldRemoval) action = "Gold removed";
                    else if (actionIdentity.find("/dropped") != std::string_view::npos) action = "Dropped";
                    else if (barterTransfer) action = "Spent";
                    else action = "Transferred out";
                }
                if (currentGold >= 0) {
                    std::snprintf(record.detail, sizeof(record.detail), "%s %lld gold; current total: %d gold",
                        action, quantity, currentGold);
                } else {
                    std::snprintf(record.detail, sizeof(record.detail), "%s %lld gold", action, quantity);
                }
            }
            AddEvent(record);
            g_sourceEventsCapturedThisSession.fetch_add(1, std::memory_order_relaxed);
            if (goldItem) {
                SKSE::log::info("SkyRewrite recorded gold inventory action: action='{}', detail='{}', quantity={}, formID={:08X}, plugin='{}', location='{}'",
                    record.identity, record.detail, quantity, record.formID, record.formPlugin, record.location);
            }

            // Eating an ingredient updates its discovered-effect bitmask
            // after the inventory callback. Read it on the next game task.
            if (consumed && ingredientItem) ScheduleIngredientEffectCapture(ingredientItem->GetFormID());

            if (isTheft) {
                // When the configurable repeated-theft response is enabled, it
                // owns bounty timing. Applying the ordinary per-crime bounty
                // here as well bypasses the threshold (and dispatches a guard
                // on the first theft).
                if (!g_theftBountyEnabled.load(std::memory_order_relaxed)) {
                    ApplyCrimeBounty(player, record, pickpocket ? "pickpocket" : "theft");
                }
                AttemptTheftBountyResponse(player, record.locationFormID, LookupLocation(record.locationFormID), record.gameDay);
                ApplyMurderTierResponses(player);
                ApplyBanditTradeResponse();
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    SkyRewriteEventSink g_eventSink;
    bool g_sinksInstalled{};
    PRISMA_UI_API::IVPrismaUI1* g_dashboardPrisma{};
    PrismaView g_dashboardView{};

    void SendDashboardScript(const std::string& script)
    {
        if (!g_dashboardPrisma || !g_dashboardPrisma->IsValid(g_dashboardView)) return;
        g_dashboardPrisma->Invoke(g_dashboardView, script.c_str());
    }

    struct GameBountySnapshot {
        std::string holdKey;
        std::string hold;
        std::string formPlugin;
        RE::FormID formID{};
        std::uint32_t violent{};
        std::uint32_t nonViolent{};
    };

    std::vector<GameBountySnapshot> GetActiveGameBounties()
    {
        constexpr std::array<std::string_view, 10> holdKeys{{
            "eastmarch", "falkreath", "haafingar", "hjaalmarch", "pale", "reach", "rift", "whiterun", "winterhold", "solstheim"
        }};
        std::vector<GameBountySnapshot> bounties;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return bounties;
        for (const auto holdKey : holdKeys) {
            auto* faction = CrimeFactionForHoldKey(holdKey);
            if (!faction) continue;
            const auto violent = player->GetViolentCrimeGoldValue(faction);
            const auto nonViolent = player->GetNonViolentCrimeGoldValue(faction);
            if (violent == 0 && nonViolent == 0) continue;
            GameBountySnapshot bounty;
            bounty.holdKey.assign(holdKey);
            bounty.hold = RegionalHoldLabel(holdKey);
            bounty.formID = faction->GetFormID();
            bounty.violent = violent;
            bounty.nonViolent = nonViolent;
            char plugin[64]{};
            CopyFormPlugin(plugin, sizeof(plugin), faction);
            bounty.formPlugin = plugin;
            bounties.push_back(std::move(bounty));
        }
        return bounties;
    }

    std::string GetActiveGameBountiesJSON()
    {
        std::string json = "[";
        const auto bounties = GetActiveGameBounties();
        for (std::size_t i = 0; i < bounties.size(); ++i) {
            if (i) json += ',';
            const auto& bounty = bounties[i];
            json += "{\"holdKey\":\"" + JsonEscape(bounty.holdKey.c_str()) + "\"";
            json += ",\"hold\":\"" + JsonEscape(bounty.hold.c_str()) + "\"";
            json += ",\"formID\":" + std::to_string(bounty.formID);
            json += ",\"formPlugin\":\"" + JsonEscape(bounty.formPlugin.c_str()) + "\"";
            json += ",\"violent\":" + std::to_string(bounty.violent);
            json += ",\"nonViolent\":" + std::to_string(bounty.nonViolent);
            json += ",\"total\":" + std::to_string(static_cast<std::uint64_t>(bounty.violent) + bounty.nonViolent) + "}";
        }
        json += ']';
        return json;
    }

    void SendDashboardGameBounties()
    {
        SendDashboardScript("SkyOS.setSkyRewriteHistory({gameBountiesOnly:" + GetActiveGameBountiesJSON() + "});");
    }

    void NotifyDashboardHistoryChanged()
    {
        if (g_dashboardHistoryNoticeQueued.exchange(true, std::memory_order_acq_rel)) return;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            g_dashboardHistoryNoticeQueued.store(false, std::memory_order_release);
            return;
        }
        tasks->AddTask([] {
            g_dashboardHistoryNoticeQueued.store(false, std::memory_order_release);
            SendDashboardScript("SkyOS.notifySkyRewriteHistoryChanged();");
        });
    }

    void SendDashboardHistory(std::size_t offset = 0, std::size_t limit = 100)
    {
        // Send only one page to the iframe. It asks for the next page when the
        // user presses Load another 100; SkyMissions Actions has its own newest-10 view.
        SendDashboardScript("SkyOS.setSkyRewriteHistory(" + GetSkyRewriteHistoryJSON(limit, offset) + ");");
    }

    void SendDashboardStatus(const char* status)
    {
        SendDashboardScript("SkyOS.setSkyRewriteActionStatus(\"" + JsonEscape(status ? status : "") + "\");");
    }

    void OnDashboardHistoryRequest(const char* request)
    {
        std::size_t offset = 0;
        std::size_t limit = 100;
        if (request && *request) {
            const std::string_view value(request);
            const auto separator = value.find('|');
            const auto offsetText = value.substr(0, separator);
            std::size_t parsedOffset{};
            const auto [offsetEnd, offsetError] = std::from_chars(offsetText.data(), offsetText.data() + offsetText.size(), parsedOffset);
            if (offsetError == std::errc{} && offsetEnd == offsetText.data() + offsetText.size()) offset = parsedOffset;
            if (separator != std::string_view::npos) {
                const auto limitText = value.substr(separator + 1);
                std::size_t parsedLimit{};
                const auto [limitEnd, limitError] = std::from_chars(limitText.data(), limitText.data() + limitText.size(), parsedLimit);
                if (limitError == std::errc{} && limitEnd == limitText.data() + limitText.size()) limit = parsedLimit;
            }
        }
        if (offset >= 2000) return;
        limit = std::clamp<std::size_t>(limit, 1, 100);
        limit = std::min(limit, 2000 - offset);
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        tasks->AddTask([offset, limit] { SendDashboardHistory(offset, limit); });
    }

    void OnDashboardGameBountyRequest(const char*)
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        tasks->AddTask([] { SendDashboardGameBounties(); });
    }

    bool DashboardEnabled(const char* value)
    {
        return value && (std::string_view(value) == "1" || std::string_view(value) == "true");
    }

    void OnDashboardLocalEcho(const char* value) { SetSkyRewriteLocalEchoEnabled(DashboardEnabled(value)); }
    void OnDashboardEchoNotification(const char* value) { SetSkyRewriteLocalEchoNotificationEnabled(DashboardEnabled(value)); }
    void OnDashboardAdaptiveGuard(const char* value) { SetSkyRewriteAdaptiveGuardResponseEnabled(DashboardEnabled(value)); }
    void OnDashboardTemporaryBounty(const char* value) { SetSkyRewriteTemporaryBountyEnabled(DashboardEnabled(value)); }
    void OnDashboardTemporaryBountyPressure(const char* value) { SetSkyRewriteTemporaryBountyPressureScaling(DashboardEnabled(value)); }
    void OnDashboardTheftBounty(const char* value) { SetSkyRewriteTheftBountyEnabled(DashboardEnabled(value)); }
    void OnDashboardDispatchGuard(const char* value) { SetSkyRewriteDispatchGuardOnBounty(DashboardEnabled(value)); }

    void OnDashboardTemporaryBountyAmount(const char* value)
    {
        unsigned int amount{};
        if (value && std::sscanf(value, "%u", &amount) == 1) SetSkyRewriteTemporaryBountyAmount(amount);
    }

    void OnDashboardTemporaryBountyDays(const char* value)
    {
        float days{};
        if (value && std::sscanf(value, "%f", &days) == 1) SetSkyRewriteTemporaryBountyDuration(days);
    }

    void OnDashboardBountyVictimMode(const char* value)
    {
        const std::string_view mode = value ? std::string_view(value) : std::string_view{};
        SetSkyRewriteBountyVictimMode(mode == "hostile" ? 1U : mode == "both" ? 2U : 0U);
    }

    void OnDashboardTheftBountyAmount(const char* value)
    {
        unsigned int amount{};
        if (value && std::sscanf(value, "%u", &amount) == 1) SetSkyRewriteTheftBountyAmount(amount);
    }

    void OnDashboardTheftBountyThreshold(const char* value)
    {
        unsigned int threshold{};
        if (value && std::sscanf(value, "%u", &threshold) == 1) SetSkyRewriteTheftBountyThreshold(threshold);
    }

    void OnDashboardEchoRules(const char* value)
    {
        unsigned int killThreshold{};
        float lookbackDays{}, cooldownDays{};
        if (value && std::sscanf(value, "%u|%f|%f", &killThreshold, &lookbackDays, &cooldownDays) == 3) {
            SetSkyRewriteLocalEchoRules(killThreshold, lookbackDays, cooldownDays);
        }
    }

    void OnDashboardJournalRetention(const char* value)
    {
        unsigned int retention{};
        if (value && std::sscanf(value, "%u", &retention) == 1) SetSkyRewriteJournalRetention(retention);
    }

    void OnDashboardClearHistory(const char*)
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        tasks->AddTask([] {
            ClearSkyRewriteHistory();
            SendDashboardHistory();
            SendDashboardStatus("History cleared in memory. Pending reversible bounty and guard responses remain journaled until safely resolved.");
        });
    }

    void OnDashboardUndoTemporaryBounty(const char* identity)
    {
        if (!identity || !*identity) return;
        const std::string eventIdentity(identity);
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) { SendDashboardStatus("Skyrim task interface is unavailable"); return; }
        tasks->AddTask([eventIdentity] {
            const auto result = UndoSkyRewriteTemporaryBounty(eventIdentity);
            SendDashboardStatus(result.c_str());
            SendDashboardHistory();
        });
    }

    void OnDashboardClearHoldBounty(const char* holdKey)
    {
        if (!holdKey || !*holdKey) return;
        const std::string requestedHold(holdKey);
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) { SendDashboardStatus("Skyrim task interface is unavailable"); return; }
        tasks->AddTask([requestedHold] {
            const auto result = ClearSkyRewriteHoldBounty(requestedHold);
            SendDashboardStatus(result.c_str());
            SendDashboardHistory();
        });
    }
}

bool InitializeSkyRewriteDashboard(PRISMA_UI_API::IVPrismaUI1* prisma, PrismaView view)
{
    if (!prisma || !prisma->IsValid(view)) return false;
    g_dashboardPrisma = prisma;
    g_dashboardView = view;

    prisma->RegisterJSListener(view, "skyos_skyrewrite_history", OnDashboardHistoryRequest);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_game_bounties", OnDashboardGameBountyRequest);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_local_echo", OnDashboardLocalEcho);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_echo_notification", OnDashboardEchoNotification);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_adaptive_guard_response", OnDashboardAdaptiveGuard);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_temporary_bounty", OnDashboardTemporaryBounty);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_temporary_bounty_amount", OnDashboardTemporaryBountyAmount);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_temporary_bounty_days", OnDashboardTemporaryBountyDays);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_temporary_bounty_pressure_scaling", OnDashboardTemporaryBountyPressure);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_bounty_victim_mode", OnDashboardBountyVictimMode);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_theft_bounty", OnDashboardTheftBounty);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_theft_bounty_amount", OnDashboardTheftBountyAmount);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_theft_bounty_threshold", OnDashboardTheftBountyThreshold);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_dispatch_guard_on_bounty", OnDashboardDispatchGuard);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_local_echo_rules", OnDashboardEchoRules);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_journal_retention", OnDashboardJournalRetention);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_clear_history", OnDashboardClearHistory);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_undo_temporary_bounty", OnDashboardUndoTemporaryBounty);
    prisma->RegisterJSListener(view, "skyos_skyrewrite_clear_hold_bounty", OnDashboardClearHoldBounty);
    SKSE::log::info("SkyRewrite registered its dashboard handlers on the SkyOS iframe host");
    return true;
}

void RegisterSkyRewriteSerialization()
{
    const auto* serialization = SKSE::GetSerializationInterface();
    if (!serialization) {
        SKSE::log::error("SkyRewrite could not access SKSE serialization");
        return;
    }
    serialization->SetUniqueID(kPluginID);
    serialization->SetSaveCallback(Save);
    serialization->SetLoadCallback(Load);
    serialization->SetRevertCallback(Revert);
}

void InstallSkyRewriteEventSinks()
{
    if (g_sinksInstalled) return;
    auto* events = RE::ScriptEventSourceHolder::GetSingleton();
    if (!events) {
        SKSE::log::error("SkyRewrite could not access Skyrim's script event sources");
        return;
    }
    if (InstallPlayerItemRemovalReasonHook()) {
        SKSE::log::info("SkyRewrite installed player item-removal reason tracking for accurate drop and consumption records");
    } else {
        SKSE::log::warn("SkyRewrite could not install player item-removal reason tracking; ambiguous alchemy removals will be recorded as inventory actions, not consumption");
    }
    if (InstallDisenchantCallbackHook()) {
        SKSE::log::info("SkyRewrite installed the enchanting-menu disenchantment history hook");
    } else {
        SKSE::log::warn("SkyRewrite could not install the enchanting-menu disenchantment history hook");
    }
    if (InstallAlchemyCraftCallbackHook()) {
        SKSE::log::info("SkyRewrite installed the alchemy-menu crafted potion name capture hook");
    } else {
        SKSE::log::warn("SkyRewrite could not install the alchemy-menu crafted potion name capture hook");
    }
    events->AddEventSink<RE::TESDeathEvent>(&g_eventSink);
    events->AddEventSink<RE::TESEnterBleedoutEvent>(&g_eventSink);
    events->AddEventSink<RE::TESActivateEvent>(&g_eventSink);
    events->AddEventSink<RE::TESQuestStageEvent>(&g_eventSink);
    events->AddEventSink<RE::TESQuestStartStopEvent>(&g_eventSink);
    events->AddEventSink<RE::TESLockChangedEvent>(&g_eventSink);
    events->AddEventSink<RE::TESTopicInfoEvent>(&g_eventSink);
    events->AddEventSink<RE::TESActorLocationChangeEvent>(&g_eventSink);
    events->AddEventSink<RE::TESContainerChangedEvent>(&g_eventSink);
    events->AddEventSink<RE::TESEquipEvent>(&g_eventSink);
    events->AddEventSink<RE::TESLoadGameEvent>(&g_eventSink);
    events->AddEventSink<RE::TESFurnitureEvent>(&g_eventSink);
    if (auto* learnedSpells = RE::SpellsLearned::GetEventSource()) {
        learnedSpells->AddEventSink(&g_eventSink);
        SKSE::log::info("SkyRewrite subscribed to Skyrim's learned-spell events");
    } else {
        SKSE::log::warn("SkyRewrite could not access Skyrim's learned-spell event source");
    }
    if (auto* discoveries = RE::LocationDiscovery::GetEventSource()) {
        discoveries->AddEventSink(&g_eventSink);
        SKSE::log::info("SkyRewrite subscribed to Skyrim's map-marker discovery events");
    } else {
        SKSE::log::warn("SkyRewrite could not access Skyrim's map-marker discovery event source");
    }
    if (auto* booksRead = RE::BooksRead::GetEventSource()) {
        booksRead->AddEventSink(&g_eventSink);
        SKSE::log::info("SkyRewrite subscribed to Skyrim's book-read event source");
    } else {
        SKSE::log::warn("SkyRewrite could not access Skyrim's book-read event source");
    }
    if (auto* skillIncreases = RE::SkillIncrease::GetEventSource()) {
        skillIncreases->AddEventSink(&g_eventSink);
        SKSE::log::info("SkyRewrite subscribed to Skyrim's skill-increase event source");
    } else {
        SKSE::log::warn("SkyRewrite could not access Skyrim's skill-increase event source");
    }
    if (auto* levelIncreases = RE::LevelIncrease::GetEventSource()) {
        levelIncreases->AddEventSink(&g_eventSink);
        SKSE::log::info("SkyRewrite subscribed to Skyrim's player-level event source");
    } else {
        SKSE::log::warn("SkyRewrite could not access Skyrim's player-level event source");
    }
    if (auto* craftedItems = RE::ItemCrafted::GetEventSource()) {
        craftedItems->AddEventSink(&g_eventSink);
    SKSE::log::info("SkyRewrite subscribed to Skyrim's item-crafted event source for smithing, tanning, potion crafting, and enchanting");
    } else {
        SKSE::log::warn("SkyRewrite could not access Skyrim's item-crafted event source");
    }
    if (auto* ui = RE::UI::GetSingleton()) {
        ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_eventSink);
    }
    // kDataLoaded can still expose Skyrim's temporary "Prisoner" player
    // object, whose ActorValueOwner subobject is not initialized yet. The
    // Player-dependent baselines are initialized by TESLoadGameEvent after
    // the real player state is available; progression events in a new game
    // are recorded directly from their event payloads.
    SnapshotKnownIngredientEffects();
    g_sinksInstalled = true;
    SKSE::log::info("SkyRewrite event journal is listening for deaths, player-facing quests, brawl outcomes, lockpicks, dialogue choices, inventory and equipment activity, book reads, skill and level increases, ingredient effects, follower changes, learned spells, shout and perk unlocks, map-marker discoveries, smithing, tanning, potion crafting, enchanting, disenchanting, and location changes");
}

void SetSkyRewriteLocalEchoEnabled(bool enabled)
{
    g_localEchoEnabled.store(enabled, std::memory_order_relaxed);
}

void SetSkyRewriteLocalEchoNotificationEnabled(bool enabled)
{
    g_localEchoNotificationEnabled.store(enabled, std::memory_order_relaxed);
}

void SetSkyRewriteAdaptiveGuardResponseEnabled(bool enabled)
{
    g_adaptiveGuardResponseEnabled.store(enabled, std::memory_order_relaxed);
}

void SetSkyRewriteTemporaryBountyEnabled(bool enabled)
{
    g_temporaryBountyEnabled.store(enabled, std::memory_order_relaxed);
}

void SetSkyRewriteTemporaryBountyAmount(std::uint32_t amount)
{
    g_temporaryBountyAmount.store(std::clamp(amount, 5U, 100U), std::memory_order_relaxed);
}

void SetSkyRewriteTemporaryBountyDuration(float days)
{
    g_temporaryBountyDuration.store(std::clamp(std::isfinite(days) ? days : 1.0F, 0.5F, 7.0F), std::memory_order_relaxed);
}

void SetSkyRewriteTemporaryBountyPressureScaling(bool enabled)
{
    g_temporaryBountyPressureScaling.store(enabled, std::memory_order_relaxed);
}

void SetSkyRewriteBountyVictimMode(std::uint32_t mode)
{
    g_bountyVictimMode.store(std::min(mode, 2U), std::memory_order_relaxed);
}

void SetSkyRewriteTheftBountyEnabled(bool enabled)
{
    g_theftBountyEnabled.store(enabled, std::memory_order_relaxed);
}

void SetSkyRewriteTheftBountyAmount(std::uint32_t amount)
{
    g_theftBountyAmount.store(std::clamp(amount, 5U, 100U), std::memory_order_relaxed);
}

void SetSkyRewriteTheftBountyThreshold(std::uint32_t thefts)
{
    g_theftBountyThreshold.store(std::clamp(thefts, 1U, 100U), std::memory_order_relaxed);
}

void SetSkyRewriteDispatchGuardOnBounty(bool enabled)
{
    g_dispatchGuardOnBounty.store(enabled, std::memory_order_relaxed);
}

void SetSkyRewriteLocalEchoRules(std::uint32_t killThreshold, float lookbackDays, float cooldownDays)
{
    g_localEchoKillThreshold.store(std::clamp(killThreshold, 1U, 20U), std::memory_order_relaxed);
    g_localEchoLookbackDays.store(std::clamp(std::isfinite(lookbackDays) ? lookbackDays : 7.0F, 0.5F, 60.0F), std::memory_order_relaxed);
    g_localEchoCooldownDays.store(std::clamp(std::isfinite(cooldownDays) ? cooldownDays : 7.0F, 0.5F, 60.0F), std::memory_order_relaxed);
}

void SetSkyRewriteJournalRetention(std::uint32_t maxEvents)
{
    const auto retention = std::clamp(maxEvents, 512U, kMaxEvents);
    g_journalRetention.store(retention, std::memory_order_relaxed);
    std::scoped_lock lock(g_eventsMutex);
    while (g_events.size() > retention) {
        auto removable = g_events.end();
        for (auto it = g_events.begin(); it != g_events.end(); ++it) {
            const auto index = static_cast<std::size_t>(it - g_events.begin());
            if (!IsProtectedWorldResponseEvent(index)) { removable = it; break; }
        }
        if (removable == g_events.end()) break;
        g_events.erase(removable);
    }
}

void ClearSkyRewriteHistory()
{
    {
        std::scoped_lock lock(g_eventsMutex);
        std::vector<EventRecord> pending;
        for (std::size_t i = 0; i < g_events.size(); ++i) {
            if ((IsTemporaryBountyApplied(g_events[i]) && !HasBountyResolution(i)) || (IsGuardConfidenceApplied(g_events[i]) && !HasGuardConfidenceResolution(i))) pending.push_back(g_events[i]);
        }
        g_events = std::move(pending);
    }
    g_sourceEventsCapturedThisSession.store(0, std::memory_order_relaxed);
    g_deathEventsSeenThisSession.store(0, std::memory_order_relaxed);
    g_playerKillsCapturedThisSession.store(0, std::memory_order_relaxed);
    ApplyBanditTradeResponse();
}

std::string UndoSkyRewriteTemporaryBounty(const std::string& identity)
{
    EventRecord applied{};
    bool found = false;
    {
        std::scoped_lock lock(g_eventsMutex);
        for (std::size_t i = 0; i < g_events.size(); ++i) {
            if (IsTemporaryBountyApplied(g_events[i]) && identity == g_events[i].identity) {
                if (HasBountyResolution(i)) return "This temporary bounty has already been resolved.";
                applied = g_events[i];
                found = true;
                break;
            }
        }
    }
    if (!found) return "The pending temporary bounty could not be found in this save.";

    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* form = RE::TESForm::LookupByID(applied.formID);
    auto* faction = form && form->Is(RE::TESFaction::FORMTYPE) ? static_cast<RE::TESFaction*>(form) : nullptr;
    const auto amount = static_cast<std::uint32_t>(applied.stage);
    const auto expected = static_cast<std::uint64_t>(applied.baseFormID) + amount;
    const auto current = player && faction ? player->GetNonViolentCrimeGoldValue(faction) : 0U;
    const bool canAttemptUndo = player && faction && amount > 0 && expected <= std::numeric_limits<std::uint32_t>::max() && current == expected;
    if (canAttemptUndo) player->ModCrimeGoldValue(faction, false, -static_cast<std::int32_t>(amount));
    const auto afterUndo = player && faction ? player->GetNonViolentCrimeGoldValue(faction) : current;
    const bool undoVerified = canAttemptUndo && afterUndo == current - amount;

    EventRecord resolution{};
    resolution.type = 7;
    resolution.gameDay = CurrentGameDay();
    resolution.formID = applied.formID;
    resolution.locationFormID = applied.locationFormID;
    resolution.stage = applied.stage;
    CopyText(resolution.formPlugin, sizeof(resolution.formPlugin), applied.formPlugin);
    CopyText(resolution.locationPlugin, sizeof(resolution.locationPlugin), applied.locationPlugin);
    CopyText(resolution.subject, sizeof(resolution.subject), applied.subject);
    CopyText(resolution.location, sizeof(resolution.location), applied.location);
    CopyText(resolution.identity, sizeof(resolution.identity), applied.identity);
    if (undoVerified) {
        std::snprintf(resolution.detail, sizeof(resolution.detail), "Temporary hold bounty manually removed: removed %u septims because the hold balance was unchanged", amount);
    } else if (canAttemptUndo) {
        CopyText(resolution.detail, sizeof(resolution.detail), "Temporary hold bounty manual undo attempted but balance verification failed; no further adjustment was made");
    } else {
        CopyText(resolution.detail, sizeof(resolution.detail), "Temporary hold bounty manual undo skipped; left the balance unchanged because it changed or the hold could not be resolved");
    }
    AddEvent(resolution);
    return undoVerified ? "Temporary hold bounty removed safely." : canAttemptUndo ? "Undo ran, but balance verification failed; see World History." : "Undo skipped to preserve the current hold balance; see World History.";
}

std::string ClearSkyRewriteHoldBounty(const std::string& requestedHoldKey)
{
    const auto holdKey = RegionalHoldKey(requestedHoldKey);
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* faction = CrimeFactionForHoldKey(holdKey);
    if (holdKey.empty() || !player || !faction) {
        return "Could not resolve Skyrim’s crime faction for that hold. No bounty was changed.";
    }

    const auto violentBefore = player->GetViolentCrimeGoldValue(faction);
    const auto nonViolentBefore = player->GetNonViolentCrimeGoldValue(faction);
    player->ClearAllCrimeGold(faction);
    const bool verified = player->GetViolentCrimeGoldValue(faction) == 0 && player->GetNonViolentCrimeGoldValue(faction) == 0;
    if (!verified) return "Skyrim did not confirm that both bounty balances cleared. SkyRewrite left its pending bounty records unresolved.";

    std::vector<EventRecord> pending;
    {
        std::scoped_lock lock(g_eventsMutex);
        for (std::size_t i = 0; i < g_events.size(); ++i) {
            const auto& event = g_events[i];
            if (IsTemporaryBountyApplied(event) && event.formID == faction->GetFormID() && !HasBountyResolution(i)) {
                pending.push_back(event);
            }
        }
    }

    const auto day = CurrentGameDay();
    const auto* holdLabel = RegionalHoldLabel(holdKey);
    const auto totalBefore = static_cast<std::uint64_t>(violentBefore) + nonViolentBefore;
    for (const auto& applied : pending) {
        EventRecord resolution{};
        resolution.type = 7;
        resolution.gameDay = day;
        resolution.formID = applied.formID;
        resolution.locationFormID = applied.locationFormID;
        resolution.stage = applied.stage;
        CopyText(resolution.formPlugin, sizeof(resolution.formPlugin), applied.formPlugin);
        CopyText(resolution.locationPlugin, sizeof(resolution.locationPlugin), applied.locationPlugin);
        CopyText(resolution.subject, sizeof(resolution.subject), applied.subject);
        CopyText(resolution.location, sizeof(resolution.location), applied.location);
        CopyText(resolution.identity, sizeof(resolution.identity), applied.identity);
        std::snprintf(resolution.detail, sizeof(resolution.detail), "Temporary hold bounty manually removed with the hold crime balance; cleared %llu total septims", static_cast<unsigned long long>(totalBefore));
        AddEvent(resolution);
    }

    if (totalBefore > 0) {
        EventRecord cleared{};
        cleared.type = 18;
        cleared.gameDay = day;
        cleared.formID = faction->GetFormID();
        cleared.baseFormID = violentBefore;
        cleared.stage = static_cast<std::uint16_t>(std::min<std::uint32_t>(nonViolentBefore, std::numeric_limits<std::uint16_t>::max()));
        CopyFormPlugin(cleared.formPlugin, sizeof(cleared.formPlugin), faction);
        CopyText(cleared.subject, sizeof(cleared.subject), holdLabel);
        CopyText(cleared.location, sizeof(cleared.location), holdLabel);
        char identity[96]{};
        std::snprintf(identity, sizeof(identity), "SkyRewrite/GameBountyClear/v1/%.*s/%.1f", static_cast<int>(holdKey.size()), holdKey.data(), day);
        CopyText(cleared.identity, sizeof(cleared.identity), identity);
        std::snprintf(cleared.detail, sizeof(cleared.detail), "Cleared %llu septims of Skyrim bounty (%u violent, %u nonviolent)",
            static_cast<unsigned long long>(totalBefore), violentBefore, nonViolentBefore);
        AddEvent(cleared);
    }

    return std::string("Cleared ") + std::to_string(totalBefore) + " septims of Skyrim bounty and resolved " +
        std::to_string(pending.size()) + " pending SkyRewrite bounty record(s) in " + holdLabel + ".";
}

std::string GetSkyRewriteHistoryJSON(std::size_t maxVisibleEvents, std::size_t eventOffset)
{
    std::vector<EventRecord> events;
    {
        std::scoped_lock lock(g_eventsMutex);
        events = g_events;
    }
    const auto retainedPlayerKills = static_cast<std::uint32_t>(std::count_if(events.begin(), events.end(), [](const EventRecord& event) {
        return event.type == 1 && std::string_view(event.detail) == "Player killed this actor";
    }));
    const auto retainedEchoes = static_cast<std::uint32_t>(std::count_if(events.begin(), events.end(), [](const EventRecord& event) {
        return event.type == 5;
    }));
    const auto retainedThefts = static_cast<std::uint32_t>(std::count_if(events.begin(), events.end(), [](const EventRecord& event) {
        return event.type == 10;
    }));
    const auto globalBanditState = std::find_if(events.begin(), events.end(), [](const EventRecord& event) {
        return IsRegionalBanditState(event) && std::string_view(event.identity) == "SkyRewrite/RegionalBanditState/v1/global";
    });
    const auto regionalBanditKills = globalBanditState != events.end()
        ? std::max(static_cast<std::uint32_t>(globalBanditState->stage), static_cast<std::uint32_t>(std::count_if(events.begin(), events.end(), [](const EventRecord& event) {
            return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" && EventMatchesVictimCategory(event, kVictimBandit);
        })))
        : static_cast<std::uint32_t>(std::count_if(events.begin(), events.end(), [](const EventRecord& event) {
            return event.type == 1 && std::string_view(event.detail) == "Player killed this actor" && EventMatchesVictimCategory(event, kVictimBandit);
        }));
    const auto globalCreatureCount = [&](std::string_view category, std::uint16_t flag) {
        const auto identity = RegionalCreatureStateIdentity({}, category);
        const auto saved = std::find_if(events.begin(), events.end(), [&](const EventRecord& event) {
            return IsRegionalCreatureState(event) && std::string_view(event.identity) == identity;
        });
        if (saved != events.end()) return static_cast<std::uint32_t>(saved->stage);
        std::uint32_t total{};
        for (const auto& event : events) {
            if (category == "forgeries") {
                if (event.type == 24) ++total;
                continue;
            }
            if (category == "thefts" || category == "itemsstolen" || category == "goldstolen") {
                if (event.type != 10 || (category == "goldstolen" && event.formID != 0x0000000FU)) continue;
                total += category == "thefts" ? 1U : std::max<std::uint16_t>(1, event.stage);
                continue;
            }
            if (event.type != 1 || std::string_view(event.detail) != "Player killed this actor") continue;
            if (category == "nonhostile") {
                if ((event.stage & kDispositionKnown) && !(event.stage & kVictimHostile) && !(event.stage & kVictimTeammate)) ++total;
                continue;
            }
            if (flag ? EventMatchesVictimCategory(event, flag) : SpecificVictimCategory(event.subject, event.identity) == category) ++total;
        }
        return total;
    };
    const auto regionalDragonKills = globalCreatureCount("dragon", kVictimDragon);
    const auto regionalForswornKills = globalCreatureCount("forsworn", kVictimForsworn);
    const auto regionalVampireKills = globalCreatureCount("vampire", kVictimVampire);
    const auto regionalMonsterKills = globalCreatureCount("monster", kVictimMonster);
    const auto regionalDraugrKills = globalCreatureCount("draugr", 0);
    const auto regionalFalmerKills = globalCreatureCount("falmer", 0);
    const auto regionalDragonPriestKills = globalCreatureCount("dragonpriest", 0);
    const auto regionalSkeletonKills = globalCreatureCount("skeleton", 0);
    const auto regionalHagravenKills = globalCreatureCount("hagraven", 0);
    const auto regionalDwarvenKills = globalCreatureCount("dwarven", 0);
    const auto regionalWolfKills = globalCreatureCount("wolf", kVictimWolf);
    const auto regionalBearKills = globalCreatureCount("bear", kVictimBear);
    const auto regionalSpiderKills = globalCreatureCount("spider", kVictimSpider);
    const auto regionalOtherHostileKills = globalCreatureCount("otherhostile", kVictimOtherHostile);
    const auto regionalAllHostileKills = globalCreatureCount("allhostile", kVictimAllHostile);
    const auto regionalCivilianKills = globalCreatureCount("civilians", kVictimCivilian);
    const auto regionalNonHostileKills = globalCreatureCount("nonhostile", 0);
    const auto regionalMurders = globalCreatureCount("murders", kVictimCivilian);
    const auto regionalThefts = globalCreatureCount("thefts", 0);
    const auto regionalForgeries = globalCreatureCount("forgeries", 0);
    const auto regionalItemsStolen = globalCreatureCount("itemsstolen", 0);
    const auto regionalGoldStolen = globalCreatureCount("goldstolen", 0);
    const auto favorQuestCompletions = static_cast<std::uint32_t>(std::count_if(events.begin(), events.end(), [](const EventRecord& event) {
        return IsReputationState(event) && std::string_view(event.identity).find("SkyRewrite/ReputationFavor/v1/") == 0;
    }));
    const auto reputationGiftState = std::find_if(events.begin(), events.end(), [](const EventRecord& event) {
        return IsReputationState(event) && std::string_view(event.identity) == "SkyRewrite/ReputationGift/v1/last";
    });
    const auto reputationGiftMilestone = reputationGiftState != events.end() ? static_cast<std::uint32_t>(reputationGiftState->stage) : 0U;
    const auto reputationGiftScore = reputationGiftState != events.end()
        ? (reputationGiftState->formID ? static_cast<std::uint32_t>(reputationGiftState->formID) : reputationGiftMilestone * 5U)
        : 0U;
    const auto reputationScore = static_cast<std::int32_t>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(favorQuestCompletions) + regionalBanditKills + regionalForswornKills + regionalVampireKills + regionalMonsterKills -
        regionalThefts - regionalForgeries - static_cast<std::int64_t>(regionalMurders) * 5,
        -500, std::numeric_limits<std::int32_t>::max()));
    auto* goldPlayer = RE::PlayerCharacter::GetSingleton();
    auto* septims = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000FU);
    const bool goldBalanceAvailable = goldPlayer && septims;
    const auto currentGold = goldBalanceAvailable
        ? static_cast<std::uint64_t>(std::max(0, goldPlayer->GetItemCount(septims)))
        : 0U;
    std::uint64_t peakGold = currentGold;
    constexpr std::string_view goldBalanceMarker = "current total: ";
    for (const auto& event : events) {
        if ((event.type != 4 && event.type != 10) || event.formID != 0x0000000FU) continue;
        const std::string_view detail(event.detail);
        const auto marker = detail.find(goldBalanceMarker);
        if (marker == std::string_view::npos) continue;
        auto cursor = marker + goldBalanceMarker.size();
        std::uint64_t balance{};
        bool foundDigit = false;
        while (cursor < detail.size() && detail[cursor] >= '0' && detail[cursor] <= '9') {
            foundDigit = true;
            balance = balance * 10U + static_cast<std::uint64_t>(detail[cursor] - '0');
            ++cursor;
        }
        if (foundDigit) peakGold = std::max(peakGold, balance);
    }
    const auto currentDay = CurrentGameDay();
    char currentDayText[32]{};
    if (std::isfinite(currentDay) && currentDay >= 0.0F) std::snprintf(currentDayText, sizeof(currentDayText), "%.1f", currentDay);
    else std::snprintf(currentDayText, sizeof(currentDayText), "null");
    std::string json = "{\"currentDay\":";
    json += currentDayText;
    json += ",\"recorderInstalled\":";
    json += g_sinksInstalled ? "true" : "false";
    json += ",\"goldBalanceAvailable\":";
    json += goldBalanceAvailable ? "true" : "false";
    json += ",\"currentGold\":" + std::to_string(currentGold);
    json += ",\"peakGold\":" + std::to_string(peakGold);
    json += ",\"sourceEventsThisSession\":" + std::to_string(g_sourceEventsCapturedThisSession.load(std::memory_order_relaxed));
    json += ",\"deathEventsSeenThisSession\":" + std::to_string(g_deathEventsSeenThisSession.load(std::memory_order_relaxed));
    json += ",\"playerKillsCapturedThisSession\":" + std::to_string(g_playerKillsCapturedThisSession.load(std::memory_order_relaxed));
    json += ",\"retainedPlayerKills\":" + std::to_string(retainedPlayerKills);
    json += ",\"retainedEchoes\":" + std::to_string(retainedEchoes);
    json += ",\"retainedThefts\":" + std::to_string(retainedThefts);
    json += ",\"regionalBanditKills\":" + std::to_string(regionalBanditKills);
    json += ",\"regionalBanditStates\":[";
    bool firstRegionalState = true;
    for (const auto& event : events) {
        if (!IsRegionalBanditState(event) || !event.formID) continue;
        if (!firstRegionalState) json += ',';
        firstRegionalState = false;
        char day[32]{};
        if (event.gameDay >= 0.0F) std::snprintf(day, sizeof(day), "%.1f", event.gameDay);
        else std::snprintf(day, sizeof(day), "null");
        json += "{\"holdFactionID\":" + std::to_string(event.formID);
        json += ",\"name\":\"" + JsonEscape(event.subject) + "\"";
        json += ",\"holdKey\":\"" + RegionalHoldKey(event.subject) + "\"";
        json += ",\"kills\":" + std::to_string(event.stage);
        json += ",\"day\":" + std::string(day) + "}";
    }
    json += "]";
    json += ",\"regionalBanditHoldStates\":[";
    bool firstBanditHoldState = true;
    for (const auto& event : events) {
        if (!IsRegionalBanditHoldKeyState(event)) continue;
        if (!firstBanditHoldState) json += ',';
        firstBanditHoldState = false;
        char day[32]{};
        if (event.gameDay >= 0.0F) std::snprintf(day, sizeof(day), "%.1f", event.gameDay);
        else std::snprintf(day, sizeof(day), "null");
        json += "{\"holdKey\":\"" + JsonEscape(event.location) + "\"";
        json += ",\"name\":\"" + JsonEscape(event.subject) + "\"";
        json += ",\"kills\":" + std::to_string(event.stage);
        json += ",\"day\":" + std::string(day) + "}";
    }
    json += "]";
    json += ",\"regionalCreatureStates\":[";
    bool firstCreatureState = true;
    for (const auto& event : events) {
        if (!IsRegionalCreatureState(event)) continue;
        const std::string_view identity(event.identity);
        const auto categorySeparator = identity.rfind('/');
        if (categorySeparator == std::string_view::npos) continue;
        const auto category = identity.substr(categorySeparator + 1);
        if (!firstCreatureState) json += ',';
        firstCreatureState = false;
        char day[32]{};
        if (event.gameDay >= 0.0F) std::snprintf(day, sizeof(day), "%.1f", event.gameDay);
        else std::snprintf(day, sizeof(day), "null");
        json += "{\"holdFactionID\":" + std::to_string(event.formID);
        json += ",\"holdKey\":\"" + JsonEscape(event.location) + "\"";
        json += ",\"name\":\"" + JsonEscape(event.subject) + "\"";
        json += ",\"category\":\"" + JsonEscape(std::string(category).c_str()) + "\"";
        json += ",\"count\":" + std::to_string(event.stage);
        json += ",\"day\":" + std::string(day) + "}";
    }
    json += "]";
    json += ",\"regionalFavorStates\":[";
    constexpr std::array<std::string_view, 10> holdKeys{{
        "eastmarch", "falkreath", "haafingar", "hjaalmarch", "pale", "reach", "rift", "whiterun", "winterhold", "solstheim"
    }};
    bool firstFavorState = true;
    for (const auto holdKey : holdKeys) {
        const auto count = static_cast<std::uint32_t>(std::count_if(events.begin(), events.end(), [&](const EventRecord& event) {
            return IsReputationState(event) && std::string_view(event.identity).find("SkyRewrite/ReputationFavor/v1/") == 0 &&
                RegionalHoldKeyForEvent(event) == holdKey;
        }));
        if (!count) continue;
        if (!firstFavorState) json += ',';
        firstFavorState = false;
        json += "{\"holdKey\":\"" + std::string(holdKey) + "\",\"count\":" + std::to_string(count) + "}";
    }
    json += "]";
    json += ",\"reputation\":{\"score\":" + std::to_string(reputationScore);
    json += ",\"favorQuests\":" + std::to_string(favorQuestCompletions);
    json += ",\"bandits\":" + std::to_string(regionalBanditKills);
    json += ",\"forsworn\":" + std::to_string(regionalForswornKills);
    json += ",\"vampires\":" + std::to_string(regionalVampireKills);
    json += ",\"monsters\":" + std::to_string(regionalMonsterKills);
    json += ",\"dragons\":" + std::to_string(regionalDragonKills);
    json += ",\"draugrs\":" + std::to_string(regionalDraugrKills);
    json += ",\"falmers\":" + std::to_string(regionalFalmerKills);
    json += ",\"dragonPriests\":" + std::to_string(regionalDragonPriestKills);
    json += ",\"skeletons\":" + std::to_string(regionalSkeletonKills);
    json += ",\"hagravens\":" + std::to_string(regionalHagravenKills);
    json += ",\"dwarvenCreatures\":" + std::to_string(regionalDwarvenKills);
    json += ",\"wolves\":" + std::to_string(regionalWolfKills);
    json += ",\"bears\":" + std::to_string(regionalBearKills);
    json += ",\"spiders\":" + std::to_string(regionalSpiderKills);
    json += ",\"otherHostile\":" + std::to_string(regionalOtherHostileKills);
    json += ",\"allHostile\":" + std::to_string(regionalAllHostileKills);
    json += ",\"civilians\":" + std::to_string(regionalCivilianKills);
    json += ",\"nonhostileKills\":" + std::to_string(regionalNonHostileKills);
    json += ",\"murders\":" + std::to_string(regionalMurders);
    json += ",\"thefts\":" + std::to_string(regionalThefts);
    json += ",\"forgeries\":" + std::to_string(regionalForgeries);
    json += ",\"itemsStolen\":" + std::to_string(regionalItemsStolen);
    json += ",\"goldStolen\":" + std::to_string(regionalGoldStolen);
    json += ",\"giftMilestone\":" + std::to_string(reputationGiftMilestone);
    json += ",\"giftScore\":" + std::to_string(reputationGiftScore) + "}";
    json += ",\"gameBounties\":" + GetActiveGameBountiesJSON();
    json += ",\"completedQuests\":[";
    bool firstCompletedQuest = true;
    if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
        for (auto* quest : dataHandler->GetFormArray<RE::TESQuest>()) {
            if (!quest || !quest->IsCompleted() || !IsRelevantJournalQuest(quest)) continue;
            if (!firstCompletedQuest) json += ',';
            firstCompletedQuest = false;
            char questPlugin[64]{};
            CopyFormPlugin(questPlugin, sizeof(questPlugin), quest);
            const auto* questName = quest->GetName();
            if (!questName || !*questName) questName = quest->GetFormEditorID();
            json += "{\"formID\":" + std::to_string(quest->GetFormID());
            json += ",\"name\":\"" + JsonEscape(questName ? questName : "Completed quest") + "\"";
            json += ",\"plugin\":\"" + JsonEscape(questPlugin) + "\"}";
        }
    }
    json += "]";
    std::vector<EventRecord> pageableEvents;
    pageableEvents.reserve(events.size());
    std::vector<bool> pairedConsumableDrops(events.size());
    for (std::size_t index = 0; index + 1 < events.size(); ++index) {
        pairedConsumableDrops[index] = IsPairedConsumableDropRecord(events[index], events[index + 1]);
    }
    for (std::size_t index = events.size(); index > 0; --index) {
        const auto eventIndex = index - 1;
        const auto& event = events[eventIndex];
        if (pairedConsumableDrops[eventIndex]) continue;
        if (!IsInternalWorldState(event) && !IsHiddenGoldInventoryEvent(event)) pageableEvents.push_back(event);
    }
    const auto visibleEventOffset = std::min(eventOffset, pageableEvents.size());
    const auto visibleEventCount = maxVisibleEvents == 0
        ? pageableEvents.size() - visibleEventOffset
        : std::min(pageableEvents.size() - visibleEventOffset, maxVisibleEvents);
    json += ",\"eventOffset\":" + std::to_string(visibleEventOffset);
    json += ",\"eventCount\":" + std::to_string(visibleEventCount);
    json += ",\"eventTotal\":" + std::to_string(pageableEvents.size());
    json += std::string(",\"hasMoreEvents\":") + (visibleEventOffset + visibleEventCount < pageableEvents.size() ? "true" : "false");
    json += ",\"events\":[";
    bool firstVisibleEvent = true;
    for (std::size_t i = 0; i < visibleEventCount; ++i) {
        auto event = pageableEvents[visibleEventOffset + i];
        const std::string_view inventoryIdentity(event.identity);
        const std::string_view inventoryDetail(event.detail);
        if (event.type == 4 && event.baseFormID &&
            (inventoryDetail.find("Unknown NPC") != std::string_view::npos ||
                inventoryDetail.find("Unknown guard") != std::string_view::npos ||
                inventoryDetail.find("Unknown container or display") != std::string_view::npos)) {
            const auto* counterparty = RE::TESForm::LookupByID(event.baseFormID);
            char counterpartyFallback[64]{};
            const auto* counterpartyName = ReferenceDisplayName(counterparty);
            if (!counterpartyName) {
                const char* label = inventoryIdentity == "SkyRewrite/InventoryAction/v1/stored" ? "Container or display" :
                    inventoryIdentity == "SkyRewrite/InventoryAction/v1/confiscated" ? "Guard" :
                    inventoryIdentity == "SkyRewrite/InventoryAction/v1/received-gift" ||
                        inventoryIdentity == "SkyRewrite/InventoryAction/v1/received-from-npc" ||
                        inventoryIdentity == "SkyRewrite/InventoryAction/v1/gifted" ||
                        inventoryIdentity == "SkyRewrite/InventoryAction/v1/transferred-to-actor" ? "NPC" : "Reference";
                std::snprintf(counterpartyFallback, sizeof(counterpartyFallback), "%s FormID %08X", label, event.baseFormID);
                counterpartyName = counterpartyFallback;
            }
            if (counterpartyName && *counterpartyName) {
                if (inventoryIdentity == "SkyRewrite/InventoryAction/v1/received-gift") {
                    std::snprintf(event.detail, sizeof(event.detail), "Received gift from: %.68s", counterpartyName);
                } else if (inventoryIdentity == "SkyRewrite/InventoryAction/v1/received-from-npc") {
                    std::snprintf(event.detail, sizeof(event.detail), "Received from NPC: %.70s", counterpartyName);
                } else if (inventoryIdentity == "SkyRewrite/InventoryAction/v1/gifted") {
                    std::snprintf(event.detail, sizeof(event.detail), "Gifted to NPC: %.72s", counterpartyName);
                } else if (inventoryIdentity == "SkyRewrite/InventoryAction/v1/transferred-to-actor") {
                    std::snprintf(event.detail, sizeof(event.detail), "Transferred to NPC: %.68s", counterpartyName);
                } else if (inventoryIdentity == "SkyRewrite/InventoryAction/v1/confiscated") {
                    std::snprintf(event.detail, sizeof(event.detail), "Confiscated by guard: %.66s", counterpartyName);
                } else if (inventoryIdentity == "SkyRewrite/InventoryAction/v1/stored") {
                    std::snprintf(event.detail, sizeof(event.detail), "Stored in: %.74s", counterpartyName);
                }
            }
        }
        if (!firstVisibleEvent) json += ',';
        firstVisibleEvent = false;
        char day[32]{};
        if (event.gameDay >= 0.0F) std::snprintf(day, sizeof(day), "%.1f", event.gameDay);
        else std::snprintf(day, sizeof(day), "null");
        json += "{\"kind\":\"";
        json += event.type == 1 ? "death" : event.type == 3 ? "location" : event.type == kEventBrawl ? "brawl" : event.type == 25 ? "spell" :
            event.type == 27 ? "shout" : event.type == 28 ? "perk" : event.type == 29 ? "discovery" :
            (event.type == kEventEquipped || event.type == kEventUnequipped) ? "equipment" :
            event.type == kEventSkillIncreased ? "skill" : event.type == kEventLevelIncreased ? "level" :
            event.type == kEventBookRead ? "read" : event.type == kEventIngredientEffectLearned ? "ingredientEffect" :
            event.type == kEventItemDisenchanted ? "disenchantment" :
            (event.type == kEventFollowerStarted || event.type == kEventFollowerStopped) ? "follower" :
            (event.type == kEventItemSmithed || event.type == kEventItemTanned ||
                event.type == kEventPotionCrafted || event.type == kEventItemEnchanted ||
                event.type == kEventCraftMaterialUsed || event.type == kEventItemCooked) ? "crafting" :
            (event.type == 4 || event.type == 10) ? "inventory" :
            event.type == 21 ? "lockpick" : event.type == 22 ? "consumption" : event.type == 23 ? "interaction" : event.type == 24 ? "forgery" :
            ((event.type >= 5 && event.type <= 9) || (event.type >= 11 && event.type <= 13)) ? "echo" : event.type == 15 ? "reputation" :
            event.type == 17 ? "shopTheft" : (event.type == 18 || event.type == 26) ? "bounty" : "quest";
        json += "\",\"day\":" + std::string(day);
        json += ",\"formID\":" + std::to_string(event.formID);
        json += ",\"eventType\":" + std::to_string(event.type);
        if (event.type == 4 || event.type == kEventEquipped || event.type == kEventUnequipped || event.type == kEventBookRead) {
            auto* eventForm = RE::TESForm::LookupByID(event.formID);
            auto* tome = eventForm ? eventForm->As<RE::TESObjectBOOK>() : nullptr;
            const bool spellTome = (tome && tome->TeachesSpell()) || ContainsInsensitive(event.subject, "Spell Tome:");
            if (spellTome) {
                json += ",\"spellTome\":true";
                if (tome && tome->TeachesSpell()) {
                    if (auto* taughtSpell = tome->GetSpell()) {
                        json += ",\"taughtSpellFormID\":" + std::to_string(taughtSpell->GetFormID());
                    }
                }
            }
        }
        if (event.type == 4 || event.type == 10) {
            const auto* inventoryForm = RE::TESForm::LookupByID(event.formID);
            const bool gold = event.formID == 0x0000000FU || IsGoldForm(inventoryForm);
            json += std::string(",\"isGold\":") + (gold ? "true" : "false");
        }
        if (event.type == 19) json += ",\"questCompleted\":true";
        if (event.type == 20) json += ",\"questStarted\":true";
        json += ",\"baseFormID\":" + std::to_string(event.baseFormID);
        json += ",\"formPlugin\":\"" + JsonEscape(event.formPlugin) + "\"";
        json += ",\"basePlugin\":\"" + JsonEscape(event.basePlugin) + "\"";
        json += ",\"locationPlugin\":\"" + JsonEscape(event.locationPlugin) + "\"";
        json += ",\"stage\":" + std::to_string(event.stage);
        if (event.type == 10) {
            json += ",\"isTheft\":true";
            const std::string_view theftIdentity(event.identity);
            constexpr std::string_view pickpocketPrefix = "SkyRewrite/Theft/v1/pickpocket/";
            const bool pickpocket = theftIdentity.find(pickpocketPrefix) == 0;
            json += ",\"theftSourceKind\":\"";
            json += pickpocket ? "pickpocket" : "container";
            json += "\"";
            if (pickpocket) {
                const auto sourceName = theftIdentity.substr(pickpocketPrefix.size());
                json += ",\"sourceName\":\"" + JsonEscape(std::string(sourceName).c_str()) + "\"";
            }
            if (const auto eventHoldKey = RegionalHoldKeyForEvent(event); !eventHoldKey.empty()) {
                json += ",\"holdKey\":\"" + eventHoldKey + "\"";
            }
        }
        if (event.type == 18) {
            json += ",\"holdKey\":\"" + RegionalHoldKey(event.subject) + "\"";
        }
        if (event.type == 26) {
            json += std::string(",\"crimeBountyApplied\":") + (event.stage ? "true" : "false");
        }
        if (event.type == 1 && std::string_view(event.detail) == "Player killed this actor") {
            const auto eventHoldKey = RegionalHoldKeyForEvent(event);
            if (!eventHoldKey.empty()) json += ",\"holdKey\":\"" + eventHoldKey + "\"";
            json += ",\"victimHostile\":";
            json += EventMatchesVictimCategory(event, kVictimAllHostile) ? "true" : ((event.stage & kDispositionKnown) ? "false" : "null");
            json += ",\"victimGuard\":";
            json += (event.stage & (1U << 15)) ? ((event.stage & 2U) ? "true" : "false") : "null";
            json += ",\"victimEssential\":";
            json += (event.stage & (1U << 15)) ? ((event.stage & 4U) ? "true" : "false") : "null";
            json += ",\"victimTeammate\":";
            json += (event.stage & (1U << 15)) ? ((event.stage & 8U) ? "true" : "false") : "null";
            json += ",\"victimCivilian\":";
            json += (event.stage & kDispositionKnown) ? ((event.stage & kVictimCivilian) ? "true" : "false") : "null";
            json += ",\"victimBandit\":";
            json += EventMatchesVictimCategory(event, kVictimBandit) ? "true" : ((event.stage & kDispositionKnown) ? "false" : "null");
            json += ",\"victimDragon\":";
            json += EventMatchesVictimCategory(event, kVictimDragon) ? "true" : ((event.stage & kDispositionKnown) ? "false" : "null");
            json += ",\"victimForsworn\":";
            json += EventMatchesVictimCategory(event, kVictimForsworn) ? "true" : ((event.stage & kDispositionKnown) ? "false" : "null");
            json += ",\"victimVampire\":";
            json += EventMatchesVictimCategory(event, kVictimVampire) ? "true" : ((event.stage & kDispositionKnown) ? "false" : "null");
            json += ",\"victimWolf\":";
            json += EventMatchesVictimCategory(event, kVictimWolf) ? "true" : ((event.stage & kDispositionKnown) ? "false" : "null");
            json += ",\"victimBear\":";
            json += EventMatchesVictimCategory(event, kVictimBear) ? "true" : ((event.stage & kDispositionKnown) ? "false" : "null");
            json += ",\"victimSpider\":";
            json += EventMatchesVictimCategory(event, kVictimSpider) ? "true" : ((event.stage & kDispositionKnown) ? "false" : "null");
            json += ",\"victimMonster\":";
            json += EventMatchesVictimCategory(event, kVictimMonster) ? "true" : ((event.stage & kDispositionKnown) ? "false" : "null");
            json += ",\"victimOtherHostile\":";
            json += EventMatchesVictimCategory(event, kVictimOtherHostile) ? "true" : ((event.stage & kDispositionKnown) ? "false" : "null");
            if (EventMatchesVictimCategory(event, kVictimBandit) || EventMatchesVictimCategory(event, kVictimAllHostile)) {
                if (auto* holdFaction = CrimeFactionForLocation(event.locationFormID)) {
                    json += ",\"holdFactionID\":" + std::to_string(holdFaction->GetFormID());
                    const auto* holdName = holdFaction->GetName();
                    if (!holdName || !*holdName) holdName = holdFaction->GetFormEditorID();
                    json += ",\"holdName\":\"" + JsonEscape(holdName ? holdName : "") + "\"";
                    json += ",\"holdKey\":\"" + RegionalHoldKey(holdName ? holdName : "") + "\"";
                }
            }
        }
        json += ",\"subject\":\"" + JsonEscape(event.subject) + "\"";
        json += ",\"detail\":\"" + JsonEscape(event.detail) + "\"";
        json += ",\"identity\":\"" + JsonEscape(event.identity) + "\"";
        if (event.type == 4) {
            const auto* itemForm = RE::TESForm::LookupByID(event.formID);
            const auto itemCategory = itemForm && itemForm->As<RE::IngredientItem>() ? "ingredient" :
                ContainsInsensitive(event.subject, "pelt") || ContainsInsensitive(event.subject, "hide") ? "peltHide" : "other";
            json += ",\"itemCategory\":\"" + std::string(itemCategory) + "\"";
        }
        json += ",\"location\":\"" + JsonEscape(event.location) + "\"";
        json += ",\"locationFormID\":" + std::to_string(event.locationFormID) + "}";
    }
    json += "]}";
    return json;
}

extern "C" __declspec(dllexport) std::uint32_t __cdecl SkyRewrite_GetAPIVersion()
{
    return SKYREWRITE_API_VERSION;
}

extern "C" __declspec(dllexport) std::uint32_t __cdecl SkyRewrite_GetVisibleEventCount()
{
    std::scoped_lock lock(g_eventsMutex);
    return static_cast<std::uint32_t>(std::count_if(g_events.begin(), g_events.end(), [](const EventRecord& event) {
        return !IsInternalWorldState(event) && !IsHiddenGoldInventoryEvent(event);
    }));
}

extern "C" __declspec(dllexport) std::int32_t __cdecl SkyRewrite_GetVisibleEventAt(std::uint32_t index, SkyRewriteEventV1* eventOut)
{
    if (!eventOut || eventOut->structSize < sizeof(SkyRewriteEventV1)) return 0;
    SkyRewriteEventV1 result{};
    result.structSize = sizeof(result);
    std::scoped_lock lock(g_eventsMutex);
    std::uint32_t visibleIndex = 0;
    for (auto it = g_events.rbegin(); it != g_events.rend(); ++it) {
        if (IsInternalWorldState(*it) || IsHiddenGoldInventoryEvent(*it)) continue;
        if (visibleIndex++ != index) continue;
        result.eventType = it->type;
        result.gameDay = it->gameDay;
        result.formID = it->formID;
        result.locationFormID = it->locationFormID;
        result.baseFormID = it->baseFormID;
        result.stage = it->stage;
        CopyText(result.formPlugin, sizeof(result.formPlugin), it->formPlugin);
        CopyText(result.locationPlugin, sizeof(result.locationPlugin), it->locationPlugin);
        CopyText(result.basePlugin, sizeof(result.basePlugin), it->basePlugin);
        CopyText(result.subject, sizeof(result.subject), it->subject);
        CopyText(result.detail, sizeof(result.detail), it->detail);
        CopyText(result.location, sizeof(result.location), it->location);
        CopyText(result.identity, sizeof(result.identity), it->identity);
        *eventOut = result;
        return 1;
    }
    return 0;
}

extern "C" __declspec(dllexport) std::uint32_t __cdecl SkyRewrite_GetActiveGameBountyCount()
{
    return static_cast<std::uint32_t>(GetActiveGameBounties().size());
}

extern "C" __declspec(dllexport) std::int32_t __cdecl SkyRewrite_GetActiveGameBountyAt(std::uint32_t index, SkyRewriteGameBountyV1* bountyOut)
{
    if (!bountyOut || bountyOut->structSize < sizeof(SkyRewriteGameBountyV1)) return 0;
    const auto bounties = GetActiveGameBounties();
    if (index >= bounties.size()) return 0;
    const auto& bounty = bounties[index];
    SkyRewriteGameBountyV1 result{};
    result.structSize = sizeof(result);
    result.formID = bounty.formID;
    result.violent = bounty.violent;
    result.nonViolent = bounty.nonViolent;
    result.total = static_cast<std::uint32_t>(static_cast<std::uint64_t>(bounty.violent) + bounty.nonViolent);
    CopyText(result.holdKey, sizeof(result.holdKey), bounty.holdKey.c_str());
    CopyText(result.hold, sizeof(result.hold), bounty.hold.c_str());
    CopyText(result.formPlugin, sizeof(result.formPlugin), bounty.formPlugin.c_str());
    *bountyOut = result;
    return 1;
}
}
